#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#if (!defined(XRDP_GLX)) && (!defined(XRDP_EGL))
#define XRDP_GLX
//#define XRDP_EGL
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include <epoxy/gl.h>

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xorgxrdp_helper.h"
#include "xorgxrdp_helper_x11.h"
#include "log.h"

/* X11 */
static Display *g_display = NULL;
static int g_x_socket = 0;
static int g_screen_num = 0;
static Screen *g_screen = NULL;
static Window g_root_window = None;
static Visual *g_vis = NULL;
static GC g_gc;

#if defined(XRDP_NVENC)

#include "xorgxrdp_helper_nvenc.h"

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_init(void)
{
    return xorgxrdp_helper_nvenc_init();
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_delete_encoder(struct enc_info *ei)
{
    return xorgxrdp_helper_nvenc_delete_encoder(ei);
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_create_encoder(int width, int height, int enc_texture,
                                       int tex_format, struct enc_info **ei)
{
    return xorgxrdp_helper_nvenc_create_encoder(width, height, enc_texture,
            tex_format, ei);
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_encode(struct enc_info *ei, int enc_texture,
                               void *cdata, int *cdata_bytes)
{
    return xorgxrdp_helper_nvenc_encode(ei, enc_texture, cdata, cdata_bytes);
}

#elif defined(XRDP_YAMI)

#include "xorgxrdp_helper_yami.h"

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_init(void)
{
    return xorgxrdp_helper_yami_init();
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_delete_encoder(struct enc_info *ei)
{
    return xorgxrdp_helper_yami_delete_encoder(ei);
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_create_encoder(int width, int height, int enc_texture,
                                       int tex_format, struct enc_info **ei)
{
    return xorgxrdp_helper_yami_create_encoder(width, height, enc_texture,
            tex_format, ei);
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_encode(struct enc_info *ei, int enc_texture,
                               void *cdata, int *cdata_bytes)
{
    return xorgxrdp_helper_yami_encode(ei, enc_texture, cdata, cdata_bytes);
}

#else

struct enc_info
{
    int pad0;
};

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_init(void)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_delete_encoder(struct enc_info *ei)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_create_encoder(int width, int height, int enc_texture,
                                       int tex_format, struct enc_info **ei)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_encoder_encode(struct enc_info *ei, int enc_texture,
                               void *cdata, int *cdata_bytes)
{
    return 1;
}

#endif

#if defined(XRDP_GLX)

#include <epoxy/glx.h>

static int g_n_fbconfigs = 0;
static int g_n_pixconfigs = 0;
static GLXFBConfig *g_fbconfigs = NULL;
static GLXFBConfig *g_pixconfigs = NULL;
static GLXContext g_gl_context = 0;

static const int g_fbconfig_attrs[] =
{
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,  True,
    GLX_RED_SIZE,      8,
    GLX_GREEN_SIZE,    8,
    GLX_BLUE_SIZE,     8,
    None
};

static const int g_pixconfig_attrs[] =
{
    GLX_BIND_TO_TEXTURE_RGBA_EXT,       True,
    GLX_DRAWABLE_TYPE,                  GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT,    GLX_TEXTURE_2D_BIT_EXT,
    GLX_DOUBLEBUFFER,                   False,
    GLX_Y_INVERTED_EXT,                 True,
    None
};

static const int g_pixmap_attribs[] =
{
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    None
};

typedef GLXPixmap inf_image_t;

/*****************************************************************************/
static int
xorgxrdp_helper_inf_init(void)
{
    const char *ext_str;
    int glx_ver;
    int ok;

    glx_ver = epoxy_glx_version(g_display, g_screen_num);
    LOGLN((LOG_LEVEL_INFO, LOGS "glx_ver %d", LOGP, glx_ver));
    if (glx_ver < 11) /* GLX version 1.1 */
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "glx_ver too old %d", LOGP, glx_ver));
        return 1;
    }
    if (!epoxy_has_glx_extension(g_display, g_screen_num,
                                 "GLX_EXT_texture_from_pixmap"))
    {
        ext_str = glXQueryExtensionsString(g_display, g_screen_num);
        LOGLN((LOG_LEVEL_ERROR, LOGS "GLX_EXT_texture_from_pixmap not "
               "present [%s]", LOGP, ext_str));
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "GLX_EXT_texture_from_pixmap present", LOGP));
    g_fbconfigs = glXChooseFBConfig(g_display, g_screen_num,
                                    g_fbconfig_attrs, &g_n_fbconfigs);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_fbconfigs %p", LOGP, g_fbconfigs));
    g_gl_context = glXCreateNewContext(g_display, g_fbconfigs[0],
                                       GLX_RGBA_TYPE, NULL, 1);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_gl_context %p", LOGP, g_gl_context));
    ok = glXMakeCurrent(g_display, g_root_window, g_gl_context);
    LOGLN((LOG_LEVEL_INFO, LOGS "ok %d", LOGP, ok));
    g_pixconfigs = glXChooseFBConfig(g_display, g_screen_num,
                                     g_pixconfig_attrs, &g_n_pixconfigs);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_pixconfigs %p g_n_pixconfigs %d", LOGP,
           g_pixconfigs, g_n_pixconfigs));
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_create_image(Pixmap pixmap, inf_image_t *inf_image)
{
    *inf_image = glXCreatePixmap(g_display, g_pixconfigs[0],
                                 pixmap, g_pixmap_attribs);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_destroy_image(inf_image_t inf_image)
{
    glXDestroyPixmap(g_display, inf_image);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_bind_tex_image(inf_image_t inf_image)
{
    glXBindTexImageEXT(g_display, inf_image, GLX_FRONT_EXT, NULL);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_release_tex_image(inf_image_t inf_image)
{
    glXReleaseTexImageEXT(g_display, inf_image, GLX_FRONT_EXT);
    return 0;
}

#elif defined(XRDP_EGL)

#include <epoxy/egl.h>

static EGLDisplay g_egl_display;
static EGLContext g_egl_context;
static EGLSurface g_egl_surface;
static EGLConfig g_ecfg;
static EGLint g_num_config;

static EGLint g_choose_config_attr[] =
{
    EGL_COLOR_BUFFER_TYPE,     EGL_RGB_BUFFER,
    EGL_BUFFER_SIZE,           32,
    EGL_RED_SIZE,              8,
    EGL_GREEN_SIZE,            8,
    EGL_BLUE_SIZE,             8,
    EGL_ALPHA_SIZE,            8,

    EGL_DEPTH_SIZE,            24,
    EGL_STENCIL_SIZE,          8,

    EGL_SAMPLE_BUFFERS,        0,
    EGL_SAMPLES,               0,

    EGL_SURFACE_TYPE,          EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
    EGL_RENDERABLE_TYPE,       EGL_OPENGL_BIT,

    //EGL_BIND_TO_TEXTURE_RGB,   EGL_TRUE,
    //EGL_Y_INVERTED_NOK, EGL_TRUE,

    EGL_NONE,

    //EGL_RED_SIZE,        8,
    //EGL_GREEN_SIZE,      8,
    //EGL_BLUE_SIZE,       8,
    //EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    //EGL_RENDERABLE_TYPE, 0,
    //EGL_BIND_TO_TEXTURE_RGB, EGL_TRUE,
    //EGL_NONE
    //EGL_RED_SIZE,           8,
    //EGL_GREEN_SIZE,         8,
    //EGL_BLUE_SIZE,          8,
    //EGL_ALPHA_SIZE,         0,
    //EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
    //EGL_CONFIG_CAVEAT,      EGL_NONE,
    //EGL_MATCH_NATIVE_PIXMAP, 1,
    //EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
    //EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
    //EGL_NONE
};
static EGLint g_create_context_attr[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

static const EGLint g_create_surface_attr[] =
{
    //EGL_Y_INVERTED_NOK, EGL_TRUE,
    EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
    //EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
    //EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
    //EGL_MIPMAP_TEXTURE, EGL_TRUE,
    //EGL_TEXTURE_TARGET
    EGL_NONE
};

typedef EGLSurface inf_image_t;

/*****************************************************************************/
static int
xorgxrdp_helper_inf_init(void)
{
    int ok;

    g_egl_display = eglGetDisplay((EGLNativeDisplayType) g_display);
    eglInitialize(g_egl_display, NULL, NULL);
    eglChooseConfig(g_egl_display, g_choose_config_attr, &g_ecfg,
                    1, &g_num_config);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_ecfg %p g_num_config %d",
           LOGP, g_ecfg, g_num_config));
    g_egl_surface = eglCreateWindowSurface(g_egl_display, g_ecfg,
                                           g_root_window, NULL);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_egl_surface %p", LOGP, g_egl_surface));
    eglBindAPI(EGL_OPENGL_API);
    g_egl_context = eglCreateContext(g_egl_display, g_ecfg,
                                     EGL_NO_CONTEXT, g_create_context_attr);
    LOGLN((LOG_LEVEL_INFO, LOGS "g_egl_context %p", LOGP, g_egl_context));
    ok = eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface,
                        g_egl_context);
    LOGLN((LOG_LEVEL_INFO, LOGS "ok %d", LOGP, ok));

    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_create_image(Pixmap pixmap, inf_image_t *inf_image)
{
    *inf_image = eglCreatePixmapSurface(g_egl_display, g_ecfg,
                                        pixmap, g_create_surface_attr);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_destroy_image(inf_image_t inf_image)
{
    eglDestroySurface(g_egl_display, inf_image);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_bind_tex_image(inf_image_t inf_image)
{
    eglBindTexImage(g_egl_display, inf_image, EGL_BACK_BUFFER);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_release_tex_image(inf_image_t inf_image)
{
    eglReleaseTexImage(g_egl_display, inf_image, EGL_BACK_BUFFER);
    return 0;
}

#else

typedef void *inf_image_t;

/*****************************************************************************/
static int
xorgxrdp_helper_inf_init(void)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_create_image(Pixmap pixmap, inf_image_t *inf_image)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_destroy_image(inf_image_t inf_image)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_bind_tex_image(inf_image_t inf_image)
{
    return 1;
}

/*****************************************************************************/
static int
xorgxrdp_helper_inf_release_tex_image(inf_image_t inf_image)
{
    return 1;
}

#endif

struct mon_info
{
    int width;
    int height;
    Pixmap pixmap;
    inf_image_t inf_image;
    GLuint bmp_texture;
    GLuint enc_texture;
    int tex_format;
    GLfloat *(*get_vertices)(GLuint *vertices_bytes,
                             GLuint *vertices_pointes,
                             int num_crects, struct xh_rect *crects,
                             int width, int height);
    struct xh_rect viewport;
    struct enc_info *ei;
};

static struct mon_info g_mons[16];

static GLuint g_quad_vao = 0;
static GLuint g_fb = 0;

#define XH_SHADERCOPY           0
#define XH_SHADERRGB2YUV420     1
#define XH_SHADERRGB2YUV444     2
#define XH_SHADERRGB2YUV420MV   3
#define XH_SHADERRGB2YUV420AV   4
#define XH_SHADERRGB2YUV420AVV2 5

#define XH_NUM_SHADERS 6

struct shader_info
{
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint program;
    GLint tex_loc;
    GLint tex_size_loc;
    GLint ymath_loc;
    GLint umath_loc;
    GLint vmath_loc;
    int current_matrix;
};
static struct shader_info g_si[XH_NUM_SHADERS];

static const GLfloat g_vertices[] =
{
    -1.0f,  1.0f,
        -1.0f, -1.0f,
        1.0f,  1.0f,
        1.0f, -1.0f
    };

struct rgb2yuv_matrix
{
    GLfloat ymath[4];
    GLfloat umath[4];
    GLfloat vmath[4];
};

static struct rgb2yuv_matrix g_rgb2yux_matrix[3] =
{
    {
        /* yuv bt601 lagecy */
        {  66.0 / 256.0,  129.0 / 256.0,  25.0 / 256.0,  16.0 / 256.0 },
        { -38.0 / 256.0,  -74.0 / 256.0, 112.0 / 256.0, 128.0 / 256.0 },
        { 112.0 / 256.0,  -94.0 / 256.0, -18.0 / 256.0, 128.0 / 256.0 }
    },
    {
        /* yuv bt709 full range, used in gfx h264 */
        {  54.0 / 256.0,  183.0 / 256.0,  18.0 / 256.0,   0.0 / 256.0 },
        { -29.0 / 256.0,  -99.0 / 256.0, 128.0 / 256.0, 128.0 / 256.0 },
        { 128.0 / 256.0, -116.0 / 256.0, -12.0 / 256.0, 128.0 / 256.0 }
    },
    {
        /* yuv remotefx and gfx progressive remotefx */
        {    0.299000,     0.587000,    0.114000,         0.0 },
        {   -0.168935,    -0.331665,    0.500590,         0.5 },
        {    0.499830,    -0.418531,   -0.081282,         0.5 }
    }
};

static const GLchar g_vs[] =
    "\
attribute vec4 position;\n\
void main(void)\n\
{\n\
    gl_Position = vec4(position.xy, 0.0, 1.0);\n\
}\n";
static const GLchar g_fs_copy[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
void main(void)\n\
{\n\
    gl_FragColor = texture2D(tex, gl_FragCoord.xy / tex_size);\n\
}\n";
static const GLchar g_fs_rgb_to_yuv420[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        pix = texture2D(tex, vec2(x, y) / tex_size);\n\
        pix = vec4(pix.rgb, 1.0);\n\
        gl_FragColor = clamp(dot(ymath, pix), 0.0, 1.0);\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x, y + 1.0) / tex_size);\n\
            pix += texture2D(tex, vec2(x + 1.0, y + 1.0) / tex_size);\n\
            pix /= 4.0;\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x - 1.0, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x, y + 1.0) / tex_size);\n\
            pix += texture2D(tex, vec2(x - 1.0, y + 1.0) / tex_size);\n\
            pix /= 4.0;\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
}\n";
static const GLchar g_fs_rgb_to_yuv444[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    pix = texture2D(tex, gl_FragCoord.xy / tex_size);\n\
    pix = vec4(pix.rgb, 1.0);\n\
    pix = vec4(dot(vmath, pix), dot(umath, pix), dot(ymath, pix), 1.0);\n\
    gl_FragColor = clamp(pix, 0.0, 1.0);\n\
}\n";
/*
RGB
    00 10 20 30 40 50 60 70 80 90 A0 B0 C0 D0 E0 F0
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    02 12 22 32 42 52 62 72 82 92 A2 B2 C2 D2 E2 F2
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F3
    04 14 24 34 44 54 64 74 84 94 A4 B4 C4 D4 E4 F4
    05 15 25 35 45 55 65 75 85 95 A5 B5 C5 D5 E5 F5
    06 16 26 36 46 56 66 76 86 96 A6 B6 C6 D6 E6 F6
    07 17 27 37 47 57 67 77 87 97 A7 B7 C7 D7 E7 F7
    08 18 28 38 48 58 68 78 88 98 A8 B8 C8 D8 E8 F8
    09 19 29 39 49 59 69 79 89 99 A9 B9 C9 D9 E9 F9
    0A 1A 2A 3A 4A 5A 6A 7A 8A 9A AA BA CA DA EA FA
    0B 1B 2B 3B 4B 5B 6B 7B 8B 9B AB BB CB DB EB FB
    0C 1C 2C 3C 4C 5C 6C 7C 8C 9C AC BC CC DC EC FC
    0D 1D 2D 3D 4D 5D 6D 7D 8D 9D AD BD CD DD ED FD
    0E 1E 2E 3E 4E 5E 6E 7E 8E 9E AE BE CE DE EE FE
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
MAIN VIEW - NV12
    /---------------------Y-----------------------\
    00 10 20 30 40 50 60 70 80 90 A0 B0 C0 D0 E0 F0
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
    /U /V /U /V /U /V /U /V /U /V /U /V /U /V /U /V
    00 00 20 20 40 40 60 60 80 80 A0 A0 C0 C0 E0 E0
    02 02 22 22 42 42 62 62 82 82 A2 A2 C2 C2 E2 E2
    ...
    0E 0E 2E 2E 4E 4E 6E 6E 8E 8E AE AE CE CE EE EE
*/
static const GLchar g_fs_rgb_to_yuv420_mv[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        pix = texture2D(tex, vec2(x, y) / tex_size);\n\
        pix = vec4(pix.rgb, 1.0);\n\
        gl_FragColor = clamp(dot(ymath, pix), 0.0, 1.0);\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x - 1.0, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
}\n";
/*
AUXILIARY VIEW - NV12
    /---------------------U-----------------------\
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F4
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
    /---------------------V-----------------------\
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F4
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
    ... (8 LINES U, 8 LINES V, REPEAT)
    /U /V /U /V /U /V /U /V /U /V /U /V /U /V /U /V
    10 10 30 30 50 50 70 70 90 90 B0 B0 D0 D0 F0 F0
    12 12 32 32 52 52 72 72 92 92 B2 B2 D2 D2 F2 F2
    ...
    1E 1E 3E 3E 5E 5E 7E 7E 9E 9E BE BE DE DE FE FE
*/
static const GLchar g_fs_rgb_to_yuv420_av[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    float y1;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        y1 = mod(y, 16.0);\n\
        if (y1 < 8.0)\n\
        {\n\
            y = floor(y / 16.0) * 8.0 + y1;\n\
            y = floor(y) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            y = floor(y / 16.0) * 8.0 + (y1 - 8.0);\n\
            y = floor(y) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
}\n";
/*
AUXILIARY VIEW V2 - NV12
    /----------U----------\ /----------V----------\
    10 30 50 70 90 B0 D0 F0 10 30 50 70 90 B0 D0 F0
    11 31 51 71 91 B1 D1 F1 11 31 51 71 91 B1 D1 F1
    ...
    1F 3F 5F 7F 9F BF DF FF 1F 3F 5F 7F 9F BF DF FF
    /----------U----------\ /----------V----------\
    01 21 41 61 81 A1 C1 E1 01 21 41 61 81 A1 C1 E1
    03 23 43 63 83 A3 C3 E3 03 23 43 63 83 A3 C3 E3
    ...
    0F 2F 4F 6F 8F AF CF EF 0F 2F 4F 6F 8F AF CF EF
*/
static const GLchar g_fs_rgb_to_yuv420_av_v2[] =
    "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    float x1;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    x1 = tex_size.x / 2.0;\n\
    if (y < tex_size.y)\n\
    {\n\
        if (x < x1)\n\
        {\n\
            x = floor(x) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            x = floor(x - x1) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 1.5;\n\
        if (x < x1)\n\
        {\n\
            x = floor(x) * 2.0 + 0.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(umath, pix), 0.0, 1.0);\n\
        }\n\
        else\n\
        {\n\
            x = floor(x - x1) * 2.0 + 0.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix = vec4(pix.rgb, 1.0);\n\
            gl_FragColor = clamp(dot(vmath, pix), 0.0, 1.0);\n\
        }\n\
    }\n\
}\n";

/*****************************************************************************/
int
xorgxrdp_helper_x11_init(void)
{
    const GLchar *vsource[XH_NUM_SHADERS];
    const GLchar *fsource[XH_NUM_SHADERS];
    GLint linked;
    GLint compiled;
    GLint vlength;
    GLint flength;
    GLuint quad_vbo;
    int index;
    int gl_ver;

    /* x11 */
    g_display = XOpenDisplay(0);
    if (g_display == NULL)
    {
        return 1;
    }
    g_x_socket = XConnectionNumber(g_display);
    g_screen_num = DefaultScreen(g_display);
    g_screen = ScreenOfDisplay(g_display, g_screen_num);
    g_root_window = RootWindowOfScreen(g_screen);
    g_vis = XDefaultVisual(g_display, g_screen_num);
    g_gc = DefaultGC(g_display, 0);
    if (xorgxrdp_helper_inf_init() != 0)
    {
        return 1;
    }
    gl_ver = epoxy_gl_version();
    LOGLN((LOG_LEVEL_INFO, LOGS "gl_ver %d", LOGP, gl_ver));
    if (gl_ver < 30)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "gl_ver too old %d", LOGP, gl_ver));
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "vendor: %s", LOGP,
           (const char *) glGetString(GL_VENDOR)));
    LOGLN((LOG_LEVEL_INFO, LOGS "version: %s", LOGP,
           (const char *) glGetString(GL_VERSION)));
    /* create vertex array */
    glGenVertexArrays(1, &g_quad_vao);
    glBindVertexArray(g_quad_vao);
    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);
    glGenFramebuffers(1, &g_fb);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &quad_vbo);

    /* create copy shader */
    vsource[XH_SHADERCOPY] = g_vs;
    fsource[XH_SHADERCOPY] = g_fs_copy;
    /* create rgb2yuv shader */
    vsource[XH_SHADERRGB2YUV420] = g_vs;
    fsource[XH_SHADERRGB2YUV420] = g_fs_rgb_to_yuv420;
    /* create rgb2yuv shader */
    vsource[XH_SHADERRGB2YUV444] = g_vs;
    fsource[XH_SHADERRGB2YUV444] = g_fs_rgb_to_yuv444;

    vsource[XH_SHADERRGB2YUV420MV] = g_vs;
    fsource[XH_SHADERRGB2YUV420MV] = g_fs_rgb_to_yuv420_mv;

    vsource[XH_SHADERRGB2YUV420AV] = g_vs;
    fsource[XH_SHADERRGB2YUV420AV] = g_fs_rgb_to_yuv420_av;

    vsource[XH_SHADERRGB2YUV420AVV2] = g_vs;
    fsource[XH_SHADERRGB2YUV420AVV2] = g_fs_rgb_to_yuv420_av_v2;

    for (index = 0; index < XH_NUM_SHADERS; index++)
    {
        g_si[index].vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        g_si[index].fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        vlength = g_strlen(vsource[index]);
        flength = g_strlen(fsource[index]);
        glShaderSource(g_si[index].vertex_shader, 1,
                       &(vsource[index]), &vlength);
        glShaderSource(g_si[index].fragment_shader, 1,
                       &(fsource[index]), &flength);
        glCompileShader(g_si[index].vertex_shader);
        glGetShaderiv(g_si[index].vertex_shader, GL_COMPILE_STATUS,
                      &compiled);
        LOGLN((LOG_LEVEL_INFO, LOGS "vertex_shader compiled %d", LOGP, compiled));
        glCompileShader(g_si[index].fragment_shader);
        glGetShaderiv(g_si[index].fragment_shader, GL_COMPILE_STATUS,
                      &compiled);
        LOGLN((LOG_LEVEL_INFO, LOGS "fragment_shader compiled %d", LOGP, compiled));
        g_si[index].program = glCreateProgram();
        glAttachShader(g_si[index].program, g_si[index].vertex_shader);
        glAttachShader(g_si[index].program, g_si[index].fragment_shader);
        glLinkProgram(g_si[index].program);
        glGetProgramiv(g_si[index].program, GL_LINK_STATUS, &linked);
        LOGLN((LOG_LEVEL_INFO, LOGS "linked %d", LOGP, linked));
        g_si[index].tex_loc =
            glGetUniformLocation(g_si[index].program, "tex");
        g_si[index].tex_size_loc =
            glGetUniformLocation(g_si[index].program, "tex_size");
        g_si[index].ymath_loc =
            glGetUniformLocation(g_si[index].program, "ymath");
        g_si[index].umath_loc =
            glGetUniformLocation(g_si[index].program, "umath");
        g_si[index].vmath_loc =
            glGetUniformLocation(g_si[index].program, "vmath");
        LOGLN((LOG_LEVEL_INFO, LOGS "tex_loc %d "
               "tex_size_loc %d ymath_loc %d umath_loc %d vmath_loc %d",
               LOGP, g_si[index].tex_loc, g_si[index].tex_size_loc,
               g_si[index].ymath_loc, g_si[index].umath_loc,
               g_si[index].vmath_loc));
        /* set default matrix */
        glUseProgram(g_si[index].program);
        if (g_si[index].ymath_loc >= 0)
        {
            glUniform4fv(g_si[index].ymath_loc, 1, g_rgb2yux_matrix[0].ymath);
        }
        if (g_si[index].umath_loc >= 0)
        {
            glUniform4fv(g_si[index].umath_loc, 1, g_rgb2yux_matrix[0].umath);
        }
        if (g_si[index].vmath_loc >= 0)
        {
            glUniform4fv(g_si[index].vmath_loc, 1, g_rgb2yux_matrix[0].vmath);
        }
        glUseProgram(0);
    }
    g_memset(g_mons, 0, sizeof(g_mons));
    xorgxrdp_helper_encoder_init();
    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_x11_get_wait_objs(intptr_t *objs, int *obj_count)
{
    objs[*obj_count] = g_x_socket;
    (*obj_count)++;
    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_x11_check_wait_objs(void)
{
    XEvent xevent;

    while (XPending(g_display) > 0)
    {
        LOGLN((LOG_LEVEL_INFO, LOGS "loop", LOGP));
        XNextEvent(g_display, &xevent);
    }
    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_x11_delete_all_pixmaps(void)
{
    int index;
    struct mon_info *mi;

    for (index = 0; index < 16; index++)
    {
        mi = g_mons + index;
        if (mi->pixmap != 0)
        {
            xorgxrdp_helper_encoder_delete_encoder(mi->ei);
            glDeleteTextures(1, &(mi->bmp_texture));
            glDeleteTextures(1, &(mi->enc_texture));
            xorgxrdp_helper_inf_destroy_image(mi->inf_image);
            XFreePixmap(g_display, mi->pixmap);
            mi->pixmap = 0;
        }
    }
    return 0;
}

/*****************************************************************************/
static GLfloat *
get_vertices_all(GLuint *vertices_bytes, GLuint *vertices_pointes,
                 int num_crects, struct xh_rect *crects,
                 int width, int height)
{
    GLfloat *vertices;

    if (num_crects < 1)
    {
        return NULL;
    }
    vertices = g_new(GLfloat, 12);
    if (vertices == NULL)
    {
        return NULL;
    }
    vertices[0]  = -1;
    vertices[1]  =  1;
    vertices[2]  = -1;
    vertices[3]  = -1;
    vertices[4]  =  1;
    vertices[5]  =  1;
    vertices[6]  = -1;
    vertices[7]  = -1;
    vertices[8]  =  1;
    vertices[9]  =  1;
    vertices[10] =  1;
    vertices[11] = -1;
    *vertices_bytes = sizeof(GLfloat) * 12;
    *vertices_pointes = 6;
    return vertices;
}

/*****************************************************************************/
static GLfloat *
get_vertices420(GLuint *vertices_bytes, GLuint *vertices_pointes,
                int num_crects, struct xh_rect *crects,
                int width, int height)
{
    GLfloat *vertices;
    GLfloat *vert;
    GLfloat x1;
    GLfloat x2;
    GLfloat y1;
    GLfloat y2;
    int index;
    GLfloat fwidth;
    GLfloat fheight;
    const GLfloat fac13 = 1.0 / 3.0;
    const GLfloat fac23 = 2.0 / 3.0;
    const GLfloat fac43 = 4.0 / 3.0;
    struct xh_rect *crect;

    if (num_crects < 1)
    {
        return get_vertices_all(vertices_bytes, vertices_pointes,
                                num_crects, crects, width, height);
    }
    vertices = g_new(GLfloat, num_crects * 24);
    if (vertices == NULL)
    {
        return NULL;
    }
    fwidth = width  / 2.0;
    fheight = height / 2.0;
    for (index = 0; index < num_crects; index++)
    {
        crect = crects + index;
        LOGLND((LOG_LEVEL_INFO, LOGS "rect index %d x %d y %d w %d h %d",
                LOGP, index, crect->x, crect->y, crect->w, crect->h));
        x1 = crect->x / fwidth;
        y1 = crect->y / fheight;
        x2 = (crect->x + crect->w) / fwidth;
        y2 = (crect->y + crect->h) / fheight;
        vert = vertices + index * 24;
        /* y box */
        vert[0]  =  x1 - 1.0;
        vert[1]  =  y1 * fac23 - 1.0;
        vert[2]  =  x1 - 1.0;
        vert[3]  =  y2 * fac23 - 1.0;
        vert[4]  =  x2 - 1.0;
        vert[5]  =  y1 * fac23 - 1.0;
        vert[6]  =  x1 - 1.0;
        vert[7]  =  y2 * fac23 - 1.0;
        vert[8]  =  x2 - 1.0;
        vert[9]  =  y1 * fac23 - 1.0;
        vert[10] =  x2 - 1.0;
        vert[11] =  y2 * fac23 - 1.0;
        /* uv box */
        vert[12] =  x1 - 1.0;
        vert[13] = (y1 * fac13 + fac43) - 1.0;
        vert[14] =  x1 - 1.0;
        vert[15] = (y2 * fac13 + fac43) - 1.0;
        vert[16] =  x2 - 1.0;
        vert[17] = (y1 * fac13 + fac43) - 1.0;
        vert[18] =  x1  - 1.0;
        vert[19] = (y2 * fac13 + fac43) - 1.0;
        vert[20] =  x2 - 1.0;
        vert[21] = (y1 * fac13 + fac43) - 1.0;
        vert[22] =  x2 - 1.0;
        vert[23] = (y2 * fac13 + fac43) - 1.0;
    }
    *vertices_bytes = sizeof(GLfloat) * num_crects * 24;
    *vertices_pointes = num_crects * 12;
    return vertices;
}

/*****************************************************************************/
static GLfloat *
get_vertices444(GLuint *vertices_bytes, GLuint *vertices_pointes,
                int num_crects, struct xh_rect *crects,
                int width, int height)
{
    GLfloat *vertices;
    GLfloat *vert;
    GLfloat x1;
    GLfloat x2;
    GLfloat y1;
    GLfloat y2;
    int index;
    GLfloat fwidth;
    GLfloat fheight;
    struct xh_rect *crect;

    if (num_crects < 1)
    {
        return get_vertices_all(vertices_bytes, vertices_pointes,
                                num_crects, crects, width, height);
    }
    vertices = g_new(GLfloat, num_crects * 12);
    if (vertices == NULL)
    {
        return NULL;
    }
    fwidth = width  / 2.0;
    fheight = height / 2.0;
    for (index = 0; index < num_crects; index++)
    {
        crect = crects + index;
        x1 = crect->x / fwidth;
        y1 = crect->y / fheight;
        x2 = (crect->x + crect->w) / fwidth;
        y2 = (crect->y + crect->h) / fheight;
        vert = vertices + index * 12;
        vert[0]  = x1 - 1.0;
        vert[1]  = y1 - 1.0;
        vert[2]  = x1 - 1.0;
        vert[3]  = y2 - 1.0;
        vert[4]  = x2 - 1.0;
        vert[5]  = y1 - 1.0;
        vert[6]  = x1 - 1.0;
        vert[7]  = y2 - 1.0;
        vert[8]  = x2 - 1.0;
        vert[9]  = y1 - 1.0;
        vert[10] = x2 - 1.0;
        vert[11] = y2 - 1.0;
    }
    *vertices_bytes = sizeof(GLfloat) * num_crects * 12;
    *vertices_pointes = num_crects * 6;
    return vertices;
}

/*****************************************************************************/
int
xorgxrdp_helper_x11_create_pixmap(int width, int height, int magic,
                                  int con_id, int mon_id)
{
    struct mon_info *mi;
    Pixmap pixmap;
    XImage *ximage;
    int img[64];
    inf_image_t inf_image;
    GLuint bmp_texture;
    GLuint enc_texture;

    mi = g_mons + (mon_id & 0xF);
    mi->tex_format = XH_YUV420;
    //mi->tex_format = XH_YUV444;
    if (mi->pixmap != 0)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "error already setup", LOGP));
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "width %d height %d, "
           "magic 0x%8.8x, con_id %d mod_id %d", LOGP, width, height,
           magic, con_id, mon_id));
    pixmap = XCreatePixmap(g_display, g_root_window, width, height, 24);
    LOGLN((LOG_LEVEL_INFO, LOGS "pixmap %d", LOGP, (int) pixmap));

    if (xorgxrdp_helper_inf_create_image(pixmap, &inf_image) != 0)
    {
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "inf_image %p", LOGP, (void *) inf_image));

    g_memset(img, 0, sizeof(img));
    img[0] = magic;
    img[1] = con_id;
    img[2] = mon_id;
    ximage = XCreateImage(g_display, g_vis, 24, ZPixmap, 0, (char *) img,
                          4, 4, 32, 0);
    XPutImage(g_display, pixmap, g_gc, ximage, 0, 0, 0, 0, 4, 4);
    XFree(ximage);

    glEnable(GL_TEXTURE_2D);
    /* texture that gets encoded */
    glGenTextures(1, &enc_texture);
    glBindTexture(GL_TEXTURE_2D, enc_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (mi->tex_format == XH_YUV420)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height * 3 / 2, 0,
                     GL_RED, GL_UNSIGNED_BYTE, NULL);
        mi->get_vertices = get_vertices420;
        mi->viewport.x = 0;
        mi->viewport.y = 0;
        mi->viewport.w = width;
        mi->viewport.h = height * 3 / 2;
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
        mi->get_vertices = get_vertices444;
        mi->viewport.x = 0;
        mi->viewport.y = 0;
        mi->viewport.w = width;
        mi->viewport.h = height;
    }
    /* texture that binds with pixmap */
    glGenTextures(1, &bmp_texture);
    glBindTexture(GL_TEXTURE_2D, bmp_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (xorgxrdp_helper_encoder_create_encoder(width, height,
            enc_texture, mi->tex_format,
            &(mi->ei)) != 0)
    {
        return 1;
    }

    mi->pixmap = pixmap;
    mi->inf_image = inf_image;
    mi->enc_texture = enc_texture;
    mi->width = width;
    mi->height = height;
    mi->bmp_texture = bmp_texture;

    return 0;
}

