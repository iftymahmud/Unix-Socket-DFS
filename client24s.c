#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define SM_PORT 9980
#define SM_IP "10.60.8.51"

int connect_to_server();
void handle_ufile(int sockfd, char *filename, char *destination_path);
void handle_dfile(int sockfd, char *filepath);
void handle_rmfile(int sockfd, char *filepath);
void handle_dtar(int sockfd, char *filetype);
void handle_display(int sockfd, char *pathname);
int validate_filetype(const char *filename, const char *expected_extension);
void receive_server_response(int sockfd);
void print_help();
void expand_tilde(char *path);

int main() {
    printf("---------------------------------------------\n");
    printf("Type 'help' to see the usage, 'exit' to quit.\n");
    printf("---------------------------------------------\n");

    char input[BUFSIZE];
   

    while (1) {
         int sockfd = connect_to_server();

        // Prompt user for input
        printf("client24s$ ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets failed");
            break;
        }

        // Remove trailing newline character
        input[strcspn(input, "\n")] = 0;

        // Split the input into command and arguments
        char *command = strtok(input, " ");
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");


        //Properly formatting Argument 1 for ufile
        char temp1[1024];
        if (arg1 != NULL) {
            strcpy(temp1, arg1);
        } else {
            temp1[0] = '\0';
        }

        //Properly formatting Argument 2 for ufile
        char temp2[1024];
        if (arg2 != NULL) {
            strcpy(temp2, arg2);
        } else {
            temp2[0] = '\0';
        }
        
        


        // printf("Command: %s\n", command);
        // printf("Argument 1: %s\n", arg1);
        // printf("Argument 2: %s\n", arg2);
        

        if (command == NULL) {
            continue;  // Empty input, prompt again
        }

        if (strcmp(command, "ufile") == 0) {
            if (arg1 == NULL || arg2 == NULL) {
                fprintf(stderr, "Usage: ufile <filename> <destination_path>\n");
            } else {
                expand_tilde(temp2);
                handle_ufile(sockfd, temp1, temp2);
            }
        } else if (strcmp(command, "dfile") == 0) {
            if (arg1 == NULL) {
                fprintf(stderr, "Usage: dfile <filepath>\n");
            } else {
                expand_tilde(arg1);
                handle_dfile(sockfd, arg1);
            }
        } else if (strcmp(command, "rmfile") == 0) {
            if (arg1 == NULL) {
                fprintf(stderr, "Usage: rmfile <filepath>\n");
            } else {
                expand_tilde(arg1);
                handle_rmfile(sockfd, arg1);
            }
        } else if (strcmp(command, "dtar") == 0) {
            if (arg1 == NULL) {
                fprintf(stderr, "Usage: dtar <filetype>\n");
            } else {
                expand_tilde(arg1);
                handle_dtar(sockfd, arg1);
            }
        } else if (strcmp(command, "display") == 0) {
            if (arg1 == NULL) {
                fprintf(stderr, "Usage: display <pathname>\n");
            } else {
                expand_tilde(arg1);
                handle_display(sockfd, arg1);
            }
        } else if (strcmp(command, "help") == 0) {
            print_help();
        } else if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");
            break;
        } else {
            fprintf(stderr, "Invalid command: %s\n", command);
        }
        close(sockfd);
    }

    
    return 0;
}

// Function to print the usage instructions
void print_help() {
    printf("Available commands:\n");
    printf("  ufile <filename> <destination_path> - Upload a file to the server\n");
    printf("  dfile <filepath> - Download a file from the server\n");
    printf("  rmfile <filepath> - Remove a file from the server\n");
    printf("  dtar <filetype> - Download a tar file of a specific type (.c, .pdf, .txt)\n");
    printf("  display <pathname> - Display contents of a file or directory on the server\n");
    printf("  help - Show this help message\n");
    printf("  exit - Exit the program\n");
}

// Function to expand ~ to the user's home directory
void expand_tilde(char *path) {
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            char expanded_path[BUFSIZE];
            snprintf(expanded_path, sizeof(expanded_path), "%s%s", home, path + 1);
            // Ensure null-termination
            strncpy(path, expanded_path, BUFSIZE - 1);
            path[BUFSIZE - 1] = '\0';
        }
    }
}














