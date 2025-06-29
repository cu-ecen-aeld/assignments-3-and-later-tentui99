#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

#define PORT "9000"
#define BACKLOG 10
#define DATAFILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int sockfd = -1;
int client_fd = -1;
volatile sig_atomic_t exit_flag = 0;

void handle_signal(int signo) {
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_flag = 1;
}

void setup_signals() {
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits again

    umask(0);
    chdir("/");

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
}

int setup_server_socket() {
    struct addrinfo hints, *res, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rv));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (!p) {
        syslog(LOG_ERR, "Failed to bind");
        return -1;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        return -1;
    }

    return 0;
}

void handle_client(int client_fd, struct sockaddr_storage client_addr) {
    char ipstr[INET6_ADDRSTRLEN];
    void *addr;
    if (client_addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        addr = &(s->sin_addr);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
        addr = &(s->sin6_addr);
    }
    inet_ntop(client_addr.ss_family, addr, ipstr, sizeof ipstr);
    syslog(LOG_INFO, "Accepted connection from %s", ipstr);

    char *recv_buf = NULL;
    size_t total = 0;
    char buf[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
        recv_buf = realloc(recv_buf, total + bytes_received + 1);
        memcpy(recv_buf + total, buf, bytes_received);
        total += bytes_received;
        recv_buf[total] = '\0';

        if (strchr(recv_buf, '\n')) break;
    }

    if (bytes_received == -1) {
        syslog(LOG_ERR, "recv failed");
        goto cleanup;
    }

    int fd = open(DATAFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file");
        goto cleanup;
    }

    write(fd, recv_buf, total);
    close(fd);

    // Read full content and send back
    fd = open(DATAFILE, O_RDONLY);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to read file");
        goto cleanup;
    }

    while ((bytes_received = read(fd, buf, sizeof(buf))) > 0) {
        send(client_fd, buf, bytes_received, 0);
    }

    close(fd);

cleanup:
    syslog(LOG_INFO, "Closed connection from %s", ipstr);
    if (recv_buf) free(recv_buf);
}

int main(int argc, char *argv[]) {
    bool daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0);

    openlog("aesdsocket", LOG_PID, LOG_USER);
    setup_signals();

    if (setup_server_socket() == -1) {
        closelog();
        return 1;
    }

    if (daemon_mode) daemonize();

    while (!exit_flag) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof client_addr;

        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            if (exit_flag) break;
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        handle_client(client_fd, client_addr);
        close(client_fd);
    }

    // Cleanup
    if (sockfd != -1) close(sockfd);
    remove(DATAFILE);
    closelog();
    return 0;
}