/*****************************************************************************/
int
xorgxrdp_helper_x11_encode_pixmap(int width, int height, int mon_id,
                                  int num_crects, struct xh_rect *crects,
                                  void *cdata, int *cdata_bytes)
{
    struct mon_info *mi;
    struct shader_info *si;
    int rv;
    GLuint vao;
    GLuint vbo;
    GLfloat *vertices;
    GLuint vertices_bytes;
    GLuint vertices_pointes;

    mi = g_mons + (mon_id & 0xF);
    if ((width != mi->width) || (height != mi->height))
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "error width %d should be %d "
               "height %d should be %d", LOGP,
               width, mi->width, height, mi->height));
        return 1;
    }
    /* rgb to yuv */
    si = g_si + mi->tex_format % XH_NUM_SHADERS;
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mi->bmp_texture);
    xorgxrdp_helper_inf_bind_tex_image(mi->inf_image);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, mi->enc_texture, 0);
    glUseProgram(si->program);
    /* setup vertices from crects */
    vertices = mi->get_vertices(&vertices_bytes, &vertices_pointes,
                                num_crects, crects, width, height);
    if (vertices == NULL)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "error get_vertices failed num_crects %d",
               LOGP, num_crects));
        return 1;
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);
    /* uniforms */
    glUniform2f(si->tex_size_loc, mi->width, mi->height);
    /* viewport and draw */
    glViewport(mi->viewport.x, mi->viewport.y, mi->viewport.w, mi->viewport.h);
    glDrawArrays(GL_TRIANGLES, 0, vertices_pointes);
    /* cleanup */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    g_free(vertices);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    xorgxrdp_helper_inf_release_tex_image(mi->inf_image);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    /* sync before encoding */
    XFlush(g_display);
    glFinish();
    /* encode */
    rv = xorgxrdp_helper_encoder_encode(mi->ei, mi->enc_texture,
                                        cdata, cdata_bytes);
    return rv;
}

