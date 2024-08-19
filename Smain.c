#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>


// #define PORT 8080
#define PORT 9980
#define BUFFER_SIZE 1024
#define SM_DIR "smain"
#define HH "/home/mahmud74"
#define SERVER_IP "10.60.8.51"

#define SPDF_IP "10.60.8.51"  // IP address of Spdf server
#define STEXT_IP "10.60.8.51" // IP address of Stext server
#define SPDF_PORT 6680  // PORT of Spdf server
#define STEXT_PORT 7780 // PORT of Stext server


void prcclient(int client_socket);
void process_command(int client_socket, char *command);
void upload_file(int client_socket, char *filename, char *destination);
void download_file(int client_socket, char *filename);
void remove_file(int client_socket, char *filename);
void create_tar_and_send(int client_socket, char *filetype);
void list_files(int client_socket, char *path);
void communicate_with_server(int client_socket, char *server_ip, char *command, int new_port);
void create_directory_recursive(const char *path);
int validate_filename(const char *filename);
void send_error_message(int client_socket, const char *message);
void create_base_directories();

void main_server();

int main() {
    main_server();
    return 0;
}

void main_server() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    create_base_directories();  // Ensure base directories exist

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Smain server is running...\n");

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Accept failed");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        if (!fork()) {
            close(server_socket);
            prcclient(client_socket);  // Handle the client in the child process
            close(client_socket);
            exit(0);
        }

        close(client_socket);
        waitpid(-1, NULL, WNOHANG);  // Clean up zombie processes
    }

    close(server_socket);
}



void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        bytes_read = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("Read failed");
            }
            break;  // Exit the loop when the client disconnects or an error occurs
        }

        buffer[bytes_read] = '\0';
        process_command(client_socket, buffer);

        // The client is closing the connection, so break out of the loop
        break;
    }
    
    // Close the socket after the loop
    close(client_socket);
    printf("Client connection closed.\n");
}


void process_command(int client_socket, char *command) {
    char cmd[10], filename[BUFFER_SIZE], destination[BUFFER_SIZE] = "";
    int parsed_args;

    // Find the first newline or end of string
    char *cmd_end = strchr(command, '\n');
    if (cmd_end) {
        *cmd_end = '\0';  // Null-terminate the command part
    } else if (strchr(command, '\0') == NULL) {
        send_error_message(client_socket, "Invalid command syntax.\n");
        return;
    }

    // Parse the command
    parsed_args = sscanf(command, "%s %s %s", cmd, filename, destination);

    // Basic validation
    if (parsed_args < 2) {
        send_error_message(client_socket, "Invalid command syntax.\n");
        return;
    }

    if (strcmp(cmd, "ufile") == 0 && parsed_args == 3) {
        // Trim any extra spaces around the destination
        char *dest_start = destination;
        while (*dest_start == ' ') dest_start++;
        char *dest_end = dest_start + strlen(dest_start) - 1;
        while (*dest_end == ' ' && dest_end > dest_start) dest_end--;
        dest_end[1] = '\0';

        upload_file(client_socket, filename, dest_start);
    } else if (strcmp(cmd, "dfile") == 0 && parsed_args == 2) {
        download_file(client_socket, filename);
    } else if (strcmp(cmd, "rmfile") == 0 && parsed_args == 2) {
        remove_file(client_socket, filename);
    } else if (strcmp(cmd, "dtar") == 0 && parsed_args == 2) {
        create_tar_and_send(client_socket, filename);
    } else if (strcmp(cmd, "display") == 0 && parsed_args == 2) {
        list_files(client_socket, filename);
    } else {
        send_error_message(client_socket, "Unknown command or incorrect arguments.\n");
    }
}




