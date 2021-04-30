/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 *
 * listen for incoming connection
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "log.h"
#include "string_calls.h"

/* 'g_process' is protected by the semaphore 'g_process_sem'.  One thread sets
   g_process and waits for the other to process it */
static tbus g_process_sem = 0;
static struct xrdp_process *g_process = 0;

int
xrdp_listen_conn_in(struct trans *self, struct trans *new_self);

/*****************************************************************************/
static int
xrdp_listen_create_pro_done(struct xrdp_listen *self)
{
    int pid;
    char text[256];

    pid = g_getpid();
    g_snprintf(text, 255, "xrdp_%8.8x_listen_pro_done_event", pid);
    self->pro_done_event = g_create_wait_obj(text);

    if (self->pro_done_event == 0)
    {
        LOG(LOG_LEVEL_WARNING, "Failure creating pro_done_event");
    }

    return 0;
}

/*****************************************************************************/
struct xrdp_listen *
xrdp_listen_create(void)
{
    struct xrdp_listen *self;

    self = (struct xrdp_listen *)g_malloc(sizeof(struct xrdp_listen), 1);
    xrdp_listen_create_pro_done(self);
    self->trans_list = list_create();
    self->process_list = list_create();
    self->fork_list = list_create();

    if (g_process_sem == 0)
    {
        g_process_sem = tc_sem_create(0);
    }
    return self;
}

/*****************************************************************************/
void
xrdp_listen_delete(struct xrdp_listen *self)
{
    int index;
    struct trans *ltrans;

    if (self == NULL)
    {
        return;
    }
    if (self->trans_list != NULL)
    {
        for (index = 0; index < self->trans_list->count; index++)
        {
            ltrans = (struct trans *) list_get_item(self->trans_list, index);
            trans_delete(ltrans);
        }
        list_delete(self->trans_list);
    }

    if (g_process_sem != 0)
    {
        tc_sem_delete(g_process_sem);
        g_process_sem = 0;
    }

    g_delete_wait_obj(self->pro_done_event);
    list_delete(self->process_list);
    list_delete(self->fork_list);
    g_free(self);
}

