
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arch.h"
#include "parse.h"
#include "trans.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"

#include "xorgxrdp_helper.h"
#include "xorgxrdp_helper_x11.h"

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*(x)))

struct xorgxrdp_info
{
    struct trans *xorg_trans;
    struct trans *xrdp_trans;
    struct source_info si;
    int resizing;
};

static int g_shmem_id_mapped = 0;
static int g_shmem_id = 0;
static void *g_shmem_pixels = 0;
static int g_display_num = 0;
int xrdp_invalidate = 0;

/*****************************************************************************/
static int
xorg_process_message_61(struct xorgxrdp_info *xi, struct stream *s)
{
    int num_drects;
    int num_crects;
    int flags;
    int shmem_id;
    int shmem_offset;
    int frame_id;
    int width;
    int height;
    int cdata_bytes;
    int index;
    int error;
    struct xh_rect *crects;
    char *bmpdata;

    /* dirty pixels */
    in_uint16_le(s, num_drects);
    in_uint8s(s, 8 * num_drects);
    /* copied pixels */
    in_uint16_le(s, num_crects);
    crects = g_new(struct xh_rect, num_crects);
    for (index = 0; index < num_crects; index++)
    {
        in_uint16_le(s, crects[index].x);
        in_uint16_le(s, crects[index].y);
        in_uint16_le(s, crects[index].w);
        in_uint16_le(s, crects[index].h);
    }
    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_id);
    in_uint32_le(s, shmem_offset);

    in_uint16_le(s, width);
    in_uint16_le(s, height);

    if (xi->resizing == 3)
    {
        if (xrdp_invalidate > 0 && frame_id == 1)
        {
            // Let it through. We are no longer resizing.
            LOGLN((LOG_LEVEL_DEBUG, LOGS "Invalidate received and processing frame ID 1. Unblocking encoder. Invalidate is %d.", LOGP, xrdp_invalidate));
            xi->resizing = 0;
        }
        else
        {
            LOGLN((LOG_LEVEL_DEBUG, LOGS "Blocked Incoming Frame ID %d. Invalidate is %d", LOGP, frame_id, xrdp_invalidate));
            return 0;
        }
    }

    if (xi->resizing > 0)
    {
        return 0;
    }

    bmpdata = NULL;
    if (g_shmem_id_mapped == 0)
    {
        g_shmem_id = shmem_id;
        g_shmem_pixels = (char *) g_shmat(g_shmem_id);
        if (g_shmem_pixels == (void *) -1)
        {
            /* failed */
            g_shmem_id = 0;
            g_shmem_pixels = NULL;
            g_shmem_id_mapped = 0;
        }
        else
        {
            g_shmem_id_mapped = 1;
        }
    }
    else if (g_shmem_id != shmem_id)
    {
        g_shmem_id = shmem_id;
        g_shmdt(g_shmem_pixels);
        g_shmem_pixels = (char *) g_shmat(g_shmem_id);
        if (g_shmem_pixels == (void *) -1)
        {
            /* failed */
            g_shmem_id = 0;
            g_shmem_pixels = NULL;
            g_shmem_id_mapped = 0;
        }
    }
    if (g_shmem_pixels != NULL)
    {
        bmpdata = (char *)g_shmem_pixels + shmem_offset;
    }
    if ((bmpdata != NULL) && (flags & 1))
    {
        cdata_bytes = 16 * 1024 * 1024;
        error = xorgxrdp_helper_x11_encode_pixmap(width, height, 0,
                num_crects, crects,
                bmpdata + 4, &cdata_bytes);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_ERROR, LOGS "error %d", LOGP, error));
        }
        bmpdata[0] = cdata_bytes;
        bmpdata[1] = cdata_bytes >> 8;
        bmpdata[2] = cdata_bytes >> 16;
        bmpdata[3] = cdata_bytes >> 24;
        LOGLND((LOG_LEVEL_INFO, LOGS "cdata_bytes %d", LOGP, cdata_bytes));
    }
    g_free(crects);
    return 0;
}

