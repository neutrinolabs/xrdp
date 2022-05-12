#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>

#import "os_calls.h"
#import "log.h"

@implementation VideoToolboxEncoder: NSObject {
    VTCompressionSessionRef compressionSession;
    tintptr waitUntilFrameProcessed;
}

+(void) setMandatoryPropertiesFor: (VTCompressionSessionRef) compressionSession {
    const CFStringRef profileLevel = kVTProfileLevel_H264_Main_4_1;
    
    // 2.5Mbps?
    const int averageBitrate = 1024 * 1024 * 2.5;
    const int expectedFrameRate = 60;
    const int maxKeyframe = 1;
    const int maxFrameDelay = 0;
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_ProfileLevel, profileLevel
    );
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_AverageBitRate,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &averageBitrate)
    );
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_ExpectedFrameRate,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &expectedFrameRate)
    );
    
    /*
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_MaxKeyFrameInterval,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maxKeyframe)
    );
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_AllowFrameReordering,
        kCFBooleanFalse
    );
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_AllowTemporalCompression,
        kCFBooleanFalse
    );
     */
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_MaxFrameDelayCount,
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maxFrameDelay)
    );
    
    VTSessionSetProperty(
        compressionSession,
        kVTCompressionPropertyKey_RealTime,
        kCFBooleanTrue
    );
}


-(VideoToolboxEncoder *) init {
    self = [super init];
    
    self->compressionSession = nil;
    self->waitUntilFrameProcessed = g_create_wait_obj("vt_wait_until_frame_processed");
    
    return self;
}

-(void) dealloc {
    [super dealloc];
    
    g_delete_wait_obj(self->waitUntilFrameProcessed);
}

-(OSStatus) initializeCompressionSession: (int) width height: (int) height {
    
    OSStatus retval = VTCompressionSessionCreate(
        kCFAllocatorDefault,
        width, height,
        kCMVideoCodecType_H264,
        nil, nil,
        nil,
        nil, (__bridge void *) self,
        &self->compressionSession
    );
    
    if (retval != noErr) {
        LOG(
            LOG_LEVEL_ERROR,
            "couldn't create compression session: VTCompressionSessionCreate returned OSStatus %d",
            retval
        );
        
        return retval;
    }
    
    [VideoToolboxEncoder setMandatoryPropertiesFor: self->compressionSession];
    return 0;
}

-(OSStatus) encodeFrameSync: (CVImageBufferRef) buffer timestamp: (CMTime) timestamp outBuffer: (CMSampleBufferRef *) outBuffer {
    VTEncodeInfoFlags infoFlags = 0;
    __block OSStatus handlerRetval;
    
    g_reset_wait_obj(self->waitUntilFrameProcessed);
    
    OSStatus retval = VTCompressionSessionEncodeFrameWithOutputHandler(
        self->compressionSession,
        buffer,
        timestamp,
        kCMTimeInvalid,
        nil,
        &infoFlags,
        ^(OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef _outBuffer) {
            LOG(LOG_LEVEL_TRACE, "frame handler called with OSStatus %d", handlerRetval);
    
            handlerRetval = status;
            
            CMSampleBufferCreateCopy(kCFAllocatorDefault, _outBuffer, outBuffer);
            g_set_wait_obj(self->waitUntilFrameProcessed);
        }
    );

    if (retval != noErr) {
        LOG(
            LOG_LEVEL_WARNING,
            "VTCompressionSessionEncodeFrameWithOutputHandler() returned OSStatus %d",
            retval
        );
        return retval;
    }
    
    if (infoFlags & kVTEncodeInfo_Asynchronous) {
        LOG(LOG_LEVEL_TRACE, "waiting until frame is ready");
        g_obj_wait(&self->waitUntilFrameProcessed, 1, nil, 0, -1);
    }
    
    if (handlerRetval != noErr) {
        LOG(
            LOG_LEVEL_WARNING,
            "VTCompressionOutputHandler returned OSStatus %d",
            handlerRetval
        );
        return handlerRetval;
    }
    
    if ((infoFlags & kVTEncodeInfo_FrameDropped) != 0) {
        LOG(LOG_LEVEL_DEBUG, "frame dropped!");
    }
    
    return 0;
}

-(size_t) writeSPSPPSInto: (void *) buffer from: (CMSampleBufferRef) frame {
    const uint8_t naluStartBytes[4] = {
        0x00, 0x00, 0x00, 0x01
    };
    
    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(frame);
    
    size_t paramSize;
    const uint8_t *param;
    
    size_t offset = 0;
    
    for (int i = 0; i < 2; i++) {
        CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, i, &param, &paramSize, nil, nil);
        memcpy(buffer + offset, naluStartBytes, 4);
        offset += 4;
    
        memcpy(buffer + offset, param, paramSize);
        offset += paramSize;
    }
    
    return offset;
}