void upload_file(int client_socket, char *filename, char *destination) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char file_path[BUFFER_SIZE];
    char *file_ext = strrchr(filename, '.');

    if (file_ext && strcmp(file_ext, ".pdf") == 0) {

        strncpy(file_path, destination, BUFFER_SIZE);

        // Check if the path starts with "/home/mahmud74/smain"
        if (strncmp(file_path, "/home/mahmud74/smain", 20) == 0) {  // Corrected length to 20 characters
            // Manually construct the new path
            const char *new_base = "/home/mahmud74/spdf";
            const char *remaining_path = destination + 20;  // Corrected offset to 20


            // Construct the final path
            snprintf(file_path, BUFFER_SIZE, "%s%s", new_base, remaining_path);
        }


        // Prepare the command to send to the server
        snprintf(buffer, BUFFER_SIZE, "ufile %s %s", filename, file_path);
        
        // // Print the buffer content for debugging
        // printf("Buffer: '%s'\n", buffer);

        // Send the command to the server
        communicate_with_server(client_socket, SPDF_IP, buffer, SPDF_PORT);
    }




    
    else if (file_ext && strcmp(file_ext, ".txt") == 0) {
       strncpy(file_path, destination, BUFFER_SIZE);

        // Check if the path starts with "/home/mahmud74/smain"
        if (strncmp(file_path, "/home/mahmud74/smain", 20) == 0) {  // Corrected length to 20 characters
            // Manually construct the new path
            const char *new_base = "/home/mahmud74/stext";
            const char *remaining_path = destination + 20;  // Corrected offset to 20


            // Construct the final path
            snprintf(file_path, BUFFER_SIZE, "%s%s", new_base, remaining_path);
        }


        // Prepare the command to send to the server
        snprintf(buffer, BUFFER_SIZE, "ufile %s %s", filename, file_path);
        
        // // Print the buffer content for debugging
        // printf("Buffer: '%s'\n", buffer);

        // Send the command to the server
        communicate_with_server(client_socket, STEXT_IP, buffer, STEXT_PORT);

    } 
    
    
    else if (file_ext && strcmp(file_ext, ".c") == 0) {
        // Handle C file: Store in Smain server
        snprintf(file_path, BUFFER_SIZE, "%s/%s", destination, filename);

        // Ensure the directory path is created properly
        create_directory_recursive(destination);

        // Open the file to write the incoming data
        int file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0) {
            perror("File open failed");
            send_error_message(client_socket, "Failed to open file for writing.\n");
            return;
        }

        // Start receiving the file content
        while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
            if (write(file_fd, buffer, bytes_read) != bytes_read) {
                perror("File write failed");
                send_error_message(client_socket, "Failed to write to file.\n");
                close(file_fd);
                return;
            }
        }

        if (bytes_read == 0) {
            printf("File transfer complete.\n");
        } else if (bytes_read < 0) {
            perror("Read error");
            send_error_message(client_socket, "Error reading file content.\n");
        }

        close(file_fd);

        // Send success message to client
        const char *success_message = "File uploaded successfully.\n";
        send(client_socket, success_message, strlen(success_message), 0);
    } else {
        send_error_message(client_socket, "Unsupported file type.\n");
    }

    // Properly shutdown and close the socket
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);

    printf("Connection closed after file upload.\n");
}




