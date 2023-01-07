/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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
 * @file    libipm/libipm.c
 * @brief   Inter-Process Messaging building and parsing definitions
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include "libipm.h"
#include "libipm_private.h"
#include "libipm_facilities.h"
#include "trans.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"

/**************************************************************************//**
 * Log an output appending error
 *
 * @param trans libipm transport
 * @param format printf-like format descriptor
 */
printflike(2, 3) static void
log_append_error(struct trans *trans, const char *format, ...)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    const char *msgno_str = NULL;
    char msgno_str_buff[32];

    char buff[256];
    unsigned int len;

    /* Find a string for the message number */
    if (priv->msgno_to_str != NULL)
    {
        msgno_str = priv->msgno_to_str(priv->out_msgno);
    }
    if (msgno_str == NULL)
    {
        g_snprintf(msgno_str_buff, sizeof(msgno_str_buff),
                   "[code #%d]", priv->out_msgno);
        msgno_str = msgno_str_buff;
    }

    len = g_snprintf(buff, sizeof(buff),
                     "Error creating ipm message for %s, parameter %d :",
                     msgno_str, priv->out_param_count);
    if (len < sizeof(buff))
    {
        va_list ap;

        va_start(ap, format);
        vsnprintf(&buff[len], sizeof(buff) - len, format, ap);
        va_end(ap);
    }
    LOG(LOG_LEVEL_ERROR, "%s", buff);
}

/* Common format string for overflow reporting */
static const char *
not_enough_output_msg = "Not enough space in output buffer for '%c'";
/* Common format string for bad value reporting */
static const char *
bad_value_msg = "Type '%c' has unsupported value '%d'";

/**************************************************************************//**
 * Add a bool to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack (promoted to int)
 * @return != 0 for error
 *
 * The boolean value must be a 0 or a 1.
 */
static enum libipm_status
append_bool_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    if (!s_check_rem_out(s, 1 + 1))
    {
        log_append_error(trans, not_enough_output_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        int tmp = va_arg(*argptr, int);
        if (tmp < 0 || tmp > 1)
        {
            log_append_error(trans, bad_value_msg, c, tmp);
            rv = E_LI_BAD_VALUE;
        }
        else
        {
            out_uint8(s, c);
            out_uint8(s, tmp);
        }
    }
    return rv;
}

/**************************************************************************//**
 * Add an octet to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack (promoted to int)
 * @return != 0 for error
 */
static enum libipm_status
append_int8_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    if (!s_check_rem_out(s, 1 + 1))
    {
        log_append_error(trans, not_enough_output_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        int tmp = va_arg(*argptr, int);
        if (tmp < 0 || tmp > 255)
        {
            log_append_error(trans, bad_value_msg, c, tmp);
            rv = E_LI_BAD_VALUE;
        }
        else
        {
            out_uint8(s, c);
            out_uint8(s, tmp);
        }
    }
    return rv;
}

/**************************************************************************//**
 * Add an 16-bit integer to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack (promoted to int)
 * @return != 0 for error
 */
static enum libipm_status
append_int16_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    if (!s_check_rem_out(s, 1 + 2))
    {
        log_append_error(trans, not_enough_output_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        int tmp = va_arg(*argptr, int);
        if ((c == 'n' && (tmp < -0x8000 || tmp > 0x7fff)) ||
                (c == 'q' && (tmp < 0 || tmp > 0xffff)))
        {
            log_append_error(trans, bad_value_msg, c, tmp);
            rv = E_LI_BAD_VALUE;
        }
        else
        {
            out_uint8(s, c);
            out_uint16_le(s, tmp);
        }
    }
    return rv;
}

/**************************************************************************//**
 * Add a 32-bit integer to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack
 * @return != 0 for error
 */
static enum libipm_status
append_int32_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    if (!s_check_rem_out(s, 1 + 4))
    {
        log_append_error(trans, not_enough_output_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        /* If int is bigger than 4 bytes, the argument will be
         * promoted to 'int' rather than 'int32_t'/'uint32_t',
         * and we will need to check the specified value is in range.
         */
#if SIZEOF_INT > 4
        int tmp = va_arg(*argptr, int);
        if ((c == 'i' && (tmp < -0x80000000 || tmp > 0x7fffffff)) ||
                (c == 'u' && (tmp < 0 || tmp > 0xffffffff)))
        {
            log_append_error(trans, bad_value_msg, c, tmp);
            rv = E_LI_BAD_VALUE;
        }
        else
        {
            out_uint8(s, c);
            out_uint32_le(s, tmp);
        }
#else
        uint32_t tmp = va_arg(*argptr, uint32_t);
        out_uint8(s, c);
        out_uint32_le(s, tmp);
#endif
    }
    return rv;
}

