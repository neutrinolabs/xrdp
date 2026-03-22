/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026 Neutrinos Software Corporation
 * Some portions Classify(r)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file macos_capture.m
 * @brief Native macOS screen capture implementation using ScreenCaptureKit
 */

#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <IOSurface/IOSurface.h>
#import <CoreMedia/CoreMedia.h>

#include "macos_capture.h"

/* ScreenCaptureKit stream delegate */
@interface MacOSCaptureDelegate : NSObject <SCStreamDelegate, SCStreamOutput>
{
    @public
    struct mod_macos* mod;
    CVPixelBufferRef currentFrame;
    dispatch_semaphore_t frameSemaphore;
}
@end

@implementation MacOSCaptureDelegate

- (instancetype)initWithMod:(struct mod_macos*)m
{
    self = [super init];
    if (self)
    {
        mod = m;
        currentFrame = NULL;
        frameSemaphore = dispatch_semaphore_create(1);
    }
    return self;
}

- (void)dealloc
{
    if (currentFrame)
    {
        CVPixelBufferRelease(currentFrame);
    }
}

/* SCStreamDelegate methods */
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
    if (error)
    {
        LOG(LOG_LEVEL_ERROR, "ScreenCaptureKit stream stopped with error: %s",
            [[error localizedDescription] UTF8String]);
    }
}

/* SCStreamOutput methods */
- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
    ofType:(SCStreamOutputType)type
{
    if (type != SCStreamOutputTypeScreen)
    {
        return;
    }

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer)
    {
        return;
    }

    dispatch_semaphore_wait(frameSemaphore, DISPATCH_TIME_FOREVER);

    if (currentFrame)
    {
        CVPixelBufferRelease(currentFrame);
    }

    currentFrame = CVPixelBufferRetain((CVPixelBufferRef)imageBuffer);
    mod->has_damage = 1;

    dispatch_semaphore_signal(frameSemaphore);
}

- (CVPixelBufferRef)getCurrentFrame
{
    dispatch_semaphore_wait(frameSemaphore, DISPATCH_TIME_FOREVER);
    CVPixelBufferRef frame = currentFrame ? CVPixelBufferRetain(currentFrame) : NULL;
    dispatch_semaphore_signal(frameSemaphore);
    return frame;
}

@end

/* Capture context structure */
struct macos_capture_context
{
    SCStream* stream;
    MacOSCaptureDelegate* delegate;
    SCShareableContent* content;
    dispatch_queue_t captureQueue;
};

/**
 * Initialize macOS capture subsystem
 */
int
macos_capture_init(struct mod_macos* mod)
{
    struct macos_capture_context* ctx;

    LOG(LOG_LEVEL_INFO, "Initializing macOS native capture");

    /* Check macOS version */
    if (@available(macOS 12.3, *))
    {
        /* ScreenCaptureKit is available */
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "macOS 12.3 or later required for ScreenCaptureKit");
        return 1;
    }

    ctx = (struct macos_capture_context*)g_malloc(sizeof(struct macos_capture_context), 1);
    if (ctx == NULL)
    {
        return 1;
    }

    /* Create capture queue */
    ctx->captureQueue = dispatch_queue_create("com.xrdp.macos.capture",
                                              DISPATCH_QUEUE_SERIAL);

    /* Create delegate */
    ctx->delegate = [[MacOSCaptureDelegate alloc] initWithMod:mod];

    mod->capture_context = ctx;
    mod->has_damage = 0;

    LOG(LOG_LEVEL_INFO, "macOS capture initialized successfully");

    return 0;
}

/**
 * Start screen capture
 */
