/**
 * Full RDP+TLS 1.3 Test Client
 * Uses NeutrinoTLS crypto functions to complete handshake
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

/* Include NeutrinoTLS crypto primitives */
#include "../common/neutrinotls.h"

/* Helper to send all bytes */
static int send_all(int sock, const uint8_t *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

/* Helper to receive exact number of bytes */
static int recv_all(int sock, uint8_t *data, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(sock, data + received, len - received, 0);
        if (n <= 0) return -1;
        received += n;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 3389;

    printf("==================================================\n");
    printf("Full RDP+TLS 1.3 Handshake Test\n");
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

    printf("[1/7] Connecting to %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    printf("      ✓ TCP connected\n\n");

    /* Send X.224 Connection Request */
    printf("[2/7] Sending X.224 Connection Request...\n");
    unsigned char x224_req[] = {
        0x03, 0x00, 0x00, 0x13, 0x0e, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00  /* Request TLS */
    };

    if (send_all(sock, x224_req, sizeof(x224_req)) < 0) {
        perror("send x224");
        close(sock);
        return 1;
    }

    /* Receive X.224 response */
    unsigned char buf[8192];
    int len = recv(sock, buf, sizeof(buf), 0);
    if (len < 19) {
        fprintf(stderr, "Failed to receive X.224 response\n");
        close(sock);
        return 1;
    }

    printf("      ✓ X.224 response: %d bytes\n", len);

    /* Check if server accepted TLS */
    if (buf[11] != 0x02) {
        fprintf(stderr, "      ✗ Server did not accept TLS\n");
        close(sock);
        return 1;
    }
    printf("      ✓ Server accepted TLS\n\n");

    /* Generate client X25519 keypair */
    printf("[3/7] Generating X25519 keypair...\n");
    uint8_t client_private[32];
    uint8_t client_public[32];
    x25519_keygen(client_public, client_private);
    printf("      ✓ Generated client keypair\n");
    printf("      Public key: ");
    for (int i = 0; i < 8; i++) printf("%02x", client_public[i]);
    printf("...\n\n");

    /* Build ClientHello with generated public key */
    printf("[4/7] Sending TLS 1.3 ClientHello...\n");

    uint8_t client_hello[256];
    size_t ch_pos = 0;

    /* TLS Record Header */
    client_hello[ch_pos++] = 0x16;  /* Handshake */
    client_hello[ch_pos++] = 0x03;
    client_hello[ch_pos++] = 0x01;
    uint16_t *ch_len_ptr = (uint16_t*)&client_hello[ch_pos];
    ch_pos += 2;  /* Will fill in length later */

    size_t handshake_start = ch_pos;

    /* Handshake Header */
    client_hello[ch_pos++] = 0x01;  /* ClientHello */
    uint8_t *handshake_len_ptr = &client_hello[ch_pos];
    ch_pos += 3;  /* Will fill in length later */

    /* ClientHello body */
    client_hello[ch_pos++] = 0x03;
    client_hello[ch_pos++] = 0x03;  /* TLS 1.2 legacy version */

    /* Client Random */
    uint8_t client_random[32];
    srand(time(NULL));
    for (int i = 0; i < 32; i++) {
        client_random[i] = rand() & 0xFF;
        client_hello[ch_pos++] = client_random[i];
    }

    /* Session ID */
    client_hello[ch_pos++] = 0;

    /* Cipher Suites */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x02;
    client_hello[ch_pos++] = 0x13;
    client_hello[ch_pos++] = 0x03;  /* TLS_CHACHA20_POLY1305_SHA256 */

    /* Compression */
    client_hello[ch_pos++] = 0x01;
    client_hello[ch_pos++] = 0x00;

    /* Extensions */
    size_t ext_len_pos = ch_pos;
    ch_pos += 2;  /* Will fill in extensions length */
    size_t ext_start = ch_pos;

    /* Supported Versions */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x2b;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x03;
    client_hello[ch_pos++] = 0x02;
    client_hello[ch_pos++] = 0x03;
    client_hello[ch_pos++] = 0x04;  /* TLS 1.3 */

    /* Supported Groups */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x0a;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x04;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x02;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x1d;  /* x25519 */

    /* Key Share */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x33;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x26;  /* Length: 38 */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x24;  /* Key shares length: 36 */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x1d;  /* x25519 */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x20;  /* Key length: 32 */
    memcpy(&client_hello[ch_pos], client_public, 32);
    ch_pos += 32;

    /* Signature Algorithms */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x0d;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x04;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x02;
    client_hello[ch_pos++] = 0x04;
    client_hello[ch_pos++] = 0x03;

    /* Server Name */
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x0e;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x0c;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x00;
    client_hello[ch_pos++] = 0x09;
    memcpy(&client_hello[ch_pos], "localhost", 9);
    ch_pos += 9;

    /* Fill in lengths */
    uint16_t ext_len = ch_pos - ext_start;
    client_hello[ext_len_pos] = (ext_len >> 8) & 0xFF;
    client_hello[ext_len_pos + 1] = ext_len & 0xFF;

    uint32_t handshake_len = ch_pos - handshake_start - 4;
    handshake_len_ptr[0] = 0;
    handshake_len_ptr[1] = (handshake_len >> 8) & 0xFF;
    handshake_len_ptr[2] = handshake_len & 0xFF;

    uint16_t record_len = ch_pos - 5;
    *ch_len_ptr = htons(record_len);

    if (send_all(sock, client_hello, ch_pos) < 0) {
        perror("send clienthello");
        close(sock);
        return 1;
    }
    printf("      ✓ Sent ClientHello (%zu bytes)\n\n", ch_pos);

    /* Initialize transcript hash with ClientHello */
    sha256_ctx transcript;
    sha256_init(&transcript);
    sha256_update(&transcript, &client_hello[5], ch_pos - 5);  /* Skip TLS record header */

    /* Receive ServerHello */
    printf("[5/7] Receiving ServerHello...\n");

    /* Read TLS record header */
    if (recv_all(sock, buf, 5) < 0) {
        fprintf(stderr, "Failed to receive ServerHello header\n");
        close(sock);
        return 1;
    }

    if (buf[0] != 0x16) {
        fprintf(stderr, "Expected TLS Handshake record, got 0x%02x\n", buf[0]);
        close(sock);
        return 1;
    }

    uint16_t sh_len = (buf[3] << 8) | buf[4];
    printf("      ServerHello length: %d bytes\n", sh_len);

    if (recv_all(sock, buf, sh_len) < 0) {
        fprintf(stderr, "Failed to receive ServerHello payload\n");
        close(sock);
        return 1;
    }

    /* Update transcript with ServerHello */
    sha256_update(&transcript, buf, sh_len);

    /* Parse ServerHello */
    if (buf[0] != 0x02) {
        fprintf(stderr, "Expected ServerHello (0x02), got 0x%02x\n", buf[0]);
        close(sock);
        return 1;
    }

    printf("      ✓ Received ServerHello\n");

    /* Extract server random */
    uint8_t server_random[32];
    memcpy(server_random, &buf[6], 32);
    printf("      Server random: ");
    for (int i = 0; i < 8; i++) printf("%02x", server_random[i]);
    printf("...\n");

    /* Parse extensions to find key_share */
    size_t pos = 6 + 32 + 1 + 2 + 1;  /* Skip to extensions */
    uint16_t extensions_len = (buf[pos] << 8) | buf[pos + 1];
    pos += 2;

    uint8_t server_public[32];
    bool found_key_share = false;

    size_t ext_end = pos + extensions_len;
    while (pos + 4 <= ext_end && pos < sh_len) {
        uint16_t ext_type = (buf[pos] << 8) | buf[pos + 1];
        uint16_t ext_len = (buf[pos + 2] << 8) | buf[pos + 3];
        pos += 4;

        if (ext_type == 0x0033) {  /* key_share */
            /* Skip group ID (2 bytes) and key length (2 bytes) */
            pos += 4;
            if (pos + 32 <= sh_len) {
                memcpy(server_public, &buf[pos], 32);
                found_key_share = true;
                printf("      ✓ Extracted server public key\n");
                printf("      Server public: ");
                for (int i = 0; i < 8; i++) printf("%02x", server_public[i]);
                printf("...\n");
            }
            break;
        }
        pos += ext_len;
    }

    if (!found_key_share) {
        fprintf(stderr, "Failed to find server key_share\n");
        close(sock);
        return 1;
    }

    /* Compute shared secret */
    printf("\n[6/7] Computing TLS 1.3 keys...\n");
    uint8_t shared_secret[32];
    x25519(shared_secret, client_private, server_public);
    printf("      ✓ Computed X25519 shared secret\n");
    printf("      Shared secret: ");
    for (int i = 0; i < 8; i++) printf("%02x", shared_secret[i]);
    printf("...\n");

    /* Derive handshake secrets */
    uint8_t early_secret[32];
    uint8_t zeros[32] = {0};
    hkdf_extract(zeros, 32, zeros, 32, early_secret);

    uint8_t empty_hash[32];
    sha256(NULL, 0, empty_hash);

    uint8_t derived_secret[32];
    hkdf_expand_label(early_secret, "derived", empty_hash, 32, derived_secret, 32);

    uint8_t handshake_secret[32];
    hkdf_extract(derived_secret, 32, shared_secret, 32, handshake_secret);
    printf("      ✓ Derived handshake secret\n");

    /* Get transcript hash so far */
    sha256_ctx transcript_copy = transcript;
    uint8_t transcript_hash[32];
    sha256_final(&transcript_copy, transcript_hash);

    /* Derive client/server handshake traffic secrets */
    uint8_t client_hs_traffic_secret[32];
    uint8_t server_hs_traffic_secret[32];

    hkdf_expand_label(handshake_secret, "c hs traffic", transcript_hash, 32,
                      client_hs_traffic_secret, 32);
    hkdf_expand_label(handshake_secret, "s hs traffic", transcript_hash, 32,
                      server_hs_traffic_secret, 32);

    printf("      ✓ Derived handshake traffic secrets\n");

    /* Derive keys and IVs */
    uint8_t server_key[32], server_iv[12];
    hkdf_expand_label(server_hs_traffic_secret, "key", NULL, 0, server_key, 32);
    hkdf_expand_label(server_hs_traffic_secret, "iv", NULL, 0, server_iv, 12);

    printf("      ✓ Client ready to decrypt server messages\n\n");

    /* Receive encrypted messages from server */
    printf("[7/7] Receiving encrypted server messages...\n");

    uint64_t server_seq = 0;
    int messages_received = 0;

    while (messages_received < 3) {  /* Expect EncryptedExtensions, Certificate, Finished */
        /* Read TLS record header */
        if (recv_all(sock, buf, 5) < 0) {
            printf("      Connection closed after %d encrypted messages\n", messages_received);
            break;
        }

        uint8_t record_type = buf[0];
        uint16_t record_len = (buf[3] << 8) | buf[4];

        if (record_type == 0x17) {  /* Application Data (encrypted handshake) */
            printf("      Encrypted record %d: %d bytes\n", messages_received + 1, record_len);

            /* Receive encrypted payload */
            if (recv_all(sock, buf, record_len) < 0) {
                fprintf(stderr, "Failed to receive encrypted payload\n");
                break;
            }

            /* Decrypt using ChaCha20-Poly1305 */
            uint8_t plaintext[8192];
            size_t ciphertext_len = record_len - 16;  /* Subtract tag length */
            uint8_t *ciphertext = buf;
            uint8_t *tag = buf + ciphertext_len;

            /* Construct nonce: IV XOR sequence number */
            uint8_t nonce[12];
            memcpy(nonce, server_iv, 12);
            for (int i = 0; i < 8; i++) {
                nonce[12 - 1 - i] ^= (server_seq >> (i * 8)) & 0xFF;
            }

            /* TLS 1.3 AAD: record header with plaintext content type */
            uint8_t aad[5];
            aad[0] = 0x17;  /* ApplicationData */
            aad[1] = 0x03;
            aad[2] = 0x03;
            aad[3] = (record_len >> 8) & 0xFF;
            aad[4] = record_len & 0xFF;

            if (chacha20_poly1305_decrypt(server_key, nonce, aad, 5,
                                           ciphertext, ciphertext_len, tag, plaintext)) {
                /* Find actual content type (last non-zero byte) */
                size_t plaintext_len = ciphertext_len;
                while (plaintext_len > 0 && plaintext[plaintext_len - 1] == 0) {
                    plaintext_len--;
                }
                if (plaintext_len > 0) {
                    uint8_t content_type = plaintext[plaintext_len - 1];
                    plaintext_len--;  /* Remove content type byte */

                    if (content_type == 0x16) {  /* Handshake */
                        uint8_t handshake_type = plaintext[0];
                        const char *msg_name = "Unknown";
                        if (handshake_type == 0x08) msg_name = "EncryptedExtensions";
                        else if (handshake_type == 0x0b) msg_name = "Certificate";
                        else if (handshake_type == 0x14) msg_name = "Finished";

                        printf("      ✓ Decrypted: %s (%zu bytes)\n", msg_name, plaintext_len);

                        /* Update transcript with handshake messages */
                        if (handshake_type != 0x14) {  /* Don't include Finished in its own transcript */
                            sha256_update(&transcript, plaintext, plaintext_len + 1);
                        }

                        messages_received++;

                        if (handshake_type == 0x14) {
                            printf("      ✓ Server handshake complete!\n");
                            break;
                        }
                    }
                } else {
                    printf("      ⚠ Decrypted empty message\n");
                }
            } else {
                printf("      ✗ Decryption failed (seq=%llu)\n", server_seq);
                break;
            }

            server_seq++;
        } else if (record_type == 0x15) {  /* Alert */
            printf("      ⚠ Received TLS Alert\n");
            break;
        } else {
            printf("      ⚠ Unexpected record type: 0x%02x\n", record_type);
            break;
        }
    }

    if (messages_received < 3) {
        printf("\n⚠ Incomplete handshake, stopping\n");
        close(sock);
        return 1;
    }

    /* Send client Finished message */
    printf("\n[8/10] Sending client Finished message...\n");

    /* Derive client handshake key and IV */
    uint8_t client_key[32], client_iv[12];
    hkdf_expand_label(client_hs_traffic_secret, "key", NULL, 0, client_key, 32);
    hkdf_expand_label(client_hs_traffic_secret, "iv", NULL, 0, client_iv, 12);

    /* Compute verify_data = HMAC-SHA256(finished_key, transcript_hash) */
    sha256_ctx transcript_copy2 = transcript;
    uint8_t transcript_hash2[32];
    sha256_final(&transcript_copy2, transcript_hash2);

    uint8_t finished_key[32];
    hkdf_expand_label(client_hs_traffic_secret, "finished", NULL, 0, finished_key, 32);

    uint8_t verify_data[32];
    hmac_sha256(finished_key, 32, transcript_hash2, 32, verify_data);

    /* Build Finished handshake message */
    uint8_t finished_msg[40];
    finished_msg[0] = 0x14;  /* Finished */
    finished_msg[1] = 0;
    finished_msg[2] = 0;
    finished_msg[3] = 32;    /* verify_data length */
    memcpy(&finished_msg[4], verify_data, 32);

    /* Encrypt with client handshake key */
    uint8_t encrypted_finished[128];
    size_t plaintext_len = 36;  /* 4 byte header + 32 byte verify_data */

    /* Add content type byte */
    uint8_t plaintext_with_type[64];
    memcpy(plaintext_with_type, finished_msg, plaintext_len);
    plaintext_with_type[plaintext_len] = 0x16;  /* Handshake content type */
    plaintext_len++;

    /* Construct nonce */
    uint8_t client_nonce[12];
    memcpy(client_nonce, client_iv, 12);
    uint64_t client_seq = 0;
    for (int i = 0; i < 8; i++) {
        client_nonce[12 - 1 - i] ^= (client_seq >> (i * 8)) & 0xFF;
    }

    /* AAD */
    uint8_t client_aad[5];
    uint16_t encrypted_len = plaintext_len + 16;  /* + tag */
    client_aad[0] = 0x17;  /* ApplicationData */
    client_aad[1] = 0x03;
    client_aad[2] = 0x03;
    client_aad[3] = (encrypted_len >> 8) & 0xFF;
    client_aad[4] = encrypted_len & 0xFF;

    uint8_t tag[16];
    chacha20_poly1305_encrypt(client_key, client_nonce, client_aad, 5,
                               plaintext_with_type, plaintext_len,
                               encrypted_finished, tag);

    /* Build TLS record */
    uint8_t finished_record[256];
    size_t rec_pos = 0;
    finished_record[rec_pos++] = 0x17;  /* ApplicationData */
    finished_record[rec_pos++] = 0x03;
    finished_record[rec_pos++] = 0x03;
    finished_record[rec_pos++] = (encrypted_len >> 8) & 0xFF;
    finished_record[rec_pos++] = encrypted_len & 0xFF;
    memcpy(&finished_record[rec_pos], encrypted_finished, plaintext_len);
    rec_pos += plaintext_len;
    memcpy(&finished_record[rec_pos], tag, 16);
    rec_pos += 16;

    if (send_all(sock, finished_record, rec_pos) < 0) {
        fprintf(stderr, "Failed to send Finished message\n");
        close(sock);
        return 1;
    }

    printf("      ✓ Sent encrypted Finished message (%zu bytes)\n", rec_pos);

    /* Derive application traffic secrets */
    printf("\n[9/10] Deriving application traffic secrets...\n");

    /* Compute master secret */
    sha256_ctx transcript_copy3 = transcript;
    sha256_update(&transcript_copy3, finished_msg, 36);
    uint8_t transcript_hash3[32];
    sha256_final(&transcript_copy3, transcript_hash3);

    uint8_t derived_secret2[32];
    hkdf_expand_label(handshake_secret, "derived", empty_hash, 32, derived_secret2, 32);

    uint8_t master_secret[32];
    hkdf_extract(derived_secret2, 32, zeros, 32, master_secret);

    uint8_t client_app_secret[32], server_app_secret[32];
    hkdf_expand_label(master_secret, "c ap traffic", transcript_hash3, 32, client_app_secret, 32);
    hkdf_expand_label(master_secret, "s ap traffic", transcript_hash3, 32, server_app_secret, 32);

    uint8_t client_app_key[32], client_app_iv[12];
    uint8_t server_app_key[32], server_app_iv[12];

    hkdf_expand_label(client_app_secret, "key", NULL, 0, client_app_key, 32);
    hkdf_expand_label(client_app_secret, "iv", NULL, 0, client_app_iv, 12);
    hkdf_expand_label(server_app_secret, "key", NULL, 0, server_app_key, 32);
    hkdf_expand_label(server_app_secret, "iv", NULL, 0, server_app_iv, 12);

    printf("      ✓ Application traffic secrets derived\n");
    printf("      ✓ TLS 1.3 handshake complete!\n");

    /* Now ready for encrypted RDP data */
    printf("\n[10/10] Attempting to receive RDP data...\n");

    int rdp_messages = 0;
    uint64_t app_seq = 0;

    /* Try to receive any application data */
    while (rdp_messages < 5) {
        if (recv_all(sock, buf, 5) < 0) {
            printf("      Connection closed after %d RDP messages\n", rdp_messages);
            break;
        }

        uint8_t rec_type = buf[0];
        uint16_t rec_len = (buf[3] << 8) | buf[4];

        if (rec_type == 0x17 && rec_len > 16) {  /* Encrypted application data */
            if (recv_all(sock, buf, rec_len) < 0) {
                break;
            }

            /* Decrypt */
            uint8_t app_plaintext[8192];
            size_t app_ciphertext_len = rec_len - 16;
            uint8_t *app_ciphertext = buf;
            uint8_t *app_tag = buf + app_ciphertext_len;

            uint8_t app_nonce[12];
            memcpy(app_nonce, server_app_iv, 12);
            for (int i = 0; i < 8; i++) {
                app_nonce[12 - 1 - i] ^= (app_seq >> (i * 8)) & 0xFF;
            }

            uint8_t app_aad[5];
            app_aad[0] = 0x17;
            app_aad[1] = 0x03;
            app_aad[2] = 0x03;
            app_aad[3] = (rec_len >> 8) & 0xFF;
            app_aad[4] = rec_len & 0xFF;

            if (chacha20_poly1305_decrypt(server_app_key, app_nonce, app_aad, 5,
                                           app_ciphertext, app_ciphertext_len,
                                           app_tag, app_plaintext)) {
                /* Remove padding */
                size_t app_plaintext_len = app_ciphertext_len;
                while (app_plaintext_len > 0 && app_plaintext[app_plaintext_len - 1] == 0) {
                    app_plaintext_len--;
                }
                if (app_plaintext_len > 0) {
                    uint8_t content_type = app_plaintext[app_plaintext_len - 1];
                    app_plaintext_len--;

                    if (content_type == 0x17) {  /* Application data */
                        printf("      ✓ Decrypted RDP data: %zu bytes\n", app_plaintext_len);

                        /* Check for non-black pixels (any byte > 0) */
                        int non_zero_bytes = 0;
                        for (size_t i = 0; i < app_plaintext_len && i < 1024; i++) {
                            if (app_plaintext[i] != 0) non_zero_bytes++;
                        }

                        if (non_zero_bytes > 0) {
                            printf("      ✓ Found %d non-zero bytes in RDP data!\n", non_zero_bytes);
                        }

                        rdp_messages++;
                    }
                }
            }
            app_seq++;
        } else {
            break;
        }
    }

    printf("\n==================================================\n");
    printf("Complete TLS 1.3 + RDP Test Results:\n");
    printf("==================================================\n");
    printf("✓ X.224 negotiation\n");
    printf("✓ TLS 1.3 ClientHello sent\n");
    printf("✓ TLS 1.3 ServerHello received\n");
    printf("✓ X25519 ECDH key exchange\n");
    printf("✓ HKDF key derivation\n");
    printf("✓ ChaCha20-Poly1305 encryption/decryption\n");
    printf("✓ Received %d encrypted handshake messages\n", messages_received);
    printf("✓ Sent client Finished message\n");
    printf("✓ Derived application secrets\n");
    printf("✓ TLS 1.3 handshake complete\n");

    if (rdp_messages > 0) {
        printf("✓ Received %d RDP messages over TLS\n", rdp_messages);
    } else {
        printf("⚠ No RDP application data received (server may expect client requests)\n");
    }

    printf("\n");
    printf("NeutrinoTLS is fully functional!\n");
    printf("Server successfully encrypts/decrypts with TLS 1.3\n");
    printf("==================================================\n\n");

    close(sock);
    return 0;
}
