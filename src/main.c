/* UNP-based main for Raspberry Pi honeypot
 * Uses wrapper functions from unp.h: Socket(), Bind(), Listen(), Accept(), Read(), Select()
 * Listens on ports 23 and 80, multiplexes with select(), accepts connections,
 * reads up to 1024 bytes and passes payload to process_attack().
 */

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>

#include <unp.h>
#include "../hardware/include/honeypot.h"

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

/* Maximum bytes to read from initial connection */
#define ATTACK_READ_BYTES 1024

static int open_listen(const char *port) {
    struct addrinfo hints, *res, *rp;
    int listenfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) return -1;

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        /* Socket is a UNP wrapper that handles errors */
        listenfd = Socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
        if (Bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (Listen(listenfd, 16) == 0) break;
        }
        close(listenfd);
        listenfd = -1;
    }

    freeaddrinfo(res);
    return listenfd;
}

static void process_attack(int client_fd, struct sockaddr_storage *peer, socklen_t peerlen, const char *payload, ssize_t payload_len, uint16_t dst_port) {
    AttackEvent ev;
    memset(&ev, 0, sizeof(ev));
    (void)client_fd;

    /* Fill src ip/port */
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    if (getnameinfo((struct sockaddr*)peer, peerlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
        strncpy(host, "?", sizeof(host));
        strncpy(serv, "0", sizeof(serv));
    }

    snprintf(ev.src_ip, sizeof(ev.src_ip), "%.63s", host);
    ev.src_port = (uint16_t)atoi(serv);
    ev.dst_port = dst_port;

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ev.timestamp, sizeof(ev.timestamp), "%Y-%m-%dT%H:%M:%S%z", &tmv);

    if (payload_len > 0) {
        ssize_t copy_len = payload_len > HONEYPOT_MAX_PAYLOAD ? HONEYPOT_MAX_PAYLOAD : payload_len;
        memcpy(ev.payload, payload, copy_len);
        ev.payload_len = copy_len;
    } else {
        ev.payload_len = 0;
    }

    /* Push to backend (timeout-safe sender) */
    push_to_backend(ev);
}

int main(void) {
    const char *ports[] = {"23", "80"};
    const int nports = sizeof(ports)/sizeof(ports[0]);
    int listen_fds[16];
    fd_set master_set, readset;
    int i, maxfd = -1;

    for (i = 0; i < nports; ++i) {
        int s = open_listen(ports[i]);
        if (s < 0) {
            fprintf(stderr, "Failed to open listen on port %s\n", ports[i]);
            listen_fds[i] = -1;
            continue;
        }
        listen_fds[i] = s;
        if (s > maxfd) maxfd = s;
    }

    FD_ZERO(&master_set);
    for (i = 0; i < nports; ++i) if (listen_fds[i] >= 0) FD_SET(listen_fds[i], &master_set);

    fprintf(stderr, "pi_bait (UNP): listening on ports 23 and 80\n");

    while (1) {
        readset = master_set;
        Select(maxfd + 1, &readset, NULL, NULL, NULL);

        for (i = 0; i < nports; ++i) {
            int ls = listen_fds[i];
            if (ls < 0) continue;
            if (!FD_ISSET(ls, &readset)) continue;

            struct sockaddr_storage peer;
            socklen_t plen = sizeof(peer);
            int connfd = Accept(ls, (struct sockaddr*)&peer, &plen);

            /* Read up to ATTACK_READ_BYTES as initial payload */
            char buf[ATTACK_READ_BYTES];
            ssize_t n = Read(connfd, buf, sizeof(buf));
            if (n < 0) n = 0;

            process_attack(connfd, &peer, plen, buf, n, (uint16_t)atoi(ports[i]));

            Close(connfd);
        }
    }

    for (i = 0; i < nports; ++i) if (listen_fds[i] >= 0) Close(listen_fds[i]);
    return 0;
}