/*****************************************************************************/
/* data going from xorg to xrdp */
static int
xorg_process_message(struct xorgxrdp_info *xi, struct stream *s)
{
    int type;
    int num;
    int size;
    int index;
    char *phold;
    int width;
    int height;
    int magic;
    int con_id;
    int mon_id;

    in_uint16_le(s, type);
    in_uint16_le(s, num);
    in_uint32_le(s, size);
    if (type == 3)
    {
        for (index = 0; index < num; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, size);
            switch (type)
            {
                case 61:
                    xorg_process_message_61(xi, s);
                    break;
            }
            s->p = phold + size;
        }
        if (xi->resizing > 0)
        {
            return 0;
        }
    }
    else if (type == 100)
    {
        for (index = 0; index < num; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, size);
            LOGLN((LOG_LEVEL_DEBUG, LOGS "100 type %d size %d",
                   LOGP, type, size));
            switch (type)
            {
                case 1:
                    LOGLN((LOG_LEVEL_DEBUG, LOGS "calling "
                           "xorgxrdp_helper_x11_delete_all_pixmaps", LOGP));
                    xorgxrdp_helper_x11_delete_all_pixmaps();
                    if (xi->resizing == 1)
                    {
                        xi->resizing = 2;
                    }
                    break;
                case 2:
                    in_uint16_le(s, width);
                    in_uint16_le(s, height);
                    in_uint32_le(s, magic);
                    in_uint32_le(s, con_id);
                    in_uint32_le(s, mon_id);
                    LOGLN((LOG_LEVEL_DEBUG, LOGS "calling "
                           "xorgxrdp_helper_x11_create_pixmap", LOGP));
                    xorgxrdp_helper_x11_create_pixmap(width, height, magic,
                                                      con_id, mon_id);
                    if (xi->resizing == 2)
                    {
                        xi->resizing = 3;
                    }
                    break;
            }
            s->p = phold + size;
        }
    }
    s->p = s->data;
    return trans_write_copy_s(xi->xrdp_trans, s);
}

/*****************************************************************************/
static int
xorg_data_in(struct trans *trans)
{
    struct stream *s;
    int len;
    struct xorgxrdp_info *xi;

    xi = (struct xorgxrdp_info *) (trans->callback_data);
    s = trans_get_in_s(trans);
    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint8s(s, 4);
            in_uint32_le(s, len);
            if ((len < 0) || (len > 128 * 1024))
            {
                LOGLN((LOG_LEVEL_ERROR, LOGS "bad size %d", LOGP, len));
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len + 8;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (xorg_process_message(xi, s) != 0)
            {
                LOGLN((LOG_LEVEL_ERROR, LOGS "xorg_process_message failed",
                       LOGP));
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 1;
            break;
    }
    return 0;
}

/*****************************************************************************/
/* data going from xrdp to xorg */
static int
xrdp_process_message(struct xorgxrdp_info *xi, struct stream *s)
{
    int len;
    int msg_type1;
    int msg_type2;

    in_uint32_le(s, len);
    in_uint16_le(s, msg_type1);
    if (msg_type1 == 103) // client message
    {
        in_uint32_le(s, msg_type2);
        if (msg_type2 == 200) // invalidate
        {
            LOGLN((LOG_LEVEL_DEBUG, LOGS "Invalidate found (len: %d, msg1: %d, msg2: %d)", LOGP, len, msg_type1, msg_type2));
            ++xrdp_invalidate;
        }
        else if (msg_type2 == 300) // resize
        {
            LOGLN((LOG_LEVEL_DEBUG, LOGS "Resize found (len: %d, msg1: %d, msg2: %d)", LOGP, len, msg_type1, msg_type2));
            xi->resizing = 1;
        }
    }
    //Reset read pointer
    s->p = s->data;
    return trans_write_copy_s(xi->xorg_trans, s);
}

/*****************************************************************************/
static int
xrdp_data_in(struct trans *trans)
{
    struct stream *s;
    int len;
    struct xorgxrdp_info *xi;

    xi = (struct xorgxrdp_info *) (trans->callback_data);
    s = trans_get_in_s(trans);
    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint32_le(s, len);
            if ((len < 0) || (len > 128 * 1024))
            {
                LOGLN((LOG_LEVEL_ERROR, LOGS "bad size %d", LOGP, len));
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (xrdp_process_message(xi, s) != 0)
            {
                LOGLN((LOG_LEVEL_ERROR, LOGS "xrdp_process_message failed",
                       LOGP));
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 4;
            trans->extra_flags = 1;
            break;
    }
    return 0;
}

/*****************************************************************************/
static void
sigpipe_func(int sig)
{
    (void) sig;
}

/*****************************************************************************/
static int
get_log_path(char *path, int bytes)
{
    char *log_path;
    int rv;

    rv = 1;
    log_path = g_getenv("XORGXRDP_HELPER_LOG_PATH");
    if (log_path == 0)
    {
        log_path = g_getenv("XDG_DATA_HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/xrdp");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                rv = 0;
            }
        }
    }
    else
    {
        g_snprintf(path, bytes, "%s", log_path);
        if (g_directory_exist(path) || (g_mkdir(path) == 0))
        {
            rv = 0;
        }
    }
    if (rv != 0)
    {
        log_path = g_getenv("HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/.local");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                g_snprintf(path, bytes, "%s%s", log_path, "/.local/share");
                if (g_directory_exist(path) || (g_mkdir(path) == 0))
                {
                    g_snprintf(path, bytes, "%s%s", log_path, "/.local/share/xrdp");
                    if (g_directory_exist(path) || (g_mkdir(path) == 0))
                    {
                        rv = 0;
                    }
                }
            }
        }
    }
    return rv;
}