/*****************************************************************************/
/* returns error */
static int
xrdp_listen_add_pro(struct xrdp_listen *self, struct xrdp_process *process)
{
    list_add_item(self->process_list, (tbus)process);
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_delete_done_pro(struct xrdp_listen *self)
{
    int i;
    struct xrdp_process *pro;

    for (i = self->process_list->count - 1; i >= 0; i--)
    {
        pro = (struct xrdp_process *)list_get_item(self->process_list, i);

        if (pro != 0)
        {
            if (pro->status < 0)
            {
                xrdp_process_delete(pro);
                list_remove_item(self->process_list, i);
            }
        }
    }

    return 0;
}

/*****************************************************************************/
/* i can't get stupid in_val to work, hum using global var for now */
THREAD_RV THREAD_CC
xrdp_process_run(void *in_val)
{
    struct xrdp_process *process;

    LOG_DEVEL(LOG_LEVEL_TRACE, "process started");
    process = g_process;
    g_process = 0;
    tc_sem_inc(g_process_sem);
    xrdp_process_main_loop(process);
    LOG_DEVEL(LOG_LEVEL_TRACE, "process done");
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_get_startup_params(struct xrdp_listen *self)
{
    int fd;
    int index;
    int port_override;
    int fork_override;
    char *val;
    struct list *names;
    struct list *values;
    struct xrdp_startup_params *startup_params;

    startup_params = self->startup_params;
    port_override = startup_params->port[0] != 0;
    fork_override = startup_params->fork;
    fd = g_file_open(startup_params->xrdp_ini);
    if (fd != -1)
    {
        names = list_create();
        names->auto_free = 1;
        values = list_create();
        values->auto_free = 1;
        if (file_read_section(fd, "globals", names, values) == 0)
        {
            for (index = 0; index < names->count; index++)
            {
                val = (char *)list_get_item(names, index);
                if (val != 0)
                {
                    if (g_strcasecmp(val, "port") == 0)
                    {
                        if (port_override == 0)
                        {
                            val = (char *) list_get_item(values, index);
                            g_strncpy(startup_params->port, val,
                                      sizeof(startup_params->port) - 1);
                        }
                    }
                    if (g_strcasecmp(val, "fork") == 0)
                    {
                        if (fork_override == 0)
                        {
                            val = (char *) list_get_item(values, index);
                            startup_params->fork = g_text2bool(val);
                        }
                    }

                    if (g_strcasecmp(val, "tcp_nodelay") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        startup_params->tcp_nodelay = g_text2bool(val);
                    }

                    if (g_strcasecmp(val, "tcp_keepalive") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        startup_params->tcp_keepalive = g_text2bool(val);
                    }

                    if (g_strcasecmp(val, "tcp_send_buffer_bytes") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        startup_params->tcp_send_buffer_bytes = g_atoi(val);
                    }

                    if (g_strcasecmp(val, "tcp_recv_buffer_bytes") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        startup_params->tcp_recv_buffer_bytes = g_atoi(val);
                    }

                    if (g_strcasecmp(val, "use_vsock") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        startup_params->use_vsock = g_text2bool(val);
                    }
                }
            }
        }

        list_delete(names);
        list_delete(values);
        g_file_close(fd);
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_stop_all_listen(struct xrdp_listen *self)
{
    int index;
    struct trans *ltrans;

    if (self->trans_list == NULL)
    {
        return 0;
    }
    for (index = 0; index < self->trans_list->count; index++)
    {
        ltrans = (struct trans *)
                 list_get_item(self->trans_list, index);
        trans_delete(ltrans);
    }
    list_clear(self->trans_list);
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_parse_filename(char *strout, int strout_max,
                           const char *strin, int strin_max)
{
    int count;
    int in;
    int strin_index;
    int strout_index;

    strin_index = 0;
    strout_index = 0;
    in = 0;
    count = 0;
    while ((strin_index < strin_max) && (strout_index < strout_max))
    {
        if (in)
        {
            if ((strin[strin_index] > ' ') && (strin[strin_index] != ','))
            {
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if ((strin[strin_index] > ' ') && (strin[strin_index] != ','))
            {
                in = 1;
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
        }
        strin_index++;
        count++;
    }
    strout[strout_index] = 0;
    return count;
}

/*****************************************************************************/
static int
xrdp_listen_parse_integer(char *strout, int strout_max,
                          const char *strin, int strin_max)
{
    int count;
    int in;
    int strin_index;
    int strout_index;

    strin_index = 0;
    strout_index = 0;
    in = 0;
    count = 0;
    while ((strin_index < strin_max) && (strout_index < strout_max))
    {
        if (in)
        {
            if ((strin[strin_index] >= '0') && (strin[strin_index] <= '9'))
            {
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if ((strin[strin_index] >= '0') && (strin[strin_index] <= '9'))
            {
                in = 1;
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
        }
        strin_index++;
        count++;
    }
    strout[strout_index] = 0;
    return count;
}

/*****************************************************************************/
static int
xrdp_listen_parse_vsock(char *strout, int strout_max,
                        const char *strin, int strin_max)
{
    int count;
    int in;
    int strin_index;
    int strout_index;

    strin_index = 0;
    strout_index = 0;
    in = 0;
    count = 0;
    while ((strin_index < strin_max) && (strout_index < strout_max))
    {
        if (in)
        {
            if ((strin[strin_index] >= '0') && (strin[strin_index] <= '9'))
            {
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if (((strin[strin_index] >= '0') && (strin[strin_index] <= '9')) ||
                    (strin[strin_index] == '-'))
            {
                in = 1;
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
        }
        strin_index++;
        count++;
    }
    strout[strout_index] = 0;
    return count;
}

/*****************************************************************************/
static int
xrdp_listen_parse_ipv4(char *strout, int strout_max,
                       const char *strin, int strin_max)
{
    int count;
    int in;
    int strin_index;
    int strout_index;

    strin_index = 0;
    strout_index = 0;
    in = 0;
    count = 0;
    while ((strin_index < strin_max) && (strout_index < strout_max))
    {
        if (in)
        {
            if (((strin[strin_index] >= '0') && (strin[strin_index] <= '9')) ||
                    (strin[strin_index] == '.'))
            {
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if ((strin[strin_index] >= '0') && (strin[strin_index] <= '9'))
            {
                in = 1;
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
        }
        strin_index++;
        count++;
    }
    strout[strout_index] = 0;
    return count;
}

/*****************************************************************************/
static int
xrdp_listen_parse_ipv6(char *strout, int strout_max,
                       const char *strin, int strin_max)
{
    int count;
    int in;
    int strin_index;
    int strout_index;

    strin_index = 0;
    strout_index = 0;
    in = 0;
    count = 0;
    while ((strin_index < strin_max) && (strout_index < strout_max))
    {
        if (in)
        {
            if (strin[strin_index] != '}')
            {
                strout[strout_index++] = strin[strin_index++];
                count++;
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if (strin[strin_index] == '{')
            {
                in = 1;
                strin_index++;
                count++;
                continue;
            }
        }
        strin_index++;
        count++;
    }
    strout[strout_index] = 0;
    return count;
}

/*****************************************************************************/
/* address and port are assumed 128 bytes */
static int
xrdp_listen_pp(struct xrdp_listen *self, int *index,
               char *address, char *port, int *mode)
{
    struct xrdp_startup_params *startup_params;
    const char *str;
    const char *str_end;
    int lindex;
    int bytes;

    startup_params = self->startup_params;
    lindex = *index;
    str = startup_params->port + lindex;
    str_end = startup_params->port + g_strlen(startup_params->port);
    while (str < str_end)
    {
        if (g_strncmp(str, "unix://.", 8) == 0)
        {
            str += 8;
            lindex += 8;
            address[0] = 0;
            bytes = xrdp_listen_parse_filename(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_UNIX;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp://.:", 8) == 0)
        {
            str += 8;
            lindex += 8;
            g_strncpy(address, "127.0.0.1", 127);
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP4;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp://:", 7) == 0)
        {
            str += 7;
            lindex += 7;
            g_strncpy(address, "0.0.0.0", 127);
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP4;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp://", 6) == 0)
        {
            str += 6;
            lindex += 6;
            bytes = xrdp_listen_parse_ipv4(address, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP4;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp6://.:", 9) == 0)
        {
            str += 9;
            lindex += 9;
            g_strncpy(address, "::1", 127);
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP6;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp6://:", 8) == 0)
        {
            str += 8;
            lindex += 8;
            g_strncpy(address, "::", 127);
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP6;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "tcp6://", 7) == 0)
        {
            str += 7;
            lindex += 7;
            bytes = xrdp_listen_parse_ipv6(address, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_TCP6;
            *index = lindex;
            return 0;
        }
        else if (g_strncmp(str, "vsock://", 8) == 0)
        {
            str += 8;
            lindex += 8;
            bytes = xrdp_listen_parse_vsock(address, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            bytes = xrdp_listen_parse_vsock(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            *mode = TRANS_MODE_VSOCK;
            *index = lindex;
            return 0;
        }
        else if ((str[0] >= '0') && (str[0] <= '9'))
        {
            g_strncpy(address, "0.0.0.0", 127);
            bytes = xrdp_listen_parse_integer(port, 128, str, str_end - str);
            str += bytes;
            lindex += bytes;
            if (startup_params->use_vsock)
            {
                *mode = TRANS_MODE_VSOCK;
            }
            else
            {
                *mode = TRANS_MODE_TCP;
            }
            *index = lindex;
            return 0;
        }
        else
        {
            str++;
            lindex++;
        }
    }
    if (lindex == *index)
    {
        return 1;
    }
    if (str >= str_end)
    {
        return 1;
    }
    *index = lindex;
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_process_startup_params(struct xrdp_listen *self)
{
    int mode; /* TRANS_MODE_TCP*, TRANS_MODE_UNIX, TRANS_MODE_VSOCK */
    int error;
    int cont;
    int bytes;
    int index;
    struct trans *ltrans;
    char address[128];
    char port[128];
    struct xrdp_startup_params *startup_params;

    startup_params = self->startup_params;
    index = 0;
    cont = 1;
    while (cont)
    {
        if (xrdp_listen_pp(self, &index, address, port, &mode) != 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_listen_pp done");
            cont = 0;
            break;
        }
        LOG(LOG_LEVEL_INFO, "address [%s] port [%s] mode %d",
            address, port, mode);
        ltrans = trans_create(mode, 16, 16);
        if (ltrans == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "trans_create failed");
            xrdp_listen_stop_all_listen(self);
            return 1;
        }
        LOG(LOG_LEVEL_INFO, "listening to port %s on %s",
            port, address);
        error = trans_listen_address(ltrans, port, address);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "trans_listen_address failed");
            trans_delete(ltrans);
            xrdp_listen_stop_all_listen(self);
            return 1;
        }
        if ((mode == TRANS_MODE_TCP) ||
                (mode == TRANS_MODE_TCP4) ||
                (mode == TRANS_MODE_TCP6))
        {
            if (startup_params->tcp_nodelay)
            {
                if (g_tcp_set_no_delay(ltrans->sck))
                {
                    LOG(LOG_LEVEL_ERROR, "Error setting tcp_nodelay");
                }
            }
            if (startup_params->tcp_keepalive)
            {
                if (g_tcp_set_keepalive(ltrans->sck))
                {
                    LOG(LOG_LEVEL_ERROR, "Error setting "
                        "tcp_keepalive");
                }
            }
            if (startup_params->tcp_send_buffer_bytes > 0)
            {
                bytes = startup_params->tcp_send_buffer_bytes;
                LOG(LOG_LEVEL_INFO, "setting send buffer to %d bytes",
                    bytes);
                if (g_sck_set_send_buffer_bytes(ltrans->sck, bytes) != 0)
                {
                    LOG(LOG_LEVEL_WARNING, "error setting send buffer");
                }
                else
                {
                    if (g_sck_get_send_buffer_bytes(ltrans->sck, &bytes) != 0)
                    {
                        LOG(LOG_LEVEL_WARNING, "error getting send "
                            "buffer");
                    }
                    else
                    {
                        LOG(LOG_LEVEL_INFO, "send buffer set to %d "
                            "bytes", bytes);
                    }
                }
            }
            if (startup_params->tcp_recv_buffer_bytes > 0)
            {
                bytes = startup_params->tcp_recv_buffer_bytes;
                LOG(LOG_LEVEL_INFO, "setting recv buffer to %d bytes",
                    bytes);
                if (g_sck_set_recv_buffer_bytes(ltrans->sck, bytes) != 0)
                {
                    LOG(LOG_LEVEL_WARNING, "error setting recv buffer");
                }
                else
                {
                    if (g_sck_get_recv_buffer_bytes(ltrans->sck, &bytes) != 0)
                    {
                        LOG(LOG_LEVEL_WARNING, "error getting recv "
                            "buffer");
                    }
                    else
                    {
                        LOG(LOG_LEVEL_INFO, "recv buffer set to %d "
                            "bytes", bytes);
                    }
                }
            }
        }
        ltrans->trans_conn_in = xrdp_listen_conn_in;
        ltrans->callback_data = self;
        list_add_item(self->trans_list, (intptr_t) ltrans);
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_listen_fork(struct xrdp_listen *self, struct trans *server_trans)
{
    int pid;
    int index;
    struct xrdp_process *process;
    struct trans *ltrans;

    pid = g_fork();

    if (pid == 0)
    {
        /* child */
        /* recreate some main globals */
        xrdp_child_fork();
        /* recreate the process done wait object, not used in fork mode */
        /* close, don't delete this */
        g_close_wait_obj(self->pro_done_event);
        xrdp_listen_create_pro_done(self);
        /* delete listener, child need not listen */
        for (index = 0; index < self->trans_list->count; index++)
        {
            ltrans = (struct trans *) list_get_item(self->trans_list, index);
            trans_delete_from_child(ltrans);
        }
        list_delete(self->trans_list);
        self->trans_list = NULL;
        /* new connect instance */
        process = xrdp_process_create(self, 0);
        process->server_trans = server_trans;
        g_process = process;
        xrdp_process_run(0);
        tc_sem_dec(g_process_sem);
        xrdp_process_delete(process);
        /* mark this process to exit */
        g_set_term(1);
        return 1;
    }

    /* parent */
    trans_delete(server_trans);
    return 0;
}

/*****************************************************************************/
/* a new connection is coming in */
int
xrdp_listen_conn_in(struct trans *self, struct trans *new_self)
{
    struct xrdp_process *process;
    struct xrdp_listen *lis;

    lis = (struct xrdp_listen *)(self->callback_data);

    if (lis->startup_params->fork)
    {
        list_add_item(lis->fork_list, (intptr_t) new_self);
        return 0;
    }

    process = xrdp_process_create(lis, lis->pro_done_event);

    if (xrdp_listen_add_pro(lis, process) == 0)
    {
        /* start thread */
        process->server_trans = new_self;
        g_process = process;
        tc_thread_create(xrdp_process_run, 0);
        tc_sem_dec(g_process_sem); /* this will wait */
    }
    else
    {
        xrdp_process_delete(process);
    }

    return 0;
}

/*****************************************************************************/
/* wait for incoming connections
   passes through trans_listen_address return value */
int
xrdp_listen_main_loop(struct xrdp_listen *self)
{
    int robjs_count;
    int cont;
    int index;
    int timeout;
    intptr_t robjs[32];
    intptr_t term_obj;
    intptr_t sync_obj;
    intptr_t done_obj;
    struct trans *ltrans;

    self->status = 1;
    if (xrdp_listen_get_startup_params(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_listen_main_loop: xrdp_listen_get_port failed");
        self->status = -1;
        return 1;
    }
    if (xrdp_listen_process_startup_params(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_listen_main_loop: xrdp_listen_get_port failed");
        self->status = -1;
        return 1;
    }
    term_obj = g_get_term_event(); /*Global termination event */
    sync_obj = g_get_sync_event();
    done_obj = self->pro_done_event;
    cont = 1;
    while (cont)
    {
        /* build the wait obj list */
        robjs_count = 0;
        robjs[robjs_count++] = term_obj;
        robjs[robjs_count++] = sync_obj;
        robjs[robjs_count++] = done_obj;
        timeout = -1;

        for (index = 0; index < self->trans_list->count; index++)
        {
            ltrans = (struct trans *)
                     list_get_item(self->trans_list, index);
            if (trans_get_wait_objs(ltrans, robjs, &robjs_count) != 0)
            {
                cont = 0;
                break;
            }
        }
        if (cont == 0)
        {
            break;
        }

        /* wait - timeout -1 means wait indefinitely*/
        if (g_obj_wait(robjs, robjs_count, 0, 0, timeout) != 0)
        {
            /* error, should not get here */
            g_sleep(100);
        }

        if (g_is_wait_obj_set(term_obj)) /* termination called */
        {
            LOG(LOG_LEVEL_INFO,
                "Received termination signal, stopping the server accept new "
                "connections thread");
            break;
        }

        /* some function must be processed by this thread */
        if (g_is_wait_obj_set(sync_obj))
        {
            g_reset_wait_obj(sync_obj);
            g_process_waiting_function(); /* run the function */
        }

        if (g_is_wait_obj_set(done_obj)) /* pro_done_event */
        {
            g_reset_wait_obj(done_obj);
            /* a process has died remove it from lists*/
            xrdp_listen_delete_done_pro(self);
        }

        /* Run the callback when accept() returns a new socket*/
        for (index = 0; index < self->trans_list->count; index++)
        {
            ltrans = (struct trans *)
                     list_get_item(self->trans_list, index);
            if (trans_check_wait_objs(ltrans) != 0)
            {
                cont = 0;
                break;
            }
        }
        if (cont == 0)
        {
            break;
        }
        while (self->fork_list->count > 0)
        {
            ltrans = (struct trans *) list_get_item(self->fork_list, 0);
            list_remove_item(self->fork_list, 0);
            if (xrdp_listen_fork(self, ltrans) != 0)
            {
                cont = 0;
                break;
            }
        }
        if (cont == 0)
        {
            break;
        }
    }

    /* stop listening */
    xrdp_listen_stop_all_listen(self);

    /* second loop to wait for all process threads to close */
    cont = 1;

    while (cont)
    {
        if (self->process_list->count == 0)
        {
            break;
        }

        timeout = -1;
        /* build the wait obj list */
        robjs_count = 0;
        robjs[robjs_count++] = sync_obj;
        robjs[robjs_count++] = done_obj;

        /* wait - timeout -1 means wait indefinitely*/
        if (g_obj_wait(robjs, robjs_count, 0, 0, timeout) != 0)
        {
            /* error, should not get here */
            g_sleep(100);
        }

        /* some function must be processed by this thread */
        if (g_is_wait_obj_set(sync_obj))
        {
            g_reset_wait_obj(sync_obj);
            g_process_waiting_function(); /* run the function that is waiting*/
        }

        if (g_is_wait_obj_set(done_obj)) /* pro_done_event */
        {
            g_reset_wait_obj(done_obj);
            xrdp_listen_delete_done_pro(self);
        }
    }

    self->status = -1;
    return 0;
}

/*****************************************************************************/
/* returns 0 if xrdp can listen
   returns 1 if xrdp cannot listen */
int
xrdp_listen_test(struct xrdp_startup_params *startup_params)
{
    struct xrdp_listen *xrdp_listen;

    xrdp_listen = xrdp_listen_create();
    xrdp_listen->startup_params = startup_params;
    if (xrdp_listen_get_startup_params(xrdp_listen) != 0)
    {
        xrdp_listen_delete(xrdp_listen);
        return 1;
    }
    if (xrdp_listen_process_startup_params(xrdp_listen) != 0)
    {
        xrdp_listen_delete(xrdp_listen);
        return 1;
    }
    xrdp_listen_delete(xrdp_listen);
    return 0;
}
