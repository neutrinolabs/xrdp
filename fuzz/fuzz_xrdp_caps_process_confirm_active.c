#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "os_calls.h"
#include "parse.h"

#include "libxrdp.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

/*
 TODO:
    /home/user/CLionProjects/xrdp/libxrdp/xrdp_caps.c:777:5: runtime error: load of misaligned address 0x7b4068fe586b for type 'unsigned short', which requires 2 byte alignment
*/

extern int do_fuzz(const uint8_t *data, size_t data_size);
int do_fuzz(const uint8_t *data, size_t data_size)
{
    if (data_size == 0)
        return 0;

    struct stream *s;

    make_stream(s);
    init_stream(s, data_size);

    // Copy data to stream
    out_uint8p(s, data, data_size); // TODO: avoid copy
    s_mark_end(s);

    // Reset stream for reading
    s->p = s->data;

    struct xrdp_rdp self; // TODO: initialize?
    memset(&self.client_info, 0, sizeof(struct xrdp_client_info)); // TODO: properly initialize

    // TODO: fix "The log reference is NULL - log not initialized properly"

    xrdp_caps_process_confirm_active(&self, s);

    free_stream(s);

    return 0;
}