/*****************************************************************************/
static enum logLevels
get_log_level(const char *level_str, enum logLevels default_level)
{
    static const char *levels[] = {
        "LOG_LEVEL_ALWAYS",
        "LOG_LEVEL_ERROR",
        "LOG_LEVEL_WARNING",
        "LOG_LEVEL_INFO",
        "LOG_LEVEL_DEBUG",
        "LOG_LEVEL_TRACE"
    };
    unsigned int i;

    if (level_str == NULL || level_str[0] == 0)
    {
        return default_level;
    }
    for (i = 0; i < ARRAYSIZE(levels); ++i)
    {
        if (g_strcasecmp(levels[i], level_str) == 0)
        {
            return (enum logLevels) i;
        }
    }
    return default_level;
}

/*****************************************************************************/
static int
get_display_num_from_display(char *display_text)
{
    int index;
    int mode;
    int host_index;
    int disp_index;
    int scre_index;
    char host[256];
    char disp[256];
    char scre[256];

    g_memset(host, 0, 256);
    g_memset(disp, 0, 256);
    g_memset(scre, 0, 256);

    index = 0;
    host_index = 0;
    disp_index = 0;
    scre_index = 0;
    mode = 0;

    while (display_text[index] != 0)
    {
        if (display_text[index] == ':')
        {
            mode = 1;
        }
        else if (display_text[index] == '.')
        {
            mode = 2;
        }
        else if (mode == 0)
        {
            host[host_index] = display_text[index];
            host_index++;
        }
        else if (mode == 1)
        {
            disp[disp_index] = display_text[index];
            disp_index++;
        }
        else if (mode == 2)
        {
            scre[scre_index] = display_text[index];
            scre_index++;
        }

        index++;
    }

    host[host_index] = 0;
    disp[disp_index] = 0;
    scre[scre_index] = 0;
    g_display_num = g_atoi(disp);
    return 0;
}

