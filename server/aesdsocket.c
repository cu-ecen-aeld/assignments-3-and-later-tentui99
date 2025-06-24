// server/aesdsocket.c
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define BACKLOG 5
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

int sockfd = -1;
int client_sock = -1;

void cleanup_and_exit(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (client_sock != -1) close(client_sock);
    if (sockfd != -1) close(sockfd);
    remove(DATA_FILE);
    closelog();
    exit(0);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    char *recv_data = NULL;
    size_t total_len = 0;

    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Handle daemon mode
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }

    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    listen(sockfd, BACKLOG);

    while (1) {
        client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_sock == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        recv_data = NULL;
        total_len = 0;

        while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
            recv_data = realloc(recv_data, total_len + bytes_received + 1);
            if (!recv_data) {
                syslog(LOG_ERR, "Memory allocation failed");
                break;
            }
            memcpy(recv_data + total_len, buffer, bytes_received);
            total_len += bytes_received;
            recv_data[total_len] = '\0';

            if (memchr(buffer, '\n', bytes_received)) break;
        }

        // === Ghi dữ liệu vào file ===
        if (recv_data) {
            FILE *fp = fopen(DATA_FILE, "a");
            if (fp) {
                fwrite(recv_data, 1, total_len, fp);
                fflush(fp); // đảm bảo ghi xuống disk
                fclose(fp);
            } else {
                syslog(LOG_ERR, "Failed to open file for writing");
            }
            free(recv_data);
        }

        // === Gửi lại nội dung file ===
        FILE *fp = fopen(DATA_FILE, "r");
        if (fp) {
            while ((bytes_received = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                send(client_sock, buffer, bytes_received, 0);
            }
            fclose(fp);
        } else {
            syslog(LOG_ERR, "Failed to open file for reading");
        }

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_sock);
        client_sock = -1;
    }

    closelog();
    return 0;
}
