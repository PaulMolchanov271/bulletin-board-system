#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define SERVER_IP "0.0.0.0"
#define PORT 5555
#define BUFFER_SIZE 1024


void handle_server_response(int sock) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    if (bytes_read == 0) {
        printf("Connection closed by the server.\n");
        close(sock);
        exit(0);
    }
    return;
}

void open_file(const char *file_text)
{
    int fd[2];
    pipe(fd);
    if(fork() == 0) {
        close(fd[0]);
        dup2(fd[1], 1);
        close(fd[1]);
        execlp("echo", "echo", "-e", file_text, NULL);
        perror("echo");
        exit(1);
    }
    if(fork() == 0) {
        close(fd[1]);
        dup2(fd[0], 0);
        close(fd[0]);
        execlp("vim", "vim", "-", NULL);
        perror("vim");
        exit(1);
    }
    close(fd[0]);
    close(fd[1]);
    wait(NULL);
    wait(NULL);
    return;
}

void handle_delete_file(int sock, char* filename)
{
    int recv = -2;
    while(recv < -1) {
        read(sock, &recv, sizeof(recv));
    }
    if(recv == 1) {
        printf("The file was successfully deleted\n");
    } else if(recv == 0) {
        printf("A file with this name '%s' does not exist\n", filename);
    } else if(recv == -1) {
        printf("You do not have permission to delete files\n");
    }
    return;
}

void handle_view_file(int sock, char *filename)
{
    char *file_text;
    int received_bytes = 0, file_size = -2, i = 0;
    char buff[BUFFER_SIZE];
    int bytes_read;
    if(filename[0] == '\0') {
        printf("Enter what file you want to download: DOWNLOAD <filename>\n");
        return;
    }
    while ((filename[i] != ' ') && (filename[i] != '\0') && (filename[i] != '\n')) {
        i++;
    }
    filename[i] = '\0';
    while(file_size < -1) {
        read(sock, &file_size, sizeof(file_size));
    }
    printf("View file_size = %d\n", file_size);
    if(file_size == 0) {
        printf("Such file '%s' does not exist", filename);
        return;
    } else if(file_size == -1) {
        printf("You do not have sufficient permissions to access this file '%s'.\n", filename);
        return;
    }
    file_text = malloc((file_size+1) * sizeof(char));
    while (received_bytes < file_size) {
        bytes_read = read(sock, buff, sizeof(buff) - 1);
        if(bytes_read > 0) {
            buff[bytes_read] = 0;
            strcat(file_text, buff);
            received_bytes += bytes_read;
        }
    }
    file_text[received_bytes] = '\0';
    open_file(file_text);
    return;
}

void handle_download_file(int sock, char *filename)
{
    int received_bytes = 0, rc, fd, file_size = -2, i = 0;
    char buff[255];
    if(filename[0] == '\0') {
        printf("Enter what file you want to download: DOWNLOAD <filename>\n");
        return;
    }
    while ((filename[i] != ' ') && (filename[i] != '\0') && (filename[i] != '\n')) {
        i++;
    }
    filename[i] = '\0';
    while(file_size < -1) {
        read(sock, &file_size, sizeof(file_size));
    }
    printf("Download file_size = %d\n", file_size);
    if(file_size == 0) {
        printf("Such file '%s' does not exist", filename);
        return;
    } else if(file_size == -1) {
        printf("You do not have sufficient permissions to access this file '%s'.\n", filename);
        return;
    }
    fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if(fd == -1) {
        perror("file");
    }
    while(received_bytes < file_size) {
        rc = read(sock, buff, sizeof(buff)-1);
        if(rc > 0) {
            buff[rc] = 0;
            if(write(fd, buff, rc) != rc) {
                perror("Failed to write to file");
                close(fd);
                return;
            }
            received_bytes += rc;
        }
    }
    close(fd);
    return;
}

void handle_upload_file(int sock, char* filename)
{
    int rc, i, fd, file_size = 0;
    char buff[255];
    if (filename[0] == '\0') {
        printf("Enter what file you want to upload: UPLOAD <filename>\n");
        return;
    }
    i = 0;
    while ((filename[i] != ' ') && (filename[i] != '\0') && (filename[i] != '\n')) {
        i++;
    }
    filename[i] = '\0';
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "such file '%s' does not exist\n", filename);
        write(sock, &file_size, sizeof(file_size));
        return;
    }
    while((rc = read(fd, buff, sizeof(buff))) > 0) {
        file_size += rc;
    }
    write(sock, &file_size, sizeof(file_size));
    printf("file_size = %d\n", file_size);
    lseek(fd, 0, SEEK_SET);
    while((rc = read(fd, buff, sizeof(buff))) > 0) {
        write(sock, buff, rc);
    }
    close(fd);
    return;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int fd, rc, res, maxfd;
    fd_set readfds, writefds;
    char buff[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);
    for (;;) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(0, &readfds);
        FD_SET(sock, &readfds);
        //FD_SET(sock, &writefds);
        maxfd = sock;

        res = select(maxfd + 1, &readfds, /*&writefds*/NULL, NULL, NULL);

        if(res == -1) {
            perror("select");
            close(sock);
            return 1;
        }
        if (FD_ISSET(sock, &readfds)) {
            handle_server_response(sock);
        }
        if (FD_ISSET(0, &readfds)) {
            while ((rc = read(0, buff, BUFFER_SIZE)) > 0) {
                buff[rc] = 0;
                char* ptr = buff;
                while (*ptr == ' ') {
                    ptr++;
                }
                write(sock, buff, rc);
                if (buff[rc - 1] == '\n') {
                    if ((strncmp(ptr, "upload", 6) == 0) || (strncmp(ptr, "UPLOAD", 6) == 0)) {
                        ptr += 7;
                        handle_upload_file(sock, ptr);
                    } else if((strncmp(ptr, "download", 8) == 0) || (strncmp(ptr, "DOWNLOAD", 8) == 0)) {
                        ptr += 9;
                        while(*ptr == ' ') {
                            ptr++;
                        }
                        handle_download_file(sock, ptr);
                    } else if((strncmp(ptr, "view", 4) == 0) || (strncmp(ptr, "VIEW", 4) == 0)) {
                        ptr += 5;
                        while(*ptr == ' ') {
                            ptr++;
                        }
                        handle_view_file(sock, ptr);
                    } else if((strncmp(ptr, "delete", 6) == 0) || (strncmp(ptr, "DELETE", 6) == 0)) {
                        ptr += 7;
                        while(*ptr == ' ') {
                            ptr++;
                        }
                        handle_delete_file(sock, ptr);
                    } else {
                        break;
                    }
                }
            }
        }
    }
    close(sock);
    return 0;
}

