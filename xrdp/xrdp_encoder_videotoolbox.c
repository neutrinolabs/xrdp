//
// Created by cheesekun on 5/13/22.
//

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <VideoToolbox/VideoToolbox.h>

#include "xrdp_encoder_videotoolbox.h"

#include "os_calls.h"
#include "log.h"

static const uint8_t H264_NALU_START_BYTES[4] = {
    0x00, 0x00, 0x00, 0x01
};

struct videotoolbox_encoder
{
    VTCompressionSessionRef compression_session;
    short out_width; short out_height;
    
    tintptr wait_until_frame_processed;
    CMSampleBufferRef output_buffer;
};


/*****************************************************************************/
void
xrdp_encoder_videotoolbox_output_callback(void *maybe_encoder,
                                          void *maybe_input_frame,
                                          OSStatus status,
                                          VTEncodeInfoFlags info_flags,
                                          CMSampleBufferRef output_buffer)
{
    struct videotoolbox_encoder *encoder = maybe_encoder;
    
    LOG(LOG_LEVEL_TRACE, "frame handler called with OSStatus %d", status);
    
    if (status != noErr) {
        LOG(
            LOG_LEVEL_WARNING,
            "VTCompressionOutputHandler returned OSStatus %d",
            status
        );
    }
    
    CMSampleBufferCreateCopy(
        kCFAllocatorDefault,
        output_buffer, &encoder->output_buffer
    );
    g_set_wait_obj(encoder->wait_until_frame_processed);
}

CFDictionaryRef
xrdp_encoder_videotoolbox_create_encoder_specifications(void)
{
    return NULL;
}


/*****************************************************************************/
void
xrdp_encoder_videotoolbox_set_default_properties(VTCompressionSessionRef compression_session)
{
    const CFStringRef profileLevel = kVTProfileLevel_H264_Main_4_1;
    
    // 2.5Mbps?
    const int averageBitrate = 1024 * 1024 * 2.5;
    const int expectedFrameRate = 60;
    const int maxKeyframe = 1;
    const int maxFrameDelay = 0;
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_ProfileLevel, profileLevel
    );
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_AverageBitRate,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &averageBitrate)
    );
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_ExpectedFrameRate,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &expectedFrameRate)
    );
    
    /*
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_MaxKeyFrameInterval,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maxKeyframe)
    );
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_AllowFrameReordering,
        kCFBooleanFalse
    );
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_AllowTemporalCompression,
        kCFBooleanFalse
    );
     */
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_MaxFrameDelayCount,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maxFrameDelay)
    );
    
    VTSessionSetProperty(
        compression_session,
        kVTCompressionPropertyKey_RealTime,
        kCFBooleanTrue
    );
}

/*****************************************************************************/
OSStatus
xrdp_encoder_videotoolbox_initialize_session(struct videotoolbox_encoder *encoder,
                                             int width, int height)
{
    CFDictionaryRef encoder_specifications = xrdp_encoder_videotoolbox_create_encoder_specifications();
    
    OSStatus retval = VTCompressionSessionCreate(
        kCFAllocatorDefault,
        width, height,
        kCMVideoCodecType_H264,
        encoder_specifications, nil,
        nil,
        &xrdp_encoder_videotoolbox_output_callback, (void *) encoder,
        &encoder->compression_session
    );
    
    if (retval != noErr) {
        LOG(
            LOG_LEVEL_ERROR,
            "couldn't create compression session: VTCompressionSessionCreate returned OSStatus %d",
            retval
        );
    } else {
        xrdp_encoder_videotoolbox_set_default_properties(encoder->compression_session);
    }
    
    if (encoder_specifications != NULL) {
        CFRelease(encoder_specifications);
    }
    
    return retval;
}

/*****************************************************************************/
void
xrdp_encoder_videotoolbox_release_session(struct videotoolbox_encoder *encoder)
{
    if (encoder->compression_session == NULL) {
        return;
    }
    
    VTCompressionSessionInvalidate(encoder->compression_session);
    encoder->compression_session = NULL;
}

OSStatus
xrdp_encoder_videotoolbox_encode_frame_sync(struct videotoolbox_encoder *encoder,
                                            CVImageBufferRef input_frame,
                                            CMTime timestamp,
                                            CMSampleBufferRef output_frame)
{
    OSStatus retval = 0;
    VTEncodeInfoFlags info_flags = 0;
    
    g_reset_wait_obj(encoder->wait_until_frame_processed);
    
    retval = VTCompressionSessionEncodeFrame(
        encoder->compression_session,
        input_frame,
        timestamp,
        kCMTimeInvalid,
        NULL,
        input_frame,
        &info_flags
    );
    
    if (retval != noErr) {
        LOG(
            LOG_LEVEL_WARNING,
            "VTCompressionSessionEncodeFrameWithOutputHandler() returned OSStatus %d",
            retval
        );
        return retval;
    }
    
    if ((info_flags & kVTEncodeInfo_Asynchronous) != 0) {
        LOG(LOG_LEVEL_TRACE, "waiting until frame is ready");
        g_obj_wait(&encoder->wait_until_frame_processed, 1, nil, 0, -1);
    }
    
    if ((info_flags & kVTEncodeInfo_FrameDropped) != 0) {
        LOG(LOG_LEVEL_DEBUG, "frame dropped!");
    }
    
    return retval;
}

