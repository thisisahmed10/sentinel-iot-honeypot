/* hardware/include/honeypot.h
 * Definitions for AttackEvent and external interfaces used by the RPi honeypot
 */

#ifndef HONEYPOT_H
#define HONEYPOT_H

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>

#define HONEYPOT_MAX_PAYLOAD 2048
#define HONEYPOT_TS_LEN 64
#define HONEYPOT_IP_STRLEN 64

/* Captures an observed attack/connection event */
typedef struct AttackEvent {
    char src_ip[HONEYPOT_IP_STRLEN];
    uint16_t src_port;
    uint16_t dst_port;
    char timestamp[HONEYPOT_TS_LEN]; /* ISO-8601 string */
    ssize_t payload_len;
    unsigned char payload[HONEYPOT_MAX_PAYLOAD];
} AttackEvent;

/* Handle an incoming telnet connection on the honeypot.
 * Implementations should read relevant initial data, fill an AttackEvent,
 * and return 0 on success, or -1 on error.
 */
int handle_telnet_connection(int client_fd, struct sockaddr_storage *peer, socklen_t peerlen);

/* Push an attack event to the central backend (non-blocking preferred).
 * Implementations should serialize/push the event and return 0 on success.
 */
int push_to_backend(AttackEvent event);

#endif /* HONEYPOT_H */
