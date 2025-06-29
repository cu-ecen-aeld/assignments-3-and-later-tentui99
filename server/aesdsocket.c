#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BACKLOG (10)
#define PORT "9000"
#define MY_MAX_SIZE 500

struct addrinfo *p;
int socketfd = -1;
int new_fd = -1;
int fd = -1;

void cleanup()
{
    if (fd != -1) close(fd);
    if (socketfd != -1) close(socketfd);
    if (new_fd != -1) close(new_fd);
    if (p) freeaddrinfo(p);
    remove("/var/tmp/aesdsocketdata.txt");
}

void signal_handler(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    cleanup();
    closelog();
    exit(0);
}

void daemonize()
{
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // First child exits

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed");
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socketfd == -1) continue;

        int yes = 1;
        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(socketfd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(socketfd);
        socketfd = -1;
    }

    if (p == NULL) {
        syslog(LOG_ERR, "Failed to bind socket");
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    if (listen(socketfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        return -1;
    }

    // Truncate file mỗi lần server chạy (để test.sh không fail)
    int truncate_fd = open("/var/tmp/aesdsocketdata.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (truncate_fd != -1) {
        close(truncate_fd);
    } else {
        syslog(LOG_ERR, "Failed to truncate file");
    }

    while (1)
    {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        new_fd = accept(socketfd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1) {
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char *buf = malloc(MY_MAX_SIZE);
        size_t total_len = 0;
        size_t current_buf_size = MY_MAX_SIZE;
        int found_newline = 0;

        while (!found_newline) {
            ssize_t rc = recv(new_fd, buf + total_len, current_buf_size - total_len, 0);
            if (rc <= 0) break;
            total_len += rc;

            if (memchr(buf, '\n', total_len)) {
                found_newline = 1;
            }

            if (total_len == current_buf_size) {
                current_buf_size *= 2;
                buf = realloc(buf, current_buf_size);
            }
        }

        fd = open("/var/tmp/aesdsocketdata.txt", O_RDWR | O_CREAT | O_APPEND, 0644);
        if (fd != -1 && total_len > 0) {
            write(fd, buf, total_len);
        }

        lseek(fd, 0, SEEK_SET);
        char sendbuf[MY_MAX_SIZE];
        ssize_t rd;
        while ((rd = read(fd, sendbuf, sizeof(sendbuf))) > 0) {
            send(new_fd, sendbuf, rd, 0);
        }

        close(fd);
        fd = -1;
        close(new_fd);
        new_fd = -1;
        free(buf);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    cleanup();
    closelog();
    return 0;
}
