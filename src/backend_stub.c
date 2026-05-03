/* Timeout-safe backend sender for the Pi honeypot.
 * Opens a TCP connection to the hardcoded backend and sends a JSON record.
 */

#define _POSIX_C_SOURCE 200112L
#include "../hardware/include/honeypot.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BACKEND_IP   "192.168.1.10"
#define BACKEND_PORT "5000"
#define CONNECT_TIMEOUT_SEC 2
#define SEND_TIMEOUT_SEC 2

static size_t json_escape(const unsigned char *src, size_t len, char *dst, size_t dstlen) {
    size_t di = 0;

    for (size_t si = 0; si < len && di + 2 < dstlen; ++si) {
        unsigned char c = src[si];
        if (c == '\\' || c == '"') {
            dst[di++] = '\\';
            dst[di++] = (char)c;
        } else if (c == '\n') {
            dst[di++] = '\\';
            dst[di++] = 'n';
        } else if (c == '\r') {
            dst[di++] = '\\';
            dst[di++] = 'r';
        } else if (c == '\t') {
            dst[di++] = '\\';
            dst[di++] = 't';
        } else if (c >= 0x20 && c != 0x7f) {
            dst[di++] = (char)c;
        } else {
            int written = snprintf(dst + di, dstlen - di, "\\u%04x", c);
            if (written < 0 || (size_t)written >= dstlen - di) break;
            di += (size_t)written;
        }
    }

    if (di < dstlen) dst[di] = '\0';
    else dst[dstlen - 1] = '\0';
    return di;
}

static int wait_writable(int fd, int timeout_sec) {
    fd_set wfds;
    struct timeval tv;

    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    for (;;) {
        int rc = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (rc > 0) return 0;
        if (rc == 0) return -1;
        if (errno != EINTR) return -1;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;
    }
}

static int connect_with_timeout(struct sockaddr *sa, socklen_t salen, int timeout_sec) {
    int fd = socket(sa->sa_family, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

    int rc = connect(fd, sa, salen);
    if (rc == 0) {
        (void)fcntl(fd, F_SETFL, flags);
        return fd;
    }

    if (errno != EINPROGRESS) {
        close(fd);
        return -1;
    }

    if (wait_writable(fd, timeout_sec) < 0) {
        close(fd);
        return -1;
    }

    int so_error = 0;
    socklen_t errlen = sizeof(so_error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &errlen) < 0 || so_error != 0) {
        close(fd);
        errno = so_error;
        return -1;
    }

    (void)fcntl(fd, F_SETFL, flags);
    return fd;
}

static int send_all_with_timeout(int fd, const char *buf, size_t len, int timeout_sec) {
    size_t offset = 0;

    while (offset < len) {
        ssize_t n = send(fd, buf + offset, len - offset, 0);
        if (n > 0) {
            offset += (size_t)n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (wait_writable(fd, timeout_sec) < 0) return -1;
            continue;
        }

        return -1;
    }

    return 0;
}

int push_to_backend(AttackEvent event) {
    struct addrinfo hints, *res = NULL;
    int fd = -1;
    char payload_esc[HONEYPOT_MAX_PAYLOAD * 6 + 1];
    char json[4096 + HONEYPOT_MAX_PAYLOAD * 6];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(BACKEND_IP, BACKEND_PORT, &hints, &res) != 0) {
        return -1;
    }

    fd = connect_with_timeout(res->ai_addr, (socklen_t)res->ai_addrlen, CONNECT_TIMEOUT_SEC);
    freeaddrinfo(res);
    if (fd < 0) return -1;

    size_t payload_len = event.payload_len > 0 ? (size_t)event.payload_len : 0;
    if (payload_len > HONEYPOT_MAX_PAYLOAD) payload_len = HONEYPOT_MAX_PAYLOAD;
    json_escape(event.payload, payload_len, payload_esc, sizeof(payload_esc));

    int json_len = snprintf(
        json,
        sizeof(json),
        "{\"src_ip\":\"%s\",\"src_port\":%u,\"dst_port\":%u,\"timestamp\":\"%s\",\"payload\":\"%s\"}\n",
        event.src_ip,
        (unsigned)event.src_port,
        (unsigned)event.dst_port,
        event.timestamp,
        payload_esc);

    if (json_len < 0 || (size_t)json_len >= sizeof(json)) {
        close(fd);
        return -1;
    }

    if (send_all_with_timeout(fd, json, (size_t)json_len, SEND_TIMEOUT_SEC) < 0) {
        close(fd);
        return -1;
    }

    shutdown(fd, SHUT_WR);
    close(fd);
    return 0;
}
