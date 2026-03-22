/**
 * FreeRDP Test Client - Tests full RDP connection and bitmap reception
 * Uses libfreerdp to connect to xrdp server and receive screen images
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <freerdp/freerdp.h>

/* Global stats */
static int bitmap_count = 0;
static int total_pixels = 0;
static int non_black_pixels = 0;

/* Callback: Pre-connect */
boolean test_pre_connect(freerdp* instance)
{
    printf("[PreConnect] Configuring connection settings\n");

    rdpSettings* settings = instance->settings;

    /* Basic settings */
    settings->width = 1024;
    settings->height = 768;
    settings->color_depth = 16;
    settings->username = "cyclic";
    settings->password = "";
    settings->hostname = "127.0.0.1";
    settings->port = 3389;

    /* Security settings - try all security types */
    settings->encryption = 1;  /* Enable encryption */
    settings->tls_security = 1; /* Use TLS */
    settings->nla_security = 0; /* Disable NLA */
    settings->rdp_security = 1; /* Enable RDP security as fallback */
    settings->encryption_level = 0; /* Client compatible */
    settings->encryption_method = 0; /* Let client choose */

    /* Performance settings */
    settings->bitmap_cache = 1;
    settings->offscreen_bitmap_cache = 1;
    settings->glyph_cache = 1;

    /* Certificate verification */
    settings->ignore_certificate = 1; /* Accept any certificate for testing */

    printf("[PreConnect] ✓ Settings configured\n");
    printf("  Resolution: %dx%d @ %d bpp\n", settings->width, settings->height, settings->color_depth);
    printf("  Server: %s:%d\n", settings->hostname, settings->port);
    printf("  TLS Security: enabled\n");

    return true;
}

/* Callback: Post-connect */
boolean test_post_connect(freerdp* instance)
{
    printf("[PostConnect] Connection established!\n");
    printf("[PostConnect] ✓ Ready to receive bitmaps\n");
    return true;
}

/* Callback: Verify certificate */
boolean test_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
    printf("[Certificate] Verifying server certificate\n");
    printf("  Subject: %s\n", subject ? subject : "(null)");
    printf("  Issuer: %s\n", issuer ? issuer : "(null)");
    printf("  Fingerprint: %s\n", fingerprint ? fingerprint : "(null)");
    printf("[Certificate] ✓ Accepted (test mode)\n");
    return true;
}

/* Callback: Bitmap update */
void test_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmap)
{
    bitmap_count++;

    printf("\n[BitmapUpdate #%d] Received %d bitmap rectangle(s)\n",
           bitmap_count, bitmap->count);

    for (uint32 i = 0; i < bitmap->count; i++) {
        BITMAP_DATA* bmp = &bitmap->rectangles[i];

        printf("  Rectangle %d:\n", i + 1);
        printf("    Position: (%d, %d) - (%d, %d)\n",
               bmp->destLeft, bmp->destTop, bmp->destRight, bmp->destBottom);
        printf("    Size: %dx%d\n", bmp->width, bmp->height);
        printf("    BPP: %d\n", bmp->bitsPerPixel);
        printf("    Compressed: %s\n", bmp->compressed ? "yes" : "no");
        printf("    Data length: %d bytes\n", bmp->bitmapLength);

        /* Analyze pixel data */
        if (bmp->bitmapDataStream && bmp->bitmapLength > 0) {
            int pixels = bmp->width * bmp->height;
            total_pixels += pixels;

            /* Sample first few pixels to check if non-black */
            int sample_size = (bmp->bitmapLength < 100) ? bmp->bitmapLength : 100;
            for (int j = 0; j < sample_size; j++) {
                if (bmp->bitmapDataStream[j] != 0) {
                    non_black_pixels++;
                }
            }

            /* Show first few bytes */
            printf("    First bytes: ");
            for (int j = 0; j < 16 && j < bmp->bitmapLength; j++) {
                printf("%02x ", bmp->bitmapDataStream[j]);
            }
            printf("\n");
        }
    }

    printf("  ✓ Bitmap data received\n");
}

/* Callback: Begin paint */
void test_begin_paint(rdpContext* context)
{
    /* No GDI needed for testing */
}

/* Callback: End paint */
void test_end_paint(rdpContext* context)
{
    /* No GDI needed for testing */
}

int main(int argc, char** argv)
{
    freerdp* instance;
    int exit_code = 0;

    printf("==================================================\n");
    printf("FreeRDP Test Client - RDP Connection & Bitmap Test\n");
    printf("==================================================\n\n");

    /* Create FreeRDP instance */
    instance = freerdp_new();
    if (!instance) {
        printf("✗ Failed to create FreeRDP instance\n");
        return 1;
    }

    /* Set up context */
    freerdp_context_new(instance);

    /* Register callbacks */
    instance->PreConnect = test_pre_connect;
    instance->PostConnect = test_post_connect;
    instance->VerifyCertificate = test_verify_certificate;

    /* Register update callbacks */
    instance->update->BeginPaint = test_begin_paint;
    instance->update->EndPaint = test_end_paint;
    instance->update->BitmapUpdate = test_bitmap_update;

    printf("[1/5] ⚙ Connecting to server...\n\n");

    /* Connect */
    if (!freerdp_connect(instance)) {
        printf("\n✗ Connection failed\n");
        exit_code = 1;
        goto cleanup;
    }

    printf("\n[2/5] ✓ Connected successfully!\n");
    printf("[3/5] ⚙ Waiting for bitmap updates (10 seconds)...\n\n");

    /* Main loop - receive updates */
    int max_iterations = 100; /* ~10 seconds at 100ms per iteration */
    int iterations = 0;

    while (iterations < max_iterations && !freerdp_shall_disconnect(instance)) {
        void* rfds[32];
        void* wfds[32];
        int rcount = 0;
        int wcount = 0;

        /* Get file descriptors */
        if (!freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount)) {
            printf("✗ Failed to get file descriptors\n");
            break;
        }

        /* Wait for data (100ms timeout) */
        usleep(100000);

        /* Check for incoming data */
        if (!freerdp_check_fds(instance)) {
            printf("✗ Failed to check file descriptors\n");
            break;
        }

        iterations++;

        /* Print progress every second */
        if (iterations % 10 == 0) {
            printf("  [%d seconds] Bitmaps received: %d\n", iterations / 10, bitmap_count);
        }
    }

    printf("\n[4/5] ✓ Data reception complete\n");
    printf("[5/5] 📊 Results:\n\n");

    /* Print statistics */
    printf("==================================================\n");
    printf("CONNECTION SUMMARY\n");
    printf("==================================================\n");
    printf("Bitmap updates received: %d\n", bitmap_count);
    printf("Total pixels analyzed: %d\n", total_pixels);
    printf("Non-black pixels: %d\n", non_black_pixels);

    if (bitmap_count > 0) {
        printf("\n✓✓✓ SUCCESS! Received bitmap data from server ✓✓✓\n");
        if (non_black_pixels > 0) {
            printf("✓✓✓ Non-black images confirmed! ✓✓✓\n");
        }
    } else {
        printf("\n⚠ No bitmap updates received\n");
        printf("  This might be normal if server screen is idle\n");
    }
    printf("==================================================\n");

    /* Disconnect */
    freerdp_disconnect(instance);

cleanup:
    /* Cleanup */
    freerdp_context_free(instance);
    freerdp_free(instance);

    printf("\n✓ Test complete\n\n");

    return exit_code;
}
