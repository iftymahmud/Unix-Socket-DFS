# Distributed File System Project

## Overview

This project was developed as part of the **COMP-8567 (Advanced Systems Programming)** course during the Summer 2024 semester at the University of Windsor. The goal was to implement a Distributed File System (DFS) using socket programming, where multiple clients can upload, download, and manage files across distributed servers. The system is designed to handle three types of files: .c, .pdf, and .txt.

## Architecture

The Distributed File System has three servers:
- **Smain:** The main server that directly interacts with the clients. It handles .c files locally and communicates with the other two servers for .pdf and .txt files.
- **Spdf:** The server dedicated to storing .pdf files.
- **Stext:** The server dedicated to storing .txt files.


## Key Features

1.  **File Upload (ufile):** Clients can upload .c, .pdf, and .txt files. While the client assumes all files are stored in Smain, .pdf and .txt files are automatically transferred to Spdf and Stext servers, respectively.
    
2.  **File Download (dfile):** Clients can download files by specifying their paths. Depending on the file type, Smain either fetches the file locally or communicates with Spdf or Stext to retrieve it.
    
3.  **File Deletion (rmfile):** Clients can request the deletion of files, with Smain handling the request locally or delegating it to the appropriate server based on the file type.
    
4.  **Create Tar Archive (dtar):** Clients can request a tar archive of all files of a specific type. Smain either creates the tar archive locally or requests it from Spdf or Stext.

5. **Display File List (display):** Clients can request a list of all files in a specified directory, and Smain provides a consolidated list of .c, .pdf, and .txt files across all servers.

## Implementation Details

The project is implemented in C using socket programming to manage communication between the clients and the servers. Each server runs on a different machine or terminal.

### Files:

-   **Smain.c:** Contains the implementation of the main server that handles client requests, manages .c files, and coordinates with Spdf and Stext.
    
-   **Spdf.c:** Manages the storage and retrieval of .pdf files.
    
-   **Stext.c:** Manages the storage and retrieval of .txt files.

- **client24s.c:** The client program that sends commands to Smain for file operations.

## How to Run

1. Compile all C files using the appropriate compiler (e.g., gcc).
2. Start each server (Smain, Spdf, Stext) on different machines or terminals.
3. Run the client program to interact with the Distributed File System by entering the commands defined in the project.



## Error Handling


The system is designed to handle various error conditions, such as invalid commands, file not found, and communication errors between servers.

## Acknowledgments

This project was developed as part of the COMP-8567 course at the University of Windsor. Special thanks to our course instructor, Dr. Prashanth Cheluvasai Ranga, for the guidance provided throughout the semester.