void download_file(int client_socket, char *filename) {
    char buffer[BUFFER_SIZE], file_path[BUFFER_SIZE];
    ssize_t bytes_read;
    int file_fd;  // Declare the file descriptor for local file handling

    printf("Download request for file: %s\n", filename);  // Debugging output

    // Determine file type
    char *file_ext = strrchr(filename, '.');

    if (file_ext && strcmp(file_ext, ".pdf") == 0) {
        // printf("Handling .pdf file\n");  // Debugging output

        // Replace the "/home/mahmud74/smain" prefix with "/home/mahmud74/spdf"
        if (strncmp(filename, "/home/mahmud74/smain", 20) == 0) {
            snprintf(file_path, BUFFER_SIZE, "/home/mahmud74/spdf%s", filename + 20);
        } else {
            snprintf(file_path, BUFFER_SIZE, "%s", filename);
        }

        // Send the download request to the Spdf server
        snprintf(buffer, BUFFER_SIZE, "dfile %s", file_path);
        printf("Sending request to Spdf: %s\n", buffer);  // Debugging output
        communicate_with_server(client_socket, SPDF_IP, buffer, SPDF_PORT);

        // Send a success message to the client
        const char *success_message = "File downloaded successfully.\n";
        send(client_socket, success_message, strlen(success_message), 0);

        printf("PDF file transfer complete and connection closed.\n");
    } else if (file_ext && strcmp(file_ext, ".txt") == 0) {
        
        // printf("Handling .txt file\n");  // Debugging output

        // Replace the "/home/mahmud74/smain" prefix with "/home/mahmud74/stext"
        if (strncmp(filename, "/home/mahmud74/smain", 20) == 0) {
            snprintf(file_path, BUFFER_SIZE, "/home/mahmud74/stext%s", filename + 20);
        } else {
            snprintf(file_path, BUFFER_SIZE, "%s", filename);
        }

        // Send the download request to the Spdf server
        snprintf(buffer, BUFFER_SIZE, "dfile %s", file_path);
        printf("Sending request to Stext: %s\n", buffer);  // Debugging output
        communicate_with_server(client_socket, STEXT_IP, buffer, STEXT_PORT);

        // Send a success message to the client
        const char *success_message = "File downloaded successfully.\n";
        send(client_socket, success_message, strlen(success_message), 0);

        printf("TEXT file transfer complete and connection closed.\n");
    } else {
        // printf("Handling .c file\n");  // Debugging output

        // Handle .c files locally

        // Handle tilde expansion for the home directory
        char *home_dir = getenv("HOME");
        if (filename[0] == '~') {
            snprintf(file_path, BUFFER_SIZE, "%s%s", home_dir, filename + 1);
        } else {
            snprintf(file_path, BUFFER_SIZE, "%s", filename);
        }

        // Attempt to open the file
        if ((file_fd = open(file_path, O_RDONLY)) < 0) {
            perror("File open failed");
            send_error_message(client_socket, "File not found or could not be opened.\n");
            return;
        }

        // Send the file content to the client
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            // printf("Bytes read from .c file: %zd\n", bytes_read);  // Debugging output
            if (write(client_socket, buffer, bytes_read) != bytes_read) {
                perror("File send failed");
                close(file_fd);
                return;
            }
        }

        if (bytes_read < 0) {
            perror("File read failed");
        }

        close(file_fd);

        // Send a success message to the client
        // const char *success_message = "File downloaded successfully.\n";
        // send(client_socket, success_message, strlen(success_message), 0);

        shutdown(client_socket, SHUT_WR);  // Indicate that the transfer is complete
        close(client_socket);
        printf("C file transfer complete and connection closed.\n");
    }
}


















void remove_file(int client_socket, char *filename) {
    char buffer[BUFFER_SIZE]; 
    char *file_ext = strrchr(filename, '.');

    if (file_ext && strcmp(file_ext, ".pdf") == 0) {
        // Convert the path from ~/smain to ~/spdf for .pdf files
        char file_path[BUFFER_SIZE];
        if (strncmp(filename, "/home/mahmud74/smain", 20) == 0) {
            snprintf(file_path, BUFFER_SIZE, "/home/mahmud74/spdf%s", filename + 20);
        } else {
            snprintf(file_path, BUFFER_SIZE, "%s", filename);
        }

        // Instruct Spdf to delete the file
        snprintf(buffer, BUFFER_SIZE, "rmfile %s", file_path);
        communicate_with_server(client_socket, SPDF_IP, buffer, SPDF_PORT);
    } else if (file_ext && strcmp(file_ext, ".txt") == 0) {
        // Convert the path from ~/smain to ~/stext for .pdf files
        char file_path[BUFFER_SIZE];
        if (strncmp(filename, "/home/mahmud74/smain", 20) == 0) {
            snprintf(file_path, BUFFER_SIZE, "/home/mahmud74/stext%s", filename + 20);
        } else {
            snprintf(file_path, BUFFER_SIZE, "%s", filename);
        }

        // Instruct Stext to delete the file
        snprintf(buffer, BUFFER_SIZE, "rmfile %s", file_path);
        communicate_with_server(client_socket, STEXT_IP, buffer, STEXT_PORT);
    } else if (file_ext && strcmp(file_ext, ".c") == 0) {
        // For .c files, use the path as is
        char file_path[BUFFER_SIZE];
        snprintf(file_path, BUFFER_SIZE, "%s", filename);

        if (unlink(file_path) != 0) {
            perror("File deletion failed");
            send_error_message(client_socket, "Failed to delete file.\n");
        } else {
            printf("File deleted: %s\n", filename);
            // Send success message to the client
            const char *success_message = "File deleted successfully.\n";
            send(client_socket, success_message, strlen(success_message), 0);
        }
    } else {
        send_error_message(client_socket, "Unsupported file type.\n");
    }
}










