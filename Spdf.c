#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>


#define PORT 6680
#define BUFFER_SIZE 1024
#define PDF_DIR "/home/mahmud74/spdf"
#define SERVER_IP "10.60.8.51"

void handle_smain_request(int smain_socket);
void upload_file(int smain_socket, char *filename, char *destination);
void download_file(int smain_socket, char *filename);
void remove_file(int smain_socket, char *filename);
void create_tar_and_send(int smain_socket);
void list_files(int smain_socket, char *path);
void create_directory_recursive(const char *path);
void send_error_message(int smain_socket, const char *message);

int main() {
    int server_socket, smain_socket;
    struct sockaddr_in server_addr, smain_addr;
    socklen_t smain_addr_len = sizeof(smain_addr);

    // Create the base directory for PDF files
    mkdir(PDF_DIR, 0755);

    // Create the server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Spdf server is running...\n");

    while (1) {
        // Accept connection from Smain
        if ((smain_socket = accept(server_socket, (struct sockaddr *)&smain_addr, &smain_addr_len)) < 0) {
            perror("Accept failed");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // Handle the request from Smain in a separate process
        if (!fork()) {
            close(server_socket);
            handle_smain_request(smain_socket);  // Handle the request in the child process
            close(smain_socket);
            exit(0);
        }

        close(smain_socket);
    }

    close(server_socket);
    return 0;
}



void handle_smain_request(int smain_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the command from Smain
    bytes_read = read(smain_socket, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("Smain server disconnected.\n");
        } else {
            perror("Read failed");
        }
        close(smain_socket);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_read] = '\0';

    // // Debugging print
    // printf("Received buffer: '%s'\n", buffer);  // This should print the received command

    // Remove any newline character at the end
    char *cmd_end = strchr(buffer, '\n');
    if (cmd_end) {
        *cmd_end = '\0';
    }

    // Parse the command
    char cmd[BUFFER_SIZE], filename[BUFFER_SIZE] = {0}, destination[BUFFER_SIZE] = {0};
    int parsed_args = sscanf(buffer, "%s %s %s", cmd, filename, destination);

    // Handle the commands
    if (strcmp(cmd, "tar_request_command") == 0) {
        printf("Handling tar_request_command...\n");
        create_tar_and_send(smain_socket);
    } else if (strcmp(cmd, "ufile") == 0 && parsed_args == 3) {
        printf("Handling ufile command...\n");
        upload_file(smain_socket, filename, destination);
    } else if (strcmp(cmd, "dfile") == 0 && parsed_args == 2) {
        printf("Handling dfile command...\n");
        download_file(smain_socket, filename);
    } else if (strcmp(cmd, "rmfile") == 0 && parsed_args == 2) {
        printf("Handling rmfile command...\n");
        remove_file(smain_socket, filename);
    } else if (strcmp(cmd, "display") == 0 && parsed_args == 2) {
        printf("Handling display command for path: %s\n", filename);  // Debug print
        list_files(smain_socket, filename);
    } else {
        printf("Received unknown command: %s\n", cmd);
        send_error_message(smain_socket, "Unknown command or incorrect arguments.\n");
    }

    close(smain_socket);
}










void upload_file(int smain_socket, char *filename, char *destination) {
    char buffer[BUFFER_SIZE], file_path[BUFFER_SIZE];
    ssize_t bytes_read;
    int file_fd;

    // Construct the full file path
    snprintf(file_path, BUFFER_SIZE, "%s/%s", destination, filename);

    // Ensure the directory path is created properly
    create_directory_recursive(destination);

    //DEBUG
    // printf("Im in the first stage of upload_file");
    // Open the file to write the incoming data
    file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("File open failed");
        send_error_message(smain_socket, "Failed to open file for writing.\n");
        return;
    }

    // Start receiving the file content
    while ((bytes_read = read(smain_socket, buffer, BUFFER_SIZE)) > 0) {
        if (write(file_fd, buffer, bytes_read) != bytes_read) {
            perror("File write failed");
            send_error_message(smain_socket, "Failed to write to file.\n");
            close(file_fd);
            return;
        }
    }

    if (bytes_read == 0) {
        // Client closed the connection, file transfer is complete
        printf("File transfer complete.\n");
    } else if (bytes_read < 0) {
        perror("Read error");
        send_error_message(smain_socket, "Error reading file content.\n");
    }

    close(file_fd);

    // Send success message to client
    const char *success_message = "File uploaded successfully.\n";
    // printf("Sending success message to client: %s\n", success_message);  // Debugging print
    send(smain_socket, success_message, strlen(success_message), 0);
    
    // Properly shutdown and close the socket
    shutdown(smain_socket, SHUT_RDWR);
    close(smain_socket);

    printf("Connection closed after file upload.\n");
}