int connect_to_server() {
    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SM_PORT);

    if (inet_pton(AF_INET, SM_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void handle_ufile(int sockfd, char *filename, char *destination_path) {

    //DEBUG
    // printf("%s\n",filename);
    //DEBUG
    // printf("%s\n", destination_path);
    

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("File open failed");
        return;
    }

    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "ufile %s %s\n", filename, destination_path);
    send(sockfd, buffer, strlen(buffer), 0);

    // Introduce a small delay to ensure the server is ready to receive the file data
    usleep(100000); // Sleep for 100ms

    size_t n;
    while ((n = fread(buffer, 1, BUFSIZE, file)) > 0) {
        if (send(sockfd, buffer, n, 0) == -1) {
            perror("File send failed");
            fclose(file);
            return;
        }
    }

    fclose(file);
    shutdown(sockfd, SHUT_WR);  // Indicate that the file transfer is complete

    // Receive success message from the server
    receive_server_response(sockfd);

    // Properly close the socket on the client side
    // close(sockfd);
}






void handle_dfile(int sockfd, char *filepath) {
    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "dfile %s", filepath);
    send(sockfd, buffer, strlen(buffer), 0);

    // Determine the filename to save on the client side
    char *filename = strrchr(filepath, '/');
    if (filename) {
        filename++;  // Skip the '/'
    } else {
        filename = filepath;  // No '/' found, use the whole path
    }

    // printf("Saving file as: %s\n", filename);

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("File open failed on client side");
        return;
    }

    ssize_t n;
    while ((n = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
        // printf("Received %zd bytes from server.\n", n);
        if (fwrite(buffer, 1, n, file) != n) {
            perror("File write failed on client side");
            fclose(file);
            return;
        }
    }

    // if (n < 0) {
    //     perror("Receive failed");
    // } else if (n == 0) {
    //     printf("File download complete.\n");
    // }

    fclose(file);

    // Open the file again to check its content
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("File open failed for checking : Client side");
        return;
    }

    int found = 0;
    while (fgets(buffer, BUFSIZE, file) != NULL) {
        if (strstr(buffer, "File not found or could not be opened.") != NULL) {
            found = 1;
            break;
        }
    }
    fclose(file);

    int flagd = 0;
    if (found) {
        if (remove(filename) != 0) {
            perror("Failed to delete file : Client side");
        } else {
            flagd = 1;
            perror("Receive failed : File Doesn't Exist");
        }
    }

    if (flagd==0) {
        printf("File download complete.\n");
    }
    // Properly close the socket on the client side
    shutdown(sockfd, SHUT_WR);

    // // Receive success message from the server
    // receive_server_response(sockfd);

    // // close(sockfd);
}






void handle_rmfile(int sockfd, char *filepath) {
    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "rmfile %s", filepath);
    send(sockfd, buffer, strlen(buffer), 0);

    // Receive server response
    receive_server_response(sockfd);
}

void handle_dtar(int sockfd, char *filetype) {
    if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0) {
        fprintf(stderr, "Error: Invalid filetype. Use .c, .pdf, or .txt.\n");
        return;
    }

    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "dtar %s", filetype);
    send(sockfd, buffer, strlen(buffer), 0);

    snprintf(buffer, sizeof(buffer), "%sfiles.tar", filetype + 1); // Generates cfiles.tar, pdffiles.tar, txtfiles.tar
    FILE *file = fopen(buffer, "wb");
    if (file == NULL) {
        perror("File open failed");
        return;
    }

    ssize_t n;
    while ((n = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
        // Check if the data received is the success message
        if (strstr(buffer, "Tar file created and sent successfully.") != NULL) {
            printf("Server Response: %s\n", buffer);
            break;
        }

        if (fwrite(buffer, 1, n, file) != n) {
            perror("File write failed");
            fclose(file);
            return;
        }
    }
    fclose(file);
    
    // If no success message was received in the loop, try receiving it explicitly
    if (n == 0 || (n > 0 && strstr(buffer, "Tar file created and sent successfully.") == NULL)) {
        receive_server_response(sockfd);
    }
}


void handle_display(int sockfd, char *pathname) {
    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "display %s", pathname);
    send(sockfd, buffer, strlen(buffer), 0);

    ssize_t n;
    while ((n = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
        buffer[n] = '\0';
        if (strstr(buffer, "EOF") != NULL) {
            break;  // End of file list
        }
        fwrite(buffer, 1, n, stdout);
    }

    // Receive server response (if there's any)
    // receive_server_response(sockfd);
}





int validate_filetype(const char *filename, const char *expected_extension) {
    const char *ext = strrchr(filename, '.');
    return (ext != NULL && strcmp(ext, expected_extension) == 0);
}

void receive_server_response(int sockfd) {
    char response[BUFSIZE];
    ssize_t n = recv(sockfd, response, BUFSIZE - 1, 0);
    if (n > 0) {
        response[n] = '\0';
        printf("Server Response: %s\n", response);
    } else {
        printf("No response received from server or connection closed.\n");
    }
}
