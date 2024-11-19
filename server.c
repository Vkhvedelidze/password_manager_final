#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Predefined user credentials
#define USERNAME "admin"
#define PASSWORD "password123"

// Password store
typedef struct {
    char website[50];
    char password[50];
} PasswordEntry;

PasswordEntry passwordStore[100];
int passwordCount = 0;

pthread_mutex_t lock;

// Function to handle each client
void *handle_client(void *socket_desc) {
    int new_socket = *(int *)socket_desc;
    free(socket_desc);
    char buffer[BUFFER_SIZE] = {0};
    char username[50], password[50];

    // Prompt for username
    send(new_socket, "Username: ", strlen("Username: "), 0);
    int bytes_read = read(new_socket, username, sizeof(username));
    if (bytes_read <= 0) {
        close(new_socket);
        pthread_exit(NULL);
    }
    username[strcspn(username, "\n")] = 0; // Remove newline character

    // Prompt for password
    send(new_socket, "Password: ", strlen("Password: "), 0);
    bytes_read = read(new_socket, password, sizeof(password));
    if (bytes_read <= 0) {
        close(new_socket);
        pthread_exit(NULL);
    }
    password[strcspn(password, "\n")] = 0; // Remove newline character

    // Validate credentials
    if (strcmp(username, USERNAME) != 0 || strcmp(password, PASSWORD) != 0) {
        send(new_socket, "Invalid credentials. Disconnecting.\n", strlen("Invalid credentials. Disconnecting.\n"), 0);
        close(new_socket); // Close the socket
        pthread_exit(NULL); // End the thread
    }

    send(new_socket, "Authenticated!\n", strlen("Authenticated!\n"), 0);

    // Command handling loop
    while (1) {
        send(new_socket, "Enter command (store/retrieve/exit): ", strlen("Enter command (store/retrieve/exit): "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(new_socket, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        if (strcmp(buffer, "store") == 0) {
            PasswordEntry entry;
            send(new_socket, "Website: ", strlen("Website: "), 0);
            read(new_socket, entry.website, sizeof(entry.website));
            entry.website[strcspn(entry.website, "\n")] = 0;

            send(new_socket, "Password: ", strlen("Password: "), 0);
            read(new_socket, entry.password, sizeof(entry.password));
            entry.password[strcspn(entry.password, "\n")] = 0;

            pthread_mutex_lock(&lock);
            passwordStore[passwordCount++] = entry;
            pthread_mutex_unlock(&lock);

            send(new_socket, "Password stored successfully!\n", strlen("Password stored successfully!\n"), 0);
        } else if (strcmp(buffer, "retrieve") == 0) {
            char website[50];
            send(new_socket, "Website: ", strlen("Website: "), 0);
            read(new_socket, website, sizeof(website));
            website[strcspn(website, "\n")] = 0;

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
        } else if (strcmp(buffer, "exit") == 0) {
            send(new_socket, "Goodbye!\n", strlen("Goodbye!\n"), 0);
            break;
        } else {
            send(new_socket, "Invalid command.\n", strlen("Invalid command.\n"), 0);
        }
    }

    close(new_socket); // Close client connection
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket, *new_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    pthread_mutex_init(&lock, NULL);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d...\n", PORT);

    // Accept and handle client connections
    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        pthread_t client_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
        }
    }

    if (new_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_destroy(&lock);
    close(server_fd);
    return 0;
}
