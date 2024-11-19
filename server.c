#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <openssl/sha.h>

#define PORT 8080
#define BUFFER_SIZE 1024

pthread_mutex_t lock;

typedef struct {
    char username[50];
    char password_hash[65]; // SHA-256 hash length
} User;

// Hash a string using SHA-256
void hash_password(const char *password, char *hash_out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[64] = '\0';
}

// Authenticate user from file
int authenticate_user(const char *username, const char *password) {
    FILE *file = fopen("users.txt", "r");
    if (!file) {
        perror("Failed to open user file");
        return 0;
    }
    char stored_user[50], stored_hash[65], password_hash[65];
    hash_password(password, password_hash);
    while (fscanf(file, "%s %s", stored_user, stored_hash) != EOF) {
        if (strcmp(username, stored_user) == 0 && strcmp(password_hash, stored_hash) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

// Register a new user
int register_user(const char *username, const char *password) {
    FILE *file = fopen("users.txt", "a");
    if (!file) {
        perror("Failed to open user file for writing");
        return 0;
    }
    char password_hash[65];
    hash_password(password, password_hash);
    fprintf(file, "%s %s\n", username, password_hash);
    fclose(file);
    return 1;
}

// Save password entry to file
void save_password(const char *website, const char *password) {
    FILE *file = fopen("passwords.txt", "a");
    if (!file) {
        perror("Failed to open passwords file");
        return;
    }
    fprintf(file, "%s %s\n", website, password);
    fclose(file);
}

// Retrieve password by website
void retrieve_password(const char *website, char *response) {
    FILE *file = fopen("passwords.txt", "r");
    if (!file) {
        perror("Failed to open passwords file");
        strcpy(response, "Error opening file.\n");
        return;
    }
    char stored_website[50], stored_password[50];
    while (fscanf(file, "%s %s", stored_website, stored_password) != EOF) {
        if (strcmp(website, stored_website) == 0) {
            snprintf(response, BUFFER_SIZE, "Password: %s\n", stored_password);
            fclose(file);
            return;
        }
    }
    strcpy(response, "Website not found.\n");
    fclose(file);
}

// Handle client connection
void *handle_client(void *socket_desc) {
    int new_socket = *(int *)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE], username[50], password[50];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    // Authentication
    send(new_socket, "1. Login\n2. Register\nEnter choice: ", strlen("1. Login\n2. Register\nEnter choice: "), 0);
    memset(buffer, 0, BUFFER_SIZE);
    read(new_socket, buffer, BUFFER_SIZE - 1);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strcmp(buffer, "1") == 0) { // Login
        send(new_socket, "Username: ", strlen("Username: "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read > sizeof(username) - 1) bytes_read = sizeof(username) - 1;
        buffer[bytes_read] = '\0';
        strncpy(username, buffer, sizeof(username) - 1);

        send(new_socket, "Password: ", strlen("Password: "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read > sizeof(password) - 1) bytes_read = sizeof(password) - 1;
        buffer[bytes_read] = '\0';
        strncpy(password, buffer, sizeof(password) - 1);

        if (!authenticate_user(username, password)) {
            send(new_socket, "Invalid credentials. Disconnecting.\n", strlen("Invalid credentials. Disconnecting.\n"), 0);
            close(new_socket);
            pthread_exit(NULL);
        }
        send(new_socket, "Login successful!\n", strlen("Login successful!\n"), 0);

    } else if (strcmp(buffer, "2") == 0) { // Register
        send(new_socket, "Username: ", strlen("Username: "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read > sizeof(username) - 1) bytes_read = sizeof(username) - 1;
        buffer[bytes_read] = '\0';
        strncpy(username, buffer, sizeof(username) - 1);

        send(new_socket, "Password: ", strlen("Password: "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read > sizeof(password) - 1) bytes_read = sizeof(password) - 1;
        buffer[bytes_read] = '\0';
        strncpy(password, buffer, sizeof(password) - 1);

        if (register_user(username, password)) {
            send(new_socket, "Registration successful!\n", strlen("Registration successful!\n"), 0);
        } else {
            send(new_socket, "Registration failed. Try again.\n", strlen("Registration failed. Try again.\n"), 0);
            close(new_socket);
            pthread_exit(NULL);
        }
    }

    // Command loop
    while (1) {
        send(new_socket, "Commands: store, retrieve, exit\nEnter command: ", strlen("Commands: store, retrieve, exit\nEnter command: "), 0);
        memset(buffer, 0, BUFFER_SIZE);
        read(new_socket, buffer, BUFFER_SIZE - 1);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "store") == 0) {
            send(new_socket, "Website: ", strlen("Website: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
            char website[50];
            strncpy(website, buffer, sizeof(website) - 1);

            send(new_socket, "Password: ", strlen("Password: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
            char password[50];
            strncpy(password, buffer, sizeof(password) - 1);

            pthread_mutex_lock(&lock);
            save_password(website, password);
            pthread_mutex_unlock(&lock);

            send(new_socket, "Password stored successfully!\n", strlen("Password stored successfully!\n"), 0);

        } else if (strcmp(buffer, "retrieve") == 0) {
            send(new_socket, "Website: ", strlen("Website: "), 0);
            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE - 1);

            char response[BUFFER_SIZE];
            pthread_mutex_lock(&lock);
            retrieve_password(buffer, response);
            pthread_mutex_unlock(&lock);

            send(new_socket, response, strlen(response), 0);

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

    if (listen(server_fd, 3) < 0) {
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