int
macos_capture_start(struct mod_macos* mod)
{
    struct macos_capture_context* ctx = (struct macos_capture_context*)mod->capture_context;
    __block int result = 0;

    if (ctx == NULL)
    {
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "Starting macOS screen capture at %dx%d", mod->width, mod->height);

    if (@available(macOS 12.3, *))
    {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        /* Get shareable content (displays) */
        [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent* content, NSError* error)
        {
            if (error)
            {
                LOG(LOG_LEVEL_ERROR, "Failed to get shareable content: %s",
                    [[error localizedDescription] UTF8String]);
                result = 1;
                dispatch_semaphore_signal(sem);
                return;
            }

            ctx->content = content;

            if ([content.displays count] == 0)
            {
                LOG(LOG_LEVEL_ERROR, "No displays found");
                result = 1;
                dispatch_semaphore_signal(sem);
                return;
            }

            /* Use main display */
            SCDisplay* display = content.displays[0];

            /* Configure stream */
            SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
            config.width = mod->width;
            config.height = mod->height;
            config.minimumFrameInterval = CMTimeMake(1, 30); // 30 FPS
            config.pixelFormat = kCVPixelFormatType_32BGRA;
            config.showsCursor = YES;
            config.capturesAudio = NO;
            config.scalesToFit = YES;

            /* Create content filter for main display */
            SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:display
                                                            excludingWindows:@[]];

            /* Create stream */
            NSError* streamError = nil;
            ctx->stream = [[SCStream alloc] initWithFilter:filter
                                             configuration:config
                                                  delegate:ctx->delegate];

            if (streamError)
            {
                LOG(LOG_LEVEL_ERROR, "Failed to create stream: %s",
                    [[streamError localizedDescription] UTF8String]);
                /* ARC handles cleanup */
                result = 1;
                dispatch_semaphore_signal(sem);
                return;
            }

            /* Add stream output */
            [ctx->stream addStreamOutput:ctx->delegate
                                    type:SCStreamOutputTypeScreen
                     sampleHandlerQueue:ctx->captureQueue
                                   error:&streamError];

            if (streamError)
            {
                LOG(LOG_LEVEL_ERROR, "Failed to add stream output: %s",
                    [[streamError localizedDescription] UTF8String]);
                /* ARC handles cleanup */
                result = 1;
                dispatch_semaphore_signal(sem);
                return;
            }

            /* Start capture */
            [ctx->stream startCaptureWithCompletionHandler:^(NSError* error)
            {
                if (error)
                {
                    LOG(LOG_LEVEL_ERROR, "Failed to start capture: %s",
                        [[error localizedDescription] UTF8String]);
                    result = 1;
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "Screen capture started successfully");
                }

                /* ARC handles cleanup */
                dispatch_semaphore_signal(sem);
            }];
        }];

        /* Wait for completion */
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        /* ARC handles semaphore cleanup */
    }

    return result;
}

/**
 * Stop screen capture
 */
int
macos_capture_stop(struct mod_macos* mod)
{
    struct macos_capture_context* ctx = (struct macos_capture_context*)mod->capture_context;

    if (ctx == NULL || ctx->stream == NULL)
    {
        return 0;
    }

    LOG(LOG_LEVEL_INFO, "Stopping macOS screen capture");

    if (@available(macOS 12.3, *))
    {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        [ctx->stream stopCaptureWithCompletionHandler:^(NSError* error)
        {
            if (error)
            {
                LOG(LOG_LEVEL_ERROR, "Error stopping capture: %s",
                    [[error localizedDescription] UTF8String]);
            }
            dispatch_semaphore_signal(sem);
        }];

        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        /* ARC handles semaphore cleanup */

        ctx->stream = nil;
    }

    return 0;
}

/**
 * Get current frame and send to client
 */
int
macos_capture_get_frame(struct mod_macos* mod)
{
    struct macos_capture_context* ctx = (struct macos_capture_context*)mod->capture_context;
    CVPixelBufferRef frame;
    void* baseAddress;
    size_t bytesPerRow;
    size_t width, height;
    int rv = 0;

    if (ctx == NULL || ctx->delegate == NULL)
    {
        return 1;
    }

    /* Get current frame */
    frame = [ctx->delegate getCurrentFrame];
    if (frame == NULL)
    {
        return 0; /* No frame available yet */
    }

    /* Lock pixel buffer */
    CVPixelBufferLockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);

    baseAddress = CVPixelBufferGetBaseAddress(frame);
    bytesPerRow = CVPixelBufferGetBytesPerRow(frame);
    width = CVPixelBufferGetWidth(frame);
    height = CVPixelBufferGetHeight(frame);

    /* Send frame to client */
    if (mod->server_begin_update && mod->server_paint_rect && mod->server_end_update)
    {
        mod->server_begin_update(mod);

        /* Send full screen update */
        rv = mod->server_paint_rect(mod, 0, 0, width, height,
                                     (char*)baseAddress, width, height,
                                     0, 0);

        mod->server_end_update(mod);
    }

    /* Unlock and release */
    CVPixelBufferUnlockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferRelease(frame);

    mod->has_damage = 0;

    return rv;
}

/**
 * Cleanup capture subsystem
 */
int
macos_capture_deinit(struct mod_macos* mod)
{
    struct macos_capture_context* ctx = (struct macos_capture_context*)mod->capture_context;

    if (ctx == NULL)
    {
        return 0;
    }

    LOG(LOG_LEVEL_INFO, "Deinitializing macOS capture");

    macos_capture_stop(mod);

    /* ARC handles object cleanup automatically */
    ctx->content = nil;
    ctx->delegate = nil;
    ctx->captureQueue = nil;

    g_free(ctx);
    mod->capture_context = NULL;

    return 0;
}
