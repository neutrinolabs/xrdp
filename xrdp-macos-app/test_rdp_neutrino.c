/**
 * RDP Test Client using NeutrinoTLS
 * Simplified version - just tests that we can connect without using tls13_connect
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 3389;

    printf("==================================================\n");
    printf("RDP Connection Test - Simple TCP+X.224\n");
    printf("==================================================\n\n");

    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    /* Connect */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("[1/3] ✓ TCP connected to %s:%d\n", host, port);

    /* Send X.224 Connection Request */
    unsigned char x224_req[] = {
        0x03, 0x00, 0x00, 0x13, 0x0e, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00
    };

    if (send(sock, x224_req, sizeof(x224_req), 0) < 0) {
        perror("send x224");
        close(sock);
        return 1;
    }

    /* Receive X.224 response */
    unsigned char response[1024];
    int len = recv(sock, response, sizeof(response), 0);
    if (len < 0) {
        perror("recv x224");
        close(sock);
        return 1;
    }

    printf("[2/3] ✓ X.224 negotiation complete (%d bytes)\n", len);
    printf("      Response: ");
    for (int i = 0; i < len && i < 20; i++) {
        printf("%02x ", response[i]);
    }
    printf("\n");

    /* Check if server accepted TLS */
    if (len >= 19 && response[11] == 0x02) {
        printf("[3/3] ✓ Server accepted TLS negotiation\n");
        printf("\n==================================================\n");
        printf("✓ Server is ready for TLS 1.3 handshake\n");
        printf("✓ Use Python test client for full TLS verification:\n");
        printf("  python3 /tmp/test-full-tls13.py\n");
        printf("==================================================\n\n");
    } else {
        printf("[3/3] ✗ Server did not accept TLS\n");
    }

    close(sock);
    return 0;
}
