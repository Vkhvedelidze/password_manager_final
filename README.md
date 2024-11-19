
# Password Manager: Client-Server Setup

This is a simple password manager application implemented using a client-server architecture in C.

---

## How to Run the Program

### 1. Compile the Server
Run the following command to compile the server:
```bash
gcc password_manager_server.c -o server -lpthread
```

### 2. Compile the Client
Run the following command to compile the client:
```bash
gcc password_manager_client.c -o client
```

### 3. Start the Server
Start the server using:
```bash
./server
```

### 4. Start the Client
Start the client using:
```bash
./client
```

---

## Interacting with the Program

1. **Authenticate**:
   - Enter the username and password when prompted.
   - The implemented username and password are:
     ```plaintext
     Username: admin
     Password: password123
     ```

2. **Commands**:
   - Once authenticated, you can issue the following commands:
     - **`store`**: Store a new password.
     - **`retrieve`**: Retrieve a stored password.
     - **`exit`**: Exit the session.

---