void create_tar_and_send(int client_socket, char *filetype) {
    char command[BUFFER_SIZE], tar_file[BUFFER_SIZE];
    char buffer[BUFFER_SIZE]; // Declare the buffer here
    ssize_t bytes_read;
    int file_fd;


    if (strcmp(filetype, ".pdf") == 0) {
        // Request Spdf to create a tar file
        communicate_with_server(client_socket, SPDF_IP, "tar_request_command", SPDF_PORT);
    } else if (strcmp(filetype, ".txt") == 0) {
        // Request Stext to create a tar file
        communicate_with_server(client_socket, STEXT_IP, "tar_request_command", STEXT_PORT);
    } else {
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        send_error_message(client_socket, "Failed to retrieve home directory.\n");
        return;
    }

    snprintf(tar_file, BUFFER_SIZE, "files.tar");
    
    // This command only adds files of the specified type to the tar archive and suppresses warnings
    snprintf(command, BUFFER_SIZE, "find %s/%s -type f -name '*%s' -print0 | tar --null -cvf %s --files-from - 2>/dev/null", home_dir, SM_DIR, filetype, tar_file);

    if (system(command) != 0) {
        send_error_message(client_socket, "Failed to create tar file.\n");
        return;
    }

    if ((file_fd = open(tar_file, O_RDONLY)) < 0) {
        perror("Tar file open failed");
        send_error_message(client_socket, "Failed to open tar file.\n");
        return;
    }

    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, buffer, bytes_read) != bytes_read) {
            perror("Tar file read failed");
            send_error_message(client_socket, "Failed to send tar file.\n");
            close(file_fd);
            return;
        }
    }

    close(file_fd);
    unlink(tar_file);


    // Send success message to the client
    const char *success_message = "Tar file created and sent successfully.\n";
    send(client_socket, success_message, strlen(success_message), 0);
    }

}





void list_files(int client_socket, char *path) {
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    char *home_dir = getenv("HOME");
    FILE *fp;

    // Adjust the path for .c files
    if (strncmp(path, home_dir, strlen(home_dir)) == 0) {
        // If the path already starts with the home directory, use it as-is
        snprintf(command, BUFFER_SIZE, "find %s -type f -name '*.c'", path);
    } else {
        // Otherwise, construct the path relative to the home directory
        snprintf(command, BUFFER_SIZE, "find %s/%s -type f -name '*.c'", home_dir, path);
    }

    // printf("Executing command: %s\n", command);  // Debugging line

    if ((fp = popen(command, "r")) == NULL) {
        perror("popen failed");
        send_error_message(client_socket, "Failed to list .c files.\n");
        return;
    }

    // Read the output of the command, extract filenames, and send them to the client
    while (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
        // Remove any trailing newline character from the file path
        buffer[strcspn(buffer, "\n")] = 0;

        // Extract the base filename
        char *filename = basename(buffer);

        // Send the filename back to the client
        send(client_socket, filename, strlen(filename), 0);
        send(client_socket, "\n", 1, 0);  // Send newline to separate filenames
    }
    pclose(fp);

    // 2. Request .pdf files from Spdf
    snprintf(command, BUFFER_SIZE, "display %s", path);
    // printf("Sending command to Spdf: %s\n", command);  // Debugging line
    communicate_with_server(client_socket, SPDF_IP, command, SPDF_PORT);
    communicate_with_server(client_socket, STEXT_IP, command, STEXT_PORT);

    // Indicate the end of the list
    const char *end_message = "EOF";
    send(client_socket, end_message, strlen(end_message), 0);
}












