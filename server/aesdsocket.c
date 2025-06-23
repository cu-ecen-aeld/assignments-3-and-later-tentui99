// server/aesdsocket.c
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <signal.h>

#define PORT 9000
#define BACKLOG 5
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"
int sockfd = -1;
int client_sock = -1;

void signal_handler(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (client_sock != -1) {
        close(client_sock);
        client_sock = -1;
    }

    if (sockfd != -1) {
        close(sockfd);
    }
    remove("/var/tmp/aesdsocketdata");
    closelog();
    exit(0);
}

int main(int argc, char *argv[]) {

    int client_sock;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    // Mở syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Đăng ký tín hiệu
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Tạo socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Failed to create socket");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Gán socket vào port
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket");
        close(sockfd);
        return -1;
    }

    listen(sockfd, BACKLOG);

    while (1) {
 client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
    if (client_sock == -1) {
        syslog(LOG_ERR, "Failed to accept");
        continue;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // Chuẩn bị bộ đệm nhận dữ liệu
    char buffer[BUFFER_SIZE];
    char *recv_data = NULL;
    size_t total_len = 0;
    ssize_t bytes_received;

    // Nhận dữ liệu cho đến khi gặp '\n'
    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
        recv_data = realloc(recv_data, total_len + bytes_received + 1);  // +1 để đảm bảo có '\0'
        if (!recv_data) {
            syslog(LOG_ERR, "Memory allocation failed");
            break;
        }
        memcpy(recv_data + total_len, buffer, bytes_received);
        total_len += bytes_received;
        recv_data[total_len] = '\0';

        // Kiểm tra nếu đã nhận được '\n' thì dừng lại
        if (strchr(buffer, '\n')) {
            break;
        }
    }

    // Ghi dữ liệu vào file
    if (recv_data) {
        FILE *fp = fopen(DATA_FILE, "a");
        if (fp) {
            fwrite(recv_data, 1, total_len, fp);
            fclose(fp);
        } else {
            syslog(LOG_ERR, "Failed to open file for writing");
        }

        free(recv_data);
    }

    // Gửi lại toàn bộ nội dung file cho client
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
    }

    closelog();
    return 0;
}
