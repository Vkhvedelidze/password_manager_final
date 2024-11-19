#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    char website[50];
    char password[50];
} PasswordEntry;

typedef struct {
    char username[50];
    char password[50];
} User;

// Predefined user credentials
User userList[] = {
    {"admin", "password123"},
    {"user1", "mypassword1"}
};
int userCount = 2;

PasswordEntry passwordStore[100];
int passwordCount = 0;

pthread_mutex_t lock;

int authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < userCount; i++) {
        if (strcmp(userList[i].username, username) == 0 &&
            strcmp(userList[i].password, password) == 0) {
            return 1; // Valid credentials
        }
    }
    return 0; // Invalid credentials
}

void *handle_client(void *socket_desc) {
    int new_socket = *(int *)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    char username[50], password[50];

    // Authentication
    send(new_socket, "Username: ", strlen("Username: "), 0);
    memset(buffer, 0, BUFFER_SIZE);
    read(new_socket, buffer, BUFFER_SIZE);
    buffer[strcspn(buffer, "\n")] = 0; // Trim newline
    strncpy(username, buffer, sizeof(username));

    send(new_socket, "Password: ", strlen("Password: "), 0);
    memset(buffer, 0, BUFFER_SIZE);
    read(new_socket, buffer, BUFFER_SIZE);
    buffer[strcspn(buffer, "\n")] = 0; // Trim newline
    strncpy(password, buffer, sizeof(password));

    if (!authenticate_user(username, password)) {
        send(new_socket, "Invalid credentials. Disconnecting.\n", strlen("Invalid credentials. Disconnecting.\n"), 0);
        close(new_socket);
        pthread_exit(NULL);
    }

    send(new_socket, "Authenticated!\n", strlen("Authenticated!\n"), 0);

    // Command loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        send(new_socket, "Enter command (store/retrieve/delete/list/exit): ", strlen("Enter command (store/retrieve/delete/list/exit): "), 0);
        read(new_socket, buffer, BUFFER_SIZE);
        buffer[strcspn(buffer, "\n")] = 0; // Trim newline

        if (strcmp(buffer, "store") == 0) {
            PasswordEntry entry;
            memset(&entry, 0, sizeof(entry));

            send(new_socket, "Website: ", strlen("Website: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE);
            buffer[strcspn(buffer, "\n")] = 0; // Trim newline
            strncpy(entry.website, buffer, sizeof(entry.website));

            send(new_socket, "Password: ", strlen("Password: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE);
            buffer[strcspn(buffer, "\n")] = 0; // Trim newline
            strncpy(entry.password, buffer, sizeof(entry.password));

            pthread_mutex_lock(&lock);
            passwordStore[passwordCount++] = entry;
            pthread_mutex_unlock(&lock);

            send(new_socket, "Password stored successfully!\n", strlen("Password stored successfully!\n"), 0);

        } else if (strcmp(buffer, "retrieve") == 0) {
            char website[50];
            memset(website, 0, sizeof(website));

            send(new_socket, "Website: ", strlen("Website: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE);
            buffer[strcspn(buffer, "\n")] = 0; // Trim newline
            strncpy(website, buffer, sizeof(website));

            pthread_mutex_lock(&lock);
            int found = 0;
            for (int i = 0; i < passwordCount; i++) {
                if (strcmp(passwordStore[i].website, website) == 0) {
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "Password: %s\n", passwordStore[i].password);
                    send(new_socket, response, strlen(response), 0);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            if (!found) {
                send(new_socket, "Website not found.\n", strlen("Website not found.\n"), 0);
            }

        } else if (strcmp(buffer, "delete") == 0) {
            char website[50];
            memset(website, 0, sizeof(website));

            send(new_socket, "Website to delete: ", strlen("Website to delete: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE);
            buffer[strcspn(buffer, "\n")] = 0; // Trim newline
            strncpy(website, buffer, sizeof(website));

            pthread_mutex_lock(&lock);
            int found = 0;
            for (int i = 0; i < passwordCount; i++) {
                if (strcmp(passwordStore[i].website, website) == 0) {
                    for (int j = i; j < passwordCount - 1; j++) {
                        passwordStore[j] = passwordStore[j + 1];
                    }
                    passwordCount--;
                    found = 1;
                    send(new_socket, "Password deleted successfully!\n", strlen("Password deleted successfully!\n"), 0);
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            if (!found) {
                send(new_socket, "Website not found.\n", strlen("Website not found.\n"), 0);
            }

        } else if (strcmp(buffer, "list") == 0) {
            pthread_mutex_lock(&lock);
            if (passwordCount == 0) {
                send(new_socket, "No passwords stored.\n", strlen("No passwords stored.\n"), 0);
            } else {
                for (int i = 0; i < passwordCount; i++) {
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "%d. %s\n", i + 1, passwordStore[i].website);
                    send(new_socket, response, strlen(response), 0);
                }
            }
            pthread_mutex_unlock(&lock);

        } else if (strcmp(buffer, "exit") == 0) {
            send(new_socket, "Goodbye!\n", strlen("Goodbye!\n"), 0);
            break;

        } else {
            send(new_socket, "Invalid command.\n", strlen("Invalid command.\n"), 0);
        }
    }

    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket, *new_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    pthread_mutex_init(&lock, NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d...\n", PORT);

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        pthread_t client_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
        }
    }

    pthread_mutex_destroy(&lock);
    close(server_fd);
    return 0;
}