/**************************************************************************//**
 * Add a 64-bit integer to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack
 * @return != 0 for error
 */
static enum libipm_status
append_int64_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    if (!s_check_rem_out(s, 1 + 8))
    {
        log_append_error(trans, not_enough_output_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        uint64_t tmp = va_arg(*argptr, uint64_t);
        out_uint8(s, c);
        out_uint64_le(s, tmp);
    }
    return rv;
}

/**************************************************************************//**
 * Add a terminated string to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack (promoted to int)
 * @return != 0 for error
 *
 * NULL pointers are not allowed for the string.
 */
static enum libipm_status
append_char_ptr_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    const char *str = va_arg(*argptr, const char *);
    if (str == NULL)
    {
        log_append_error(trans, "String cannot be NULL");
        rv = E_LI_PROGRAM_ERROR;
    }
    else
    {
        unsigned int len = g_strlen(str);

        if ((len > (LIBIPM_MAX_MSG_SIZE - HEADER_SIZE - 1)) ||
                (!s_check_rem_out(s, 1 + 1 + len)))
        {
            log_append_error(trans,
                             "Not enough space in output buffer for "
                             "'%c'[len=%u]", c, len);
            rv = E_LI_BUFFER_OVERFLOW;
        }
        else
        {
            out_uint8(s, c);
            out_uint8p(s, str, len + 1);
        }
    }
    return rv;
}

/**************************************************************************//**
 * Add a file descriptor to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to value in argument stack (promoted to int)
 * @return != 0 for error
 */
static enum libipm_status
append_fd_type(char c, va_list *argptr, struct trans *trans)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    int fd = va_arg(*argptr, int);
    if (fd < 0)
    {
        log_append_error(trans, "File descriptor cannot be < 0");
        rv = E_LI_PROGRAM_ERROR;
    }
    else if (!s_check_rem_out(s, 1))
    {
        log_append_error(trans,
                         "Not enough space in output buffer for '%c'", c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else if (priv->out_fd_count >= MAX_FD_PER_MSG)
    {
        log_append_error(trans,
                         "Too many file descriptors for '%c'", c);
        rv = E_LI_TOO_MANY_FDS;
    }
    else
    {
        out_uint8(s, c);
        priv->out_fds[priv->out_fd_count++] = fd;
    }

    return rv;
}

/**************************************************************************//**
 * Append a fixed size block to the output stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to block descriptor
 * @return != 0 for error
 */
static enum libipm_status
append_fsb_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->out_s;
    const struct libipm_fsb *fsb = va_arg(*argptr, const struct libipm_fsb *);
    if (fsb == NULL || fsb->data == NULL)
    {
        log_append_error(trans, "Malformed descriptor for '%c'", c);
        rv = E_LI_PROGRAM_ERROR;
    }
    else
    {
        unsigned int len = fsb->datalen;

        if ((len > (LIBIPM_MAX_MSG_SIZE - HEADER_SIZE - 2)) ||
                (!s_check_rem_out(s, 1 + 2 + len)))
        {
            log_append_error(trans, "Not enough space in output buffer"
                             " for '%c'[len=%u]", c, len);
            rv = E_LI_BUFFER_OVERFLOW;
        }
        else
        {
            out_uint8(s, c);
            out_uint16_le(s, len);
            out_uint8a(s, fsb->data, len);
        }
    }
    return rv;
}

/**************************************************************************//**
 * Local function to append data to the output stream
 *
 * @param trans libipm transport
 * @param format type-system compatible string
 * @param argptr Variables containing data to append
 * @pre - trans->priv has been checked to be non-NULL
 * @return != 0 for error
 */
