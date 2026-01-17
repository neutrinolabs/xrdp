/**
 * Minimal TLS 1.3 ClientHello sender to test NeutrinoTLS server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Minimal TLS 1.3 ClientHello with X25519 key share */
static const unsigned char tls13_client_hello[] = {
    /* TLS Record Header */
    0x16,       /* Content Type: Handshake */
    0x03, 0x01, /* Legacy Version: TLS 1.0 (for compatibility) */
    0x00, 0xc8, /* Length: 200 bytes (will be updated) */

    /* Handshake Header */
    0x01,       /* Handshake Type: ClientHello */
    0x00, 0x00, 0xc4, /* Length: 196 bytes */

    /* ClientHello */
    0x03, 0x03, /* Legacy Version: TLS 1.2 */

    /* Client Random (32 bytes) */
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,

    /* Session ID */
    0x00,       /* Length: 0 (no session ID) */

    /* Cipher Suites */
    0x00, 0x02, /* Length: 2 bytes */
    0x13, 0x03, /* TLS_CHACHA20_POLY1305_SHA256 */

    /* Compression Methods */
    0x01,       /* Length: 1 byte */
    0x00,       /* null compression */

    /* Extensions */
    0x00, 0x91, /* Extensions length: 145 bytes */

    /* Supported Versions Extension */
    0x00, 0x2b, /* Extension Type: supported_versions */
    0x00, 0x03, /* Length: 3 bytes */
    0x02,       /* Versions length: 2 bytes */
    0x03, 0x04, /* TLS 1.3 */

    /* Supported Groups Extension */
    0x00, 0x0a, /* Extension Type: supported_groups */
    0x00, 0x04, /* Length: 4 bytes */
    0x00, 0x02, /* Groups length: 2 bytes */
    0x00, 0x1d, /* x25519 */

    /* Key Share Extension */
    0x00, 0x33, /* Extension Type: key_share */
    0x00, 0x26, /* Length: 38 bytes */
    0x00, 0x24, /* Key shares length: 36 bytes */
    0x00, 0x1d, /* Group: x25519 */
    0x00, 0x20, /* Key length: 32 bytes */
    /* Public key (32 bytes of dummy data) */
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,

    /* Signature Algorithms Extension */
    0x00, 0x0d, /* Extension Type: signature_algorithms */
    0x00, 0x04, /* Length: 4 bytes */
    0x00, 0x02, /* Algorithms length: 2 bytes */
    0x04, 0x03, /* ecdsa_secp256r1_sha256 */

    /* Server Name Extension */
    0x00, 0x00, /* Extension Type: server_name */
    0x00, 0x0e, /* Length: 14 bytes */
    0x00, 0x0c, /* Server names length: 12 bytes */
    0x00,       /* Type: host_name */
    0x00, 0x09, /* Name length: 9 bytes */
    'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't',
};

int main() {
    printf("=== TLS 1.3 ClientHello Test ===\n\n");

    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    /* Connect to xrdp */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3389);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    printf("[1/5] Connecting to 127.0.0.1:3389...\n");
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    printf("✓ TCP connected\n\n");

    /* Send X.224 Connection Request */
    printf("[2/5] Sending X.224 Connection Request...\n");
    unsigned char x224_req[] = {
        0x03, 0x00, 0x00, 0x13, 0x0e, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00  /* Request SSL */
    };

    if (send(sock, x224_req, sizeof(x224_req), 0) < 0) {
        perror("send x224");
        close(sock);
        return 1;
    }

    /* Receive X.224 response */
    unsigned char buf[1024];
    int len = recv(sock, buf, sizeof(buf), 0);
    if (len < 0) {
        perror("recv x224");
        close(sock);
        return 1;
    }
    printf("✓ X.224 response: %d bytes\n\n", len);

    /* Send TLS ClientHello */
    printf("[3/5] Sending TLS 1.3 ClientHello...\n");
    if (send(sock, tls13_client_hello, sizeof(tls13_client_hello), 0) < 0) {
        perror("send clienthello");
        close(sock);
        return 1;
    }
    printf("✓ Sent %zu bytes\n\n", sizeof(tls13_client_hello));

    /* Try to receive ServerHello */
    printf("[4/5] Waiting for ServerHello...\n");
    len = recv(sock, buf, sizeof(buf), 0);
    if (len < 0) {
        perror("recv serverhello");
        close(sock);
        return 1;
    }

    printf("✓ Received %d bytes\n", len);
    printf("   First 32 bytes: ");
    for (int i = 0; i < 32 && i < len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n\n");

    /* Check response */
    printf("[5/5] Analysis:\n");
    if (len > 0 && buf[0] == 0x16) {
        printf("✓ Received TLS Handshake record\n");
        if (len > 5 && buf[5] == 0x02) {
            printf("✓ Contains ServerHello!\n");
            printf("\n=== SUCCESS: NeutrinoTLS responded ===\n");
        } else {
            printf("⚠ Not a ServerHello (type: 0x%02x)\n", len > 5 ? buf[5] : 0);
        }
    } else {
        printf("✗ Unexpected response type: 0x%02x\n", buf[0]);
    }

    close(sock);
    return 0;
}
