
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

//#include "yami_inf.h"
#include "/opt/yami/include/yami_inf.h"

#include "arch.h"
#include "os_calls.h"
#include "xorgxrdp_helper.h"
#include "xorgxrdp_helper_x11.h"
#include "xorgxrdp_helper_yami.h"
#include "log.h"

extern EGLDisplay g_egl_display; /* in xorgxrdp_helper_x11.c */
extern EGLContext g_egl_context; /* in xorgxrdp_helper_x11.c */

//static char g_lib_name[] = "libyami_inf.so";
static char g_lib_name[] = "/opt/yami/lib/libyami_inf.so";
static char g_func_name[] = "yami_get_funcs";
static char g_drm_name[] = "/dev/dri/renderD128";

static yami_get_funcs_proc g_yami_get_funcs = NULL;

static struct yami_funcs g_enc_funcs;

static long g_lib = 0;

static int g_fd = -1;

struct enc_info
{
    int width;
    int height;
    int frameCount;
    int pad0;
    void *enc;
};

/*****************************************************************************/
int
xorgxrdp_helper_yami_init(void)
{
    int error;
    int version;

    LOGLN((LOG_LEVEL_INFO, LOGS, LOGP));
    g_lib = g_load_library(g_lib_name);
    if (g_lib == 0)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "load library for %s failed",
               LOGP, g_lib_name));
        return 1;
    }
    else
    {
        LOGLN((LOG_LEVEL_INFO, LOGS "loaded library %s", LOGP, g_lib_name));
    }
    g_yami_get_funcs = g_get_proc_address(g_lib, g_func_name);
    if (g_yami_get_funcs == NULL)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "get proc address for %s failed",
               LOGP, g_func_name));
        return 1;
    }
    g_memset(&g_enc_funcs, 0, sizeof(g_enc_funcs));
    error = g_yami_get_funcs(&g_enc_funcs, YI_VERSION_INT(YI_MAJOR, YI_MINOR));
    LOGLN((LOG_LEVEL_INFO, LOGS "yami_get_funcs rv %d",
           LOGP, error));
    if (error != YI_SUCCESS)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "g_yami_get_funcs failed", LOGP));
        return 1;
    }
    error = g_enc_funcs.yami_get_version(&version);
    if (error != YI_SUCCESS)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami_get_version failed", LOGP));
        return 1;
    }
    if (version < YI_VERSION_INT(YI_MAJOR, YI_MINOR))
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami version too old 0x%8.8x",
               LOGP, version));
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "yami version 0x%8.8x ok", LOGP, version));
    g_fd = g_file_open_ex(g_drm_name, 1, 1, 0, 0);
    if (g_fd == -1)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "open %s failed", LOGP, g_drm_name));
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "open %s ok, fd %d", LOGP, g_drm_name, g_fd));
    error = g_enc_funcs.yami_init(YI_TYPE_DRM, (void *) (size_t) g_fd);
    if (error != 0)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami_init failed", LOGP));
        return 1;
    }
    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_yami_create_encoder(int width, int height, int tex,
                                    int tex_format, struct enc_info **ei)
{
    int error;
    struct enc_info *lei;

    LOGLN((LOG_LEVEL_INFO, LOGS, LOGP));
    lei = g_new0(struct enc_info, 1);
    if (lei == NULL)
    {
        return 1;
    }
    error = g_enc_funcs.yami_encoder_create(&(lei->enc), width, height,
                                            YI_TYPE_H264,
                                            YI_H264_ENC_FLAGS_PROFILE_MAIN);
    if (error != YI_SUCCESS)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami_encoder_create failed %d",
               LOGP, error));
        g_free(lei);
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "yami_encoder_create ok", LOGP));
    lei->width = width;
    lei->height = height;
    *ei = lei;
    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_yami_delete_encoder(struct enc_info *ei)
{
    LOGLN((LOG_LEVEL_INFO, LOGS, LOGP));
    g_enc_funcs.yami_encoder_delete(ei->enc);
    g_free(ei);
    return 0;
}

static const EGLint g_create_image_attr[] =
{
    EGL_NONE
};

/*****************************************************************************/
int
xorgxrdp_helper_yami_encode(struct enc_info *ei, int tex,
                            void *cdata, int *cdata_bytes)
{
    EGLClientBuffer cb;
    EGLImageKHR image;
    EGLint stride;
    EGLint offset;
    int fd;
    int error;
    int fourcc;
    int num_planes;
    EGLuint64KHR modifiers;

    LOGLND((LOG_LEVEL_INFO, LOGS "tex %d", LOGP, tex));
    LOGLND((LOG_LEVEL_INFO, LOGS "g_egl_display %p", LOGP, g_egl_display));
    LOGLND((LOG_LEVEL_INFO, LOGS "g_egl_context %p", LOGP, g_egl_context));
    cb = (EGLClientBuffer) (size_t) tex;
    image = eglCreateImageKHR(g_egl_display, g_egl_context,
                              EGL_GL_TEXTURE_2D_KHR,
                              cb, g_create_image_attr);
    LOGLND((LOG_LEVEL_INFO, LOGS "image %p", LOGP, image));
    if (image == EGL_NO_IMAGE_KHR)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "eglCreateImageKHR failed", LOGP));
        return 1;
    }
    if (!eglExportDMABUFImageQueryMESA(g_egl_display, image,
                                       &fourcc, &num_planes,
                                       &modifiers))
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "eglExportDMABUFImageQueryMESA failed",
               LOGP));
        eglDestroyImageKHR(g_egl_display, image);
        return 1;
    }

    LOGLND((LOG_LEVEL_INFO, LOGS "fourcc 0x%8.8X num_planes %d "
            "modifiers %d", LOGP, fourcc, num_planes, (int) modifiers));
    if (num_planes != 1)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "eglExportDMABUFImageQueryMESA return "
               "bad num_planes %d", LOGP, num_planes));
        eglDestroyImageKHR(g_egl_display, image);
        return 1;
    }
    if (!eglExportDMABUFImageMESA(g_egl_display, image, &fd,
                                  &stride, &offset))
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "eglExportDMABUFImageMESA failed",
               LOGP));
        eglDestroyImageKHR(g_egl_display, image);
        return 1;
    }
    LOGLND((LOG_LEVEL_INFO, LOGS "fd %d stride %d offset %d", LOGP, fd,
            stride, offset));
    LOGLND((LOG_LEVEL_INFO, LOGS "width %d height %d", LOGP, ei->width,
            ei->height));
    error = g_enc_funcs.yami_encoder_set_fd_src(ei->enc, fd,
            ei->width, ei->height,
            stride,
            stride * ei->height,
            YI_YUY2);
    LOGLND((LOG_LEVEL_INFO, LOGS "yami_encoder_set_fd_src rv %d",
            LOGP, error));
    if (error != YI_SUCCESS)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami_encoder_set_fd_src failed",
               LOGP));
        g_file_close(fd);
        eglDestroyImageKHR(g_egl_display, image);
        return 1;
    }
    error = g_enc_funcs.yami_encoder_encode(ei->enc, cdata, cdata_bytes);
    LOGLND((LOG_LEVEL_INFO, LOGS "encoder_encode rv %d cdata_bytes %d",
            LOGP, error, *cdata_bytes));
    if (error != YI_SUCCESS)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "yami_encoder_encode failed", LOGP));
        g_file_close(fd);
        eglDestroyImageKHR(g_egl_display, image);
        return 1;
    }
    g_file_close(fd);
    eglDestroyImageKHR(g_egl_display, image);
    return 0;
}