static enum libipm_status
libipm_msg_out_appendv(struct trans *trans, const char *format, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    const char *cp;

    if (format != NULL)
    {
        for (cp = format; rv == 0 && *cp != '\0' ; ++cp)
        {
            char c = *cp;
            ++priv->out_param_count; /* Count the parameter */
            if (g_strchr(libipm_valid_type_chars, c) == NULL)
            {
                log_append_error(trans,
                                 "Type code '%c' is not supported", c);
                rv = E_LI_UNSUPPORTED_TYPE;
                break;
            }

            switch (c)
            {
                case 'y':
                    rv = append_int8_type(c, trans, argptr);
                    break;

                case 'b':
                    rv = append_bool_type(c, trans, argptr);
                    break;

                case 'n':
                case 'q':
                    rv = append_int16_type(c, trans, argptr);
                    break;

                case 'i':
                case 'u':
                    rv = append_int32_type(c, trans, argptr);
                    break;

                case 'x':
                case 't':
                    rv = append_int64_type(c, trans, argptr);
                    break;

                case 's':
                    rv = append_char_ptr_type(c, trans, argptr);
                    break;

                case 'h':
                    rv = append_fd_type(c, argptr, trans);
                    break;

                case 'B':
                    rv = append_fsb_type(c, trans, argptr);
                    break;

                default:
                    log_append_error(trans,
                                     "Reserved type code '%c' "
                                     "is unimplemented", c);
                    rv = E_LI_UNIMPLEMENTED_TYPE;
                    break;
            }
        }
    }

    return rv;
}

/**************************************************************************//**
 * Prepare the transport to build an output message
 * @param trans libipm trans
 * @param msgno Number of message
 * @return != 0 for error
 */
static void
init_output_buffer(struct trans *trans, unsigned short msgno)
{
    struct stream *s = trans->out_s;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    init_stream(s, LIBIPM_MAX_MSG_SIZE);

    /* Leave space for header */
    s_push_layer(s, iso_hdr, HEADER_SIZE);

    priv->out_msgno = msgno;
    priv->out_param_count = 0;
    priv->out_fd_count = 0;
}

/*****************************************************************************/

enum libipm_status
libipm_msg_out_init(struct trans *trans, unsigned short msgno,
                    const char *format, ...)
{
    enum libipm_status rv;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    if (priv == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "uninitialised transport");
        rv = E_LI_PROGRAM_ERROR;
    }
    else
    {
        va_list argptr;

        init_output_buffer(trans, msgno);

        va_start(argptr, format);
        rv = libipm_msg_out_appendv(trans, format, &argptr);
        va_end(argptr);
    }

    return rv;
}

/*****************************************************************************/

enum libipm_status
libipm_msg_out_append(struct trans *trans, const char *format, ...)
{
    enum libipm_status rv;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    if (priv == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "uninitialised transport");
        rv = E_LI_PROGRAM_ERROR;
    }
    else
    {
        va_list argptr;

        va_start(argptr, format);
        rv = libipm_msg_out_appendv(trans, format, &argptr);
        va_end(argptr);
    }

    return rv;
}

/*****************************************************************************/
void
libipm_msg_out_mark_end(struct trans *trans)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    if (priv == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "uninitialised transport");
    }
    else
    {
        struct stream *s = trans->out_s;
        s_mark_end(s);
        s_pop_layer(s, iso_hdr);

        /* Write the message header */
        out_uint16_le(s, LIBIPM_VERSION);
        out_uint16_le(s, s->end - s->data);
        out_uint16_le(s, priv->facility);
        out_uint16_le(s, priv->out_msgno);
        out_uint32_le(s, 0); /* Reserved */

        /* Move the output pointer back to the end so another
         * append works, and so libipm_msg_out_erase() knows
         * exactly what to do */
        s->p = s->end;
    }
}

/*****************************************************************************/
enum libipm_status
libipm_msg_out_simple_send(struct trans *trans, unsigned short msgno,
                           const char *format, ...)
{
    enum libipm_status rv;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    if (priv == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "uninitialised transport");
        rv = E_LI_PROGRAM_ERROR;
    }
    else
    {
        va_list argptr;

        va_start(argptr, format);
        init_output_buffer(trans, msgno);

        rv = libipm_msg_out_appendv(trans, format, &argptr);
        if (rv == E_LI_SUCCESS)
        {
            libipm_msg_out_mark_end(trans);
            if (trans_force_write(trans) != 0)
            {
                rv = E_LI_TRANSPORT_ERROR;
            }
        }
        va_end(argptr);
    }

    return rv;
}

/*****************************************************************************/

void
libipm_msg_out_erase(struct trans *trans)
{
    struct stream *s = trans->out_s;
    if (s->size > 0 && s->data != 0)
    {
        g_memset(s->data, '\0', s->p - s->data);
    }
}