/**
 * converts AVCC-formatted NALUs like..
 *   00 00 12 34 .. .. .. .. .. ..
 * [unit length] [unit data (0x1234 bytes)]
 *
 * .. to Annex B format
 *   00 00 00 01 .. .. .. .. .. ..
 * [start bytes] [nal unit data]
 */
-(size_t) avccToAnnexB: (const void*) avccNalus length: (size_t) avccLength to: (void *) annexBNalus {
    const uint8_t naluStartBytes[4] = {
        0x00, 0x00, 0x00, 0x01
    };
    size_t pos = 0;

    g_memcpy(annexBNalus, avccNalus, avccLength);
    
    while (pos < avccLength) {
        uint32_t unitLength = 0;
        
        memcpy(&unitLength, avccNalus + pos, sizeof(uint32_t));
#if defined(LITTLE_ENDIAN)
        unitLength = CFSwapInt32(unitLength);
#endif
        
        if (unitLength <= 0) {
            break;
        }
    
        memcpy(annexBNalus + pos, naluStartBytes, 4);
        pos += (4 + unitLength);
    }
    
    return pos;
}

@end

void *
xrdp_encoder_videotoolbox_create(int width, int height) {
    [VideoToolboxEncoder initialize];
    VideoToolboxEncoder *encoder = [[VideoToolboxEncoder alloc] init];
    [encoder initializeCompressionSession:width height: height];
    
    LOG(LOG_LEVEL_INFO, "new encoder object created");
    
    return (void *) encoder;
}

int
xrdp_encoder_videotoolbox_delete(void *handle) {
    VideoToolboxEncoder *encoder = (VideoToolboxEncoder *) handle;
    
    if (encoder == nil || !([encoder isKindOfClass: [VideoToolboxEncoder class]])) {
        LOG(
            LOG_LEVEL_WARNING,
            "couldn't release encoder object: is *handle instance of VideoToolboxEncoder?"
        );
    
        return 1;
    }
    
    LOG(LOG_LEVEL_INFO, "releasing encoder object");
    [encoder release];
    
    return 0;
}

int
xrdp_encoder_videotoolbox_encode(void *handle, int session,
                                 int frame_id,
                                 int width, int height,
                                 int format, const char *data,
                                 char *cdata, int *cdata_bytes) {
    
    VideoToolboxEncoder *encoder = (VideoToolboxEncoder *) handle;
    CVPixelBufferRef inputFrame = nil;
    CMSampleBufferRef outputFrame = nil;
    
    uint8_t *outputFrameData = nil;
    size_t outputFrameLength = 0;
    
    size_t offset = 0;
    
    OSStatus retval = noErr;
    
    NSDate *now = NSDate.now;
    CMTime timestamp = CMTimeMake(
        (int64_t) ([now timeIntervalSince1970] * 1000),
        1000
    );
    
    
    if (encoder == NULL || !([encoder isKindOfClass: [VideoToolboxEncoder class]])) {
        LOG(LOG_LEVEL_WARNING, "couldn't call encode function: is *handle instance of VideoToolboxEncoder?");
        return -1;
    }
    
    retval = CVPixelBufferCreateWithBytes(
        kCFAllocatorDefault,
        width, height,
        kCVPixelFormatType_32BGRA,
        (void *) data, width * 4,
        nil, nil,
        nil, &inputFrame
    );
    
    if (retval != noErr) {
        LOG(LOG_LEVEL_ERROR, "CVPixelBufferCreateWithBytes returned OSStatus %d", retval);
        return retval;
    }
    
    retval = [encoder encodeFrameSync:inputFrame timestamp: timestamp outBuffer: &outputFrame];
    
    if (retval != noErr) {
        goto ENCODE_END;
    }
    
    CMBlockBufferGetDataPointer(
        CMSampleBufferGetDataBuffer(outputFrame),
        0, nil,
        &outputFrameLength, &outputFrameData
    );
    
    
    offset += [encoder writeSPSPPSInto: cdata + offset from: outputFrame];
    offset += [encoder avccToAnnexB: outputFrameData length: outputFrameLength to: cdata + offset];
    
    *cdata_bytes = (int) offset;
    
    ENCODE_END:
    CVPixelBufferRelease(inputFrame);
    return retval;
}