void download_file(int smain_socket, char *filename) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int file_fd;

    printf("Attempting to open file: %s\n", filename);

    // Use the filename directly since it contains the full path
    if ((file_fd = open(filename, O_RDONLY)) < 0) {
        perror("File open failed");
        send_error_message(smain_socket, "File not found or could not be opened.\n");
        return;
    }

    // Check if the file is empty
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size == 0) {
        // printf("File is empty, sending empty response.\n");

        // Send an explicit empty response to the client
        send(smain_socket, "", 0, 0);

        const char *success_message = "File uploaded successfully.\n";
        // printf("Sending success message to client: %s\n", success_message);  // Debugging print
        send(smain_socket, success_message, strlen(success_message), 0);

        close(file_fd);
        shutdown(smain_socket, SHUT_WR);
        close(smain_socket);  // Ensure the socket is properly closed
        printf("File transfer complete and connection closed.\n");
        return;
    }

    // Reset the file pointer to the beginning
    lseek(file_fd, 0, SEEK_SET);

    // Start reading and sending the file content
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(smain_socket, buffer, bytes_read) != bytes_read) {
            perror("File send failed");
            close(file_fd);
            return;
        }
    }

    if (bytes_read < 0) {
        perror("File read failed");
    }

    close(file_fd);
    shutdown(smain_socket, SHUT_WR);
    close(smain_socket);  // Ensure the socket is properly closed
    printf("File transfer complete and connection closed.\n");
}






void remove_file(int smain_socket, char *filename) {
    char file_path[BUFFER_SIZE];
    snprintf(file_path, BUFFER_SIZE, "%s", filename);

    if (unlink(file_path) != 0) {
        perror("File deletion failed");
        send_error_message(smain_socket, "Failed to delete file.\n");
    } else {
        printf("File deleted: %s\n", filename);
        const char *success_message = "File deleted successfully.\n";
        send(smain_socket, success_message, strlen(success_message), 0);
    }
}






void create_tar_and_send(int smain_socket) {
    char tar_file[BUFFER_SIZE], command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int file_fd;

    // printf("Creating tar file...\n");  // Debugging print

    // Create a unique tar file name to avoid conflicts
    snprintf(tar_file, BUFFER_SIZE, "/tmp/pdf_files_%d.tar", getpid());

    // Command to find all .pdf files in PDF_DIR and create a tarball
    snprintf(command, BUFFER_SIZE, "find %s -type f -name '*.pdf' -print0 | tar --null -cvf %s --files-from - 2>/dev/null", PDF_DIR, tar_file);

    // Execute the command to create the tarball
    // printf("Executing command: %s\n", command);  // Debugging print
    if (system(command) != 0) {
        send_error_message(smain_socket, "Failed to create tar file.\n");
        return;
    }

    // Open the created tar file to send it over the socket
    if ((file_fd = open(tar_file, O_RDONLY)) < 0) {
        perror("Tar file open failed");
        send_error_message(smain_socket, "Failed to open tar file.\n");
        return;
    }

    // printf("Sending tar file to Smain...\n");  // Debugging print
    // Send the tar file content to Smain
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(smain_socket, buffer, bytes_read) != bytes_read) {
            perror("Tar file send failed");
            close(file_fd);
            unlink(tar_file);
            return;
        }
    }

    // Close and clean up
    close(file_fd);
    unlink(tar_file);

    // Confirm successful transfer
    const char *success_message = "Tar file created and sent successfully.\n";
    send(smain_socket, success_message, strlen(success_message), 0);
    printf("Tar file sent successfully.\n");  // Debugging print
}





void list_files(int smain_socket, char *path) {
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    FILE *fp;

    // If the path starts with "/home/mahmud74/smain", replace it with "/home/mahmud74/spdf"
    if (strncmp(path, "/home/mahmud74/smain", 20) == 0) {
        snprintf(command, BUFFER_SIZE, "find %s%s -type f -name '*.pdf'", PDF_DIR, path + 20);
    } else {
        // Otherwise, treat it as a relative path under PDF_DIR
        snprintf(command, BUFFER_SIZE, "find %s/%s -type f -name '*.pdf'", PDF_DIR, path);
    }

    // printf("Executing command: %s\n", command);  // Debugging print

    if ((fp = popen(command, "r")) == NULL) {
        perror("popen failed");
        send_error_message(smain_socket, "Failed to list files.\n");
        return;
    }

    // Send only the filenames back to Smain
    while (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
        // Remove any trailing newline character from the file path
        buffer[strcspn(buffer, "\n")] = 0;

        // Extract the base filename
        char *filename = basename(buffer);

        // Send the filename back to Smain
        send(smain_socket, filename, strlen(filename), 0);
        send(smain_socket, "\n", 1, 0);  // Send newline to separate filenames
    }
    pclose(fp);

    // Indicate the end of the list
    const char *end_message = "EOF";
    send(smain_socket, end_message, strlen(end_message), 0);
}









void create_directory_recursive(const char *path) {
    char temp[BUFFER_SIZE];
    char *ptr = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);

    if (len > 1 && temp[len - 1] == '/') {
        temp[len - 1] = 0;
    }

    for (ptr = temp + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = 0;
            mkdir(temp, 0755);
            *ptr = '/';
        }
    }
    mkdir(temp, 0755);
}

void send_error_message(int smain_socket, const char *message) {
    write(smain_socket, message, strlen(message));
}




// /*              __
//                / _)         
//         .-^^^-/ /          
//     __/       /              
//     <__.|_|-|_|              
// */

// ++++ Contributors ++++
// - 1 ----
// Md. Abu Hasib mahmud
// mahmud74@uwindsor.ca
// - 2 ----
// Farhat Lamia Elma
// elmaf@uwindsor.ca