void communicate_with_server(int client_socket, char *server_ip, char *command, int server_port) {
    int server_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        send_error_message(client_socket, "Internal server error.\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        send_error_message(client_socket, "Internal server error.\n");
        close(server_socket);
        return;
    }

    // Connect to the server
    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        send_error_message(client_socket, "Internal server error.\n");
        close(server_socket);
        return;
    }

    // Send the initial command to the server
    printf("Sending to server: '%s'\n", command);
    send(server_socket, command, strlen(command), 0);

    // Determine the command type by inspecting the command string
    if (strncmp(command, "ufile", 5) == 0) {
        // ufile command: Read from the client and forward to the server
        while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
            // printf("Forwarding %zd bytes to server\n", bytes_read);
            if (write(server_socket, buffer, bytes_read) != bytes_read) {
                perror("Failed to forward data to server");
                break;
            }
        }

        // Close the write side to signal the end of data
        shutdown(server_socket, SHUT_WR);
    }

    // Receive response from the server and forward it to the client
    while ((bytes_read = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (write(client_socket, buffer, bytes_read) != bytes_read) {
            perror("Failed to forward response to client");
            break;
        }
    }

    if (bytes_read == 0) {
        printf("Server closed connection after sending data.\n");
    } else if (bytes_read < 0) {
        perror("Error reading from server");
    }

    // Properly shutdown and close the server socket
    shutdown(server_socket, SHUT_RDWR);
    close(server_socket);

    // Signal to the client that we're done (in case it's waiting for more data)
    shutdown(client_socket, SHUT_WR);
    close(client_socket);
}












void create_directory_recursive(const char *path) {
    char temp[BUFFER_SIZE];
    char *ptr = NULL;
    size_t len;

    // Copy the path to a temporary buffer
    // printf("Debug: Starting directory creation for path: %s\n", path);
    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);

    // Remove trailing slash only if the path is not the root "/"
    if (len > 1 && temp[len - 1] == '/') {
        temp[len - 1] = 0;
        // printf("Debug: Removed trailing slash, new path: %s\n", temp);
    }

    // Iterate over the path and create directories one by one
    for (ptr = temp + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = 0;
            // printf("Debug: Attempting to create directory: %s\n", temp);
            // Attempt to create the directory
            if (mkdir(temp, 0755) == -1) {
                if (errno == EEXIST) {
                    // printf("Debug: Directory %s already exists.\n", temp);
                } else {
                    perror("mkdir failed");
                    // printf("Debug: Failed to create directory %s\n", temp);
                    return;
                }
            } else {
                // printf("Debug: Successfully created directory: %s\n", temp);
            }
            *ptr = '/';
        }
    }

    // Create the final directory in the path
    // printf("Debug: Attempting to create final directory: %s\n", temp);
    if (mkdir(temp, 0755) == -1) {
        if (errno == EEXIST) {
            // printf("Debug: Final directory %s already exists.\n", temp);
        } else {
            perror("mkdir failed");
            // printf("Debug: Failed to create final directory %s\n", temp);
        }
    } else {
        // printf("Debug: Successfully created final directory: %s\n", temp);
    }
}






int validate_filename(const char *filename) {
    if (strstr(filename, "..") != NULL || filename[0] == '/') {
        return 0;
    }
    return 1;
}

void send_error_message(int client_socket, const char *message) {
    write(client_socket, message, strlen(message));
}

void create_base_directories() {
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        perror("Failed to retrieve home directory");
        exit(EXIT_FAILURE);
    }

    char sm_dir[BUFFER_SIZE];
    char spdf_dir[BUFFER_SIZE];
    char stext_dir[BUFFER_SIZE];

    snprintf(sm_dir, BUFFER_SIZE, "%s/%s", home_dir, SM_DIR);
    snprintf(spdf_dir, BUFFER_SIZE, "%s/spdf", home_dir);
    snprintf(stext_dir, BUFFER_SIZE, "%s/stext", home_dir);

    mkdir(sm_dir, 0755);
    mkdir(spdf_dir, 0755);
    mkdir(stext_dir, 0755);
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