/*****************************************************************************/
size_t
xrdp_encoder_videotoolbox_write_sps_pps_into_stream(void *output_buffer,
                                                    CMSampleBufferRef frame)
{
    size_t param_size = 0;
    const uint8_t *param;
    
    size_t offset = 0;
    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(frame);
    
    for (int i = 0; i < 2; i++) {
        CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
                format,
                i,
                &param,
                &param_size,
                nil,
                nil
        );
        g_memcpy(output_buffer + offset, H264_NALU_START_BYTES, 4);
        offset += 4;
        
        g_memcpy(output_buffer + offset, param, param_size);
        offset += param_size;
    }
    
    return offset;
}


/*****************************************************************************/
/**
 * converts AVCC-formatted NALUs like..
 *   00 00 12 34 .. .. .. .. .. ..
 * [unit length] [unit data (0x1234 bytes)]
 *
 * .. to Annex B format
 *   00 00 00 01 .. .. .. .. .. ..
 * [start bytes] [nal unit data]
 */
size_t
xrdp_encoder_videotoolbox_avcc_to_annex_b(const void *in_avcc_nalus,
                                          size_t in_avcc_nalus_length,
                                          void *out_annex_b_nalus)
{
    
    uint32_t unit_length = 0;
    size_t pos = 0;
    
    g_memcpy(out_annex_b_nalus, in_avcc_nalus, in_avcc_nalus_length);
    
    while (pos < in_avcc_nalus_length) {
        
        memcpy(&unit_length, in_avcc_nalus + pos, sizeof(uint32_t));
#if defined(LITTLE_ENDIAN)
        unit_length = CFSwapInt32(unit_length);
#endif
        
        if (unit_length <= 0) {
            break;
        }
        
        memcpy(out_annex_b_nalus + pos, H264_NALU_START_BYTES, 4);
        pos += (4 + unit_length);
    }
    
    return pos;
}


/*****************************************************************************/
void *
xrdp_encoder_videotoolbox_create(int width, int height)
{
    struct videotoolbox_encoder *encoder;
    
    LOG(LOG_LEVEL_DEBUG, "creating videotoolbox encoder");
    encoder = (struct videotoolbox_encoder *) g_malloc(sizeof(struct videotoolbox_encoder), 1);
    
    if (encoder == NULL) {
        LOG(LOG_LEVEL_WARNING, "could not allocate memory");
        return NULL;
    }
    
    xrdp_encoder_videotoolbox_initialize_session(
        encoder,
        width, height
    );
    
    return encoder;
}

/*****************************************************************************/
int
xrdp_encoder_videotoolbox_delete(void *handle)
{
    struct videotoolbox_encoder *encoder = handle;
    
    if (encoder == NULL) {
        LOG(
            LOG_LEVEL_WARNING,
            "couldn't release encoder object: is *handle instance of VideoToolboxEncoder?"
        );
    
        return 1;
    }
   
    LOG(LOG_LEVEL_INFO, "releasing encoder object");
    xrdp_encoder_videotoolbox_release_session(encoder);
    g_free(encoder);
    
    return 0;
}


int
xrdp_encoder_videotoolbox_encode(void *handle, int session,
                                 int frame_id,
                                 int width, int height,
                                 int format, const char *data,
                                 char *cdata, int *cdata_bytes) {

    struct videotoolbox_encoder *encoder = handle;

    CVPixelBufferRef input_frame = nil;
    CMSampleBufferRef output_frame = nil;

    uint8_t *output_frame_ptr = nil;
    size_t output_frame_length = 0;

    size_t offset = 0;

    OSStatus retval = noErr;

    CMTime timestamp = CMTimeMake(
        (int64_t) g_time3(), 1000
    );

    if (encoder == NULL) {
        LOG(LOG_LEVEL_WARNING, "couldn't call encode function: is *handle instance of VideoToolboxEncoder?");
        return -1;
    }

    retval = CVPixelBufferCreateWithBytes(
            kCFAllocatorDefault,
            width, height,
            kCVPixelFormatType_32BGRA,
            (void *) data, width * 4,
            nil, nil,
            nil, &input_frame
    );

    if (retval != noErr) {
        LOG(LOG_LEVEL_ERROR, "CVPixelBufferCreateWithBytes returned OSStatus %d", retval);
        return retval;
    }

    retval = xrdp_encoder_videotoolbox_encode_frame_sync(
            encoder,
            input_frame,
            timestamp,
            output_frame
    );

    if (retval != noErr) {
        goto ENCODE_END;
    }

    CMBlockBufferGetDataPointer(
            CMSampleBufferGetDataBuffer(output_frame),
            0, nil,
            &output_frame_length, &output_frame_ptr
    );


    offset += xrdp_encoder_videotoolbox_write_sps_pps_into_stream(
            cdata + offset,
            output_frame
    );
    offset += xrdp_encoder_videotoolbox_avcc_to_annex_b(
            output_frame_ptr, output_frame_length,
            cdata + offset
    );

    *cdata_bytes = (int) offset;

    ENCODE_END:
    CVPixelBufferRelease(input_frame);
    return retval;
}