/*****************************************************************************/
static int
xorgxrdp_helper_setup_log(void)
{
    struct log_config logconfig;
    enum logLevels log_level;
    char log_path[256];
    char log_file[256];
    char *display_text;
    int error;

    if (get_log_path(log_path, 255) != 0)
    {
        g_writeln("error reading XORGXRDP_HELPER_LOG_PATH and HOME "
                  "environment variable");
        g_deinit();
        return 1;
    }
    display_text = g_getenv("DISPLAY");
    if (display_text != NULL)
    {
        get_display_num_from_display(display_text);
    }
    g_snprintf(log_file, 255, "%s/xorgxrdp_helper.%d.log", log_path,
               g_display_num);
    g_writeln("xorgxrdp_helper::xorgxrdp_helper_setup_log: using "
              "log file [%s]", log_file);
    if (g_file_exist(log_file))
    {
        g_file_delete(log_file);
    }
    log_level = get_log_level(g_getenv("XORGXRDP_HELPER_LOG_LEVEL"),
                              LOG_LEVEL_INFO);
    logconfig.log_file = log_file;
    logconfig.fd = -1;
    logconfig.log_level = log_level;
    logconfig.enable_syslog = 0;
    logconfig.syslog_level = LOG_LEVEL_ALWAYS;
    logconfig.program_name = "xorgxrdp_helper";
    logconfig.enable_console = 0;
    logconfig.enable_pid = 1;
#ifdef LOG_PER_LOGGER_LEVEL
    logconfig.per_logger_level = NULL;
#endif
    error = log_start_from_param(&logconfig);

    return error;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    int xorg_fd;
    int xrdp_fd;
    int error;
    intptr_t robjs[16];
    int robj_count;
    intptr_t wobjs[16];
    int wobj_count;
    int timeout;
    struct xorgxrdp_info xi;

    if (argc < 2)
    {
        g_writeln("need to pass -d");
        return 0;
    }
    if (strcmp(argv[1], "-d") != 0)
    {
        g_writeln("need to pass -d");
        return 0;
    }
    g_init("xorgxrdp_helper");

    if (xorgxrdp_helper_setup_log() != 0)
    {
        return 1;
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "startup", LOGP));
    g_memset(&xi, 0, sizeof(xi));
    g_signal_pipe(sigpipe_func);
    if (xorgxrdp_helper_x11_init() != 0)
    {
        LOGLN((LOG_LEVEL_ERROR, LOGS "xorgxrdp_helper_x11_init failed", LOGP));
        return 1;
    }
    xorg_fd = g_atoi(g_getenv("XORGXRDP_XORG_FD"));
    LOGLN((LOG_LEVEL_INFO, LOGS "xorg_fd: %s", LOGP, g_getenv("XORGXRDP_XORG_FD")));
    xrdp_fd = g_atoi(g_getenv("XORGXRDP_XRDP_FD"));
    LOGLN((LOG_LEVEL_INFO, LOGS "xorg_fd: %s", LOGP, g_getenv("XORGXRDP_XRDP_FD")));

    xi.resizing = 0;

    xi.xorg_trans = trans_create(TRANS_MODE_UNIX, 128 * 1024, 128 * 1024);
    xi.xorg_trans->sck = xorg_fd;
    xi.xorg_trans->status = TRANS_STATUS_UP;
    xi.xorg_trans->trans_data_in = xorg_data_in;
    xi.xorg_trans->header_size = 8;
    xi.xorg_trans->no_stream_init_on_data_in = 1;
    xi.xorg_trans->extra_flags = 1;
    xi.xorg_trans->callback_data = &xi;
    xi.xorg_trans->si = &(xi.si);
    xi.xorg_trans->my_source = XORGXRDP_SOURCE_XORG;

    xi.xrdp_trans = trans_create(TRANS_MODE_UNIX, 128 * 1024, 128 * 1024);
    xi.xrdp_trans->sck = xrdp_fd;
    xi.xrdp_trans->status = TRANS_STATUS_UP;
    xi.xrdp_trans->trans_data_in = xrdp_data_in;
    xi.xrdp_trans->no_stream_init_on_data_in = 1;
    xi.xrdp_trans->header_size = 4;
    xi.xrdp_trans->extra_flags = 1;
    xi.xrdp_trans->callback_data = &xi;
    xi.xrdp_trans->si = &(xi.si);
    xi.xrdp_trans->my_source = XORGXRDP_SOURCE_XRDP;
    for (;;)
    {
        robj_count = 0;
        wobj_count = 0;
        timeout = 0;
        error = trans_get_wait_objs_rw(xi.xorg_trans, robjs, &robj_count,
                                       wobjs, &wobj_count, &timeout);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_INFO, LOGS "trans_get_wait_objs_rw failed",
                   LOGP));
            break;
        }
        error = trans_get_wait_objs_rw(xi.xrdp_trans, robjs, &robj_count,
                                       wobjs, &wobj_count, &timeout);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_INFO, LOGS "trans_get_wait_objs_rw failed",
                   LOGP));
            break;
        }
        error = xorgxrdp_helper_x11_get_wait_objs(robjs, &robj_count);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_INFO, LOGS "xorgxrdp_helper_x11_get_wait_objs "
                   "failed", LOGP));
            break;
        }
        error = g_obj_wait(robjs, robj_count, wobjs, wobj_count, timeout);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_INFO, LOGS "g_obj_wait failed", LOGP));
            break;
        }
        error = trans_check_wait_objs(xi.xorg_trans);
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_INFO, LOGS "xorg_trans trans_check_wait_objs failed",
                   LOGP));
            break;
        }
        error = trans_check_wait_objs(xi.xrdp_trans);
        if (error != 0 && error != 10)
        {
            LOGLN((LOG_LEVEL_ERROR, LOGS "xrdp_trans trans_check_wait_objs failed",
                   LOGP));
            break;
        }
        error = xorgxrdp_helper_x11_check_wait_objs();
        if (error != 0)
        {
            LOGLN((LOG_LEVEL_ERROR, LOGS "xorgxrdp_helper_x11_check_wait_objs "
                   "failed", LOGP));
            break;
        }
    }
    LOGLN((LOG_LEVEL_INFO, LOGS "exit", LOGP));
    return 0;
}
