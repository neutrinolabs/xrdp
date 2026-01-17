/*
 * Simple test server to verify NeutrinoTLS works standalone
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common/neutrinossl.h"

#define PORT 3390
#define CERT_FILE "/Users/cyclic/xrdp/xrdp-macos-app/build/Debug/xrdp.app/Contents/Resources/etc/xrdp/cert.pem"
#define KEY_FILE "/Users/cyclic/xrdp/xrdp-macos-app/build/Debug/xrdp.app/Contents/Resources/etc/xrdp/key.pem"

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    NEUTRINOSSL_CTX *ctx;
    NEUTRINOSSL *ssl;
    char buffer[4096];
    int ret;

    printf("=== NeutrinoTLS Test Server ===\n");
    printf("Port: %d\n", PORT);
    printf("Cert: %s\n", CERT_FILE);
    printf("Key: %s\n", KEY_FILE);

    /* Create server socket */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen failed");
        close(server_sock);
        return 1;
    }

    printf("Listening on port %d...\n", PORT);

    /* Create NeutrinoSSL context */
    ctx = neutrinossl_ctx_new_server(CERT_FILE, KEY_FILE);
    if (!ctx) {
        fprintf(stderr, "Failed to create SSL context: %s\n", neutrinossl_get_error());
        close(server_sock);
        return 1;
    }
    printf("SSL context created\n");

    /* Accept connection */
    printf("Waiting for connection...\n");
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("accept failed");
        neutrinossl_ctx_free(ctx);
        close(server_sock);
        return 1;
    }
    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* Create SSL connection */
    ssl = neutrinossl_new(ctx, client_sock);
    if (!ssl) {
        fprintf(stderr, "Failed to create SSL connection: %s\n", neutrinossl_get_error());
        close(client_sock);
        neutrinossl_ctx_free(ctx);
        close(server_sock);
        return 1;
    }
    printf("SSL connection created\n");

    /* TLS handshake */
    printf("Starting TLS handshake...\n");
    ret = neutrinossl_accept(ssl);
    if (ret <= 0) {
        fprintf(stderr, "TLS handshake failed: %s\n", neutrinossl_get_error());
        neutrinossl_free(ssl);
        close(client_sock);
        neutrinossl_ctx_free(ctx);
        close(server_sock);
        return 1;
    }
    printf("✓ TLS handshake successful!\n");

    /* Try to read data */
    printf("Reading data...\n");
    ret = neutrinossl_read(ssl, buffer, sizeof(buffer) - 1);
    if (ret > 0) {
        buffer[ret] = '\0';
        printf("Received %d bytes: %s\n", ret, buffer);

        /* Echo back */
        const char *response = "Hello from NeutrinoTLS server!\n";
        neutrinossl_write(ssl, (void *)response, strlen(response));
        printf("Sent response\n");
    } else {
        printf("Read returned: %d\n", ret);
    }

    /* Cleanup */
    neutrinossl_free(ssl);
    close(client_sock);
    neutrinossl_ctx_free(ctx);
    close(server_sock);

    printf("=== Test completed ===\n");
    return 0;
}
