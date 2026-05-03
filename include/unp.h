/* Minimal unp.h compatibility header providing common UNP wrapper functions
 * This is a lightweight local implementation so the project can compile
 * without the full UNP library installed. It intentionally mimics the
 * wrapper names used in the book (Socket, Bind, Listen, Accept, Read, Select, Close).
 */
#ifndef UNP_H
#define UNP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static inline int Socket(int family, int type, int protocol) {
    int n = socket(family, type, protocol);
    if (n < 0) { perror("Socket"); exit(1); }
    return n;
}

static inline int Bind(int fd, const struct sockaddr *sa, socklen_t salen) {
    if (bind(fd, sa, salen) < 0) { perror("Bind"); exit(1); }
    return 0;
}

static inline int Listen(int fd, int backlog) {
    if (listen(fd, backlog) < 0) { perror("Listen"); exit(1); }
    return 0;
}

static inline int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr) {
    int n;
    for (;;) {
        n = accept(fd, sa, salenptr);
        if (n >= 0) return n;
        if (errno == EINTR) continue;
        perror("Accept");
        exit(1);
    }
}

static inline void Close(int fd) {
    if (close(fd) < 0) perror("Close");
}

static inline ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t n;
    for (;;) {
        n = read(fd, buf, count);
        if (n >= 0) return n;
        if (errno == EINTR) continue;
        perror("Read");
        return -1;
    }
}

static inline int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    int n;
    for (;;) {
        n = select(nfds, readfds, writefds, exceptfds, timeout);
        if (n >= 0) return n;
        if (errno == EINTR) continue;
        perror("Select");
        exit(1);
    }
}

#endif /* UNP_H */
