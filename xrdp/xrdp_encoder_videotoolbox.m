#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>

#import "log.h"

@implementation VideoToolboxEncoder: NSObject {
    VTCompressionSessionRef compressionSession;
}

-(VideoToolboxEncoder *) init {
    self = [super init];
    return self;
}

-(OSStatus) initializeCompressionSession: (int) width height: (int) height {
    const NSString *specificationKeys[] = {
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder
    };
    
    const void *specificationValues[] = {
        kCFBooleanTrue
    };
    
    CFDictionaryRef encoderSpecification = CFDictionaryCreate(
        kCFAllocatorDefault,
        specificationKeys, specificationValues,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    
    OSStatus retval = VTCompressionSessionCreate(
        kCFAllocatorDefault,
        width, height,
        kCMVideoCodecType_H264, encoderSpecification,
        nil, nil,
        nil, self,
        &self->compressionSession
    );
    
    if (retval != 0) {
        return retval;
    }
    
    VTSessionSetProperty(
        self->compressionSession,
        kVTCompressionPropertyKey_ProfileLevel,
        kVTProfileLevel_H264_Baseline_AutoLevel
    );
    
    // 1Mbps?
    const int averageBitrate = 1024 * 1024;
    
    VTSessionSetProperty(
        self->compressionSession,
        kVTCompressionPropertyKey_AverageBitRate,
        &averageBitrate
    );
    return 0;
}

@end

void *
xrdp_encoder_videotoolbox_create() {
    VideoToolboxEncoder *encoder = [[VideoToolboxEncoder alloc] init];
    LOG(LOG_LEVEL_DEBUG, "new encoder object created");
    
    return encoder;
}

void
xrdp_encoder_videotoolbox_delete(void *handle) {
    VideoToolboxEncoder *maybe_encoder = (VideoToolboxEncoder *) handle;
    
    if (maybe_encoder != NULL && [maybe_encoder isKindOfClass: [VideoToolboxEncoder class]]) {
        LOG(LOG_LEVEL_DEBUG, "releasing encoder object");
        [maybe_encoder release];
    }
}