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
 * Checks the message header in the input stream, and gets the size
 */
static enum libipm_status
validate_msg_header(struct trans *trans, int *size)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    enum libipm_status rv = E_LI_BAD_HEADER;

    int version;
    int facility;
    int reserved;

    in_uint16_le(trans->in_s, version);
    in_uint16_le(trans->in_s, *size);
    in_uint16_le(trans->in_s, facility);
    in_uint16_le(trans->in_s, priv->in_msgno);
    in_uint32_le(trans->in_s, reserved);

    if (version != LIBIPM_VERSION)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unexpected version number %d from IPM", version);
    }
    else if (*size < HEADER_SIZE || *size > LIBIPM_MAX_MSG_SIZE)
    {
        LOG(LOG_LEVEL_ERROR,
            "Invalid message length %d from IPM", *size);
    }
    else if (facility != priv->facility)
    {
        LOG(LOG_LEVEL_ERROR,
            "Invalid facility %d from IPM - expected %d",
            facility, priv->facility);
    }
    else if (reserved != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Invalid reserved field %d from IPM", reserved);
    }
    else
    {
        rv = E_LI_SUCCESS;
    }

    return rv;
}

/*****************************************************************************/

enum libipm_status
libipm_msg_in_check_available(struct trans *trans, int *available)
{
    enum libipm_status rv = E_LI_SUCCESS;

    *available = 0;

    if (trans == NULL || trans->extra_data == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Failed devel check");
        rv = E_LI_PROGRAM_ERROR;
    }
    else if (trans->status != TRANS_STATUS_UP)
    {
        rv = E_LI_PROGRAM_ERROR; /* Caller should have checked this */
    }
    else
    {
        unsigned int len = trans->in_s->end - trans->in_s->data; /* Data read so far */

        if (len == trans->header_size)
        {
            if (trans->extra_flags == 0)
            {
                /* We've read the header so far - validate it */
                int size;
                rv = validate_msg_header(trans, &size);
                if (rv == 0)
                {
                    /* Header is OK */
                    trans->extra_flags = 1;
                    trans->header_size = size;

                    *available = (size == HEADER_SIZE);
                }
            }
            else
            {
                *available = 1;
            }
        }
    }

    return rv;
}

/*****************************************************************************/

enum libipm_status
libipm_msg_in_wait_available(struct trans *trans)
{
    tbus wobj[1];
    int ocnt = 0;
    enum libipm_status rv = E_LI_SUCCESS;

    if (trans == NULL || trans->extra_data == NULL ||
            trans->status != TRANS_STATUS_UP)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Failed devel check");
        rv = E_LI_PROGRAM_ERROR;
    }
    else if (trans_get_wait_objs(trans, wobj, &ocnt) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't get wait object for libipm transport");
        rv = E_LI_TRANSPORT_ERROR;
    }
    else
    {
        int gotmsg = 0;
        /* Prevent trans_check_wait_objs() actioning any callcacks
         * when the message is complete */
        ttrans_data_in saved_trans_data_in = trans->trans_data_in;
        trans->trans_data_in = NULL;

        while (rv == E_LI_SUCCESS && !gotmsg)
        {
            if (g_obj_wait(wobj, ocnt, NULL, 0, -1) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Error waiting on libipm transport");
                rv = E_LI_TRANSPORT_ERROR;
            }
            else if (trans_check_wait_objs(trans) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Error reading libipm transport");
                rv = E_LI_TRANSPORT_ERROR;
            }
            else
            {
                /* This call logs errors already */
                rv = libipm_msg_in_check_available(trans, &gotmsg);
            }
        }

        /* Restore transport callback operation */
        trans->trans_data_in = saved_trans_data_in;
    }

    return rv;
}

/*****************************************************************************/

unsigned short
libipm_msg_in_get_msgno(const struct trans *trans)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    return (priv == NULL) ? 0 : priv->in_msgno;
}

/*****************************************************************************/

char
libipm_msg_in_peek_type(struct trans *trans)
{
    int result;

    if (s_check_rem(trans->in_s, 1))
    {
        char c;
        in_uint8_peek(trans->in_s, c);
        if (g_strchr(libipm_valid_type_chars, c) != NULL)
        {
            result = c; /* Only return valid characters */
        }
        else
        {
            result = '?';
        }
    }
    else
    {
        result = '\0';
    }

    return result;
}

/**************************************************************************//**
 * Log an input parsing error
 *
 * @param trans libipm transport
 * @param format printf-like format descriptor
 */
printflike(2, 3) static void
log_parse_error(struct trans *trans, const char *format, ...)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    const char *msgno_str = NULL;
    char msgno_str_buff[32];

    char buff[256];
    unsigned int len;

    /* Find a string for the message number */
    if (priv->msgno_to_str != NULL)
    {
        msgno_str = priv->msgno_to_str(priv->in_msgno);
    }
    if (msgno_str == NULL)
    {
        g_snprintf(msgno_str_buff, sizeof(msgno_str_buff),
                   "[code #%d]", priv->in_msgno);
        msgno_str = msgno_str_buff;
    }

    len = g_snprintf(buff, sizeof(buff),
                     "Error parsing ipm message for %s, parameter %d :",
                     msgno_str, priv->in_param_count);
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
not_enough_input_msg = "Input buffer overflow for '%c'";

/**************************************************************************//**
 * Extract a bool from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr Pointer to pointer to receive the value
 * @return != 0 for error
 *
 * The value must be a 0 or 1 on-the-wire, or an error is returned.
 */
static enum libipm_status
extract_bool_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;
    int b;

    if (!s_check_rem(s, 1))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        in_uint8(s, b);
        if (b < 0 || b > 1)
        {
            log_parse_error(trans, "Boolean has value other than 0/1");
            rv = E_LI_BAD_VALUE;
        }
        else
        {
            int *tmp = va_arg(*argptr, int *);
            *tmp = b;
        }
    }
    return rv;
}

/**************************************************************************//**
 * Extract an octet from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_int8_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;

    if (!s_check_rem(s, 1))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        unsigned char *tmp = va_arg(*argptr, unsigned char *);
        in_uint8(s, *tmp);
    }
    return rv;
}


/**************************************************************************//**
 * Extract a 16-bit integer from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_int16_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;

    if (!s_check_rem(s, 2))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        uint16_t *tmp = va_arg(*argptr, uint16_t *);
        /*
         * C99 7.18.1.1 requires int16_t (if present) to be a two's
         * complement representation, so this line is valid for both
         * int16_t and uint16_t */
        in_uint16_le(s, *tmp);
    }
    return rv;
}

/**************************************************************************//**
 * Extract a 32-bit integer from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_int32_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;

    if (!s_check_rem(s, 4))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        uint32_t *tmp = va_arg(*argptr, uint32_t *);
        /*
         * C99 7.18.1.1 requires int32_t (if present) to be a two's
         * complement representation, so this line is valid for both
         * int32_t and uint32_t */
        in_uint32_le(s, *tmp);
    }
    return rv;
}

/**************************************************************************//**
 * Extract a 64-bit integer from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_int64_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;

    if (!s_check_rem(s, 8))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        uint64_t *tmp = va_arg(*argptr, uint64_t *);
        /*
         * C99 7.18.1.1 requires int64_t (if present) to be a two's
         * complement representation, so this line is valid for both
         * int64_t and uint64_t */
        in_uint64_le(s, *tmp);
    }
    return rv;
}

/**************************************************************************//**
 * Extract a char * pointer from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_char_ptr_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;

    /* Look for a string terminator in the rest of the input stream */
    char *termptr = g_strnchr(s->p,  '\0', s->end - s->p);

    if (termptr == NULL)
    {
        log_parse_error(trans, "Unterminated string value");
        rv = E_LI_BAD_VALUE;
    }
    else
    {
        char **tmp = va_arg(*argptr, char **);

        *tmp = s->p;
        s->p = termptr + 1;
    }
    return rv;
}


/**************************************************************************//**
 * Extract a file descriptor from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to receive the value
 * @return != 0 for error
 */
static enum libipm_status
extract_fd_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    /* File descriptor available? */
    if (priv->in_fd_index >= priv->in_fd_count)
    {
        log_parse_error(trans, "No file descriptors available");
        rv = E_LI_TOO_MANY_FDS;
    }
    else
    {
        int *tmp = va_arg(*argptr, int *);

        *tmp = priv->in_fds[priv->in_fd_index++];
    }
    return rv;
}

/**************************************************************************//**
 * Extract a fixed size block from the input stream
 *
 * @param c Type letter which triggered the call
 * @param trans libipm transport
 * @param argptr argptr to pointer to block descriptor
 * @return != 0 for error
 */
static enum libipm_status
extract_fsb_type(char c, struct trans *trans, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;
    const struct libipm_fsb *fsb = va_arg(*argptr, const struct libipm_fsb *);

    if (fsb == NULL || fsb->data == NULL)
    {
        log_parse_error(trans, "Malformed descriptor for '%c'", c);
        rv = E_LI_PROGRAM_ERROR;
    }
    else if (!s_check_rem(s, 2))
    {
        log_parse_error(trans, not_enough_input_msg, c);
        rv = E_LI_BUFFER_OVERFLOW;
    }
    else
    {
        unsigned int len;
        in_uint16_le(s, len);

        if (len != fsb->datalen)
        {
            log_parse_error(trans, "Type '%c'. Expected %u bytes, but got %u",
                            c, fsb->datalen, len);
            rv = E_LI_BAD_VALUE;
        }
        else if (!s_check_rem(s, len))
        {
            log_parse_error(trans, not_enough_input_msg, c);
            rv = E_LI_BUFFER_OVERFLOW;
        }
        else
        {
            in_uint8a(s, fsb->data, len);
            rv = E_LI_SUCCESS;
        }
    }
    return rv;
}

/**************************************************************************//**
 * Local function to pull data from the input stream
 *
 * @param trans libipm transport
 * @param format type-system compatible string
 * @param argptr Pointers to variables to receive extracted data
 * @pre - trans->priv has been checked to be non-NULL
 * @return != 0 for error
 */
static enum libipm_status
libipm_msg_in_parsev(struct trans *trans, const char *format, va_list *argptr)
{
    enum libipm_status rv = E_LI_SUCCESS;
    struct stream *s = trans->in_s;
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;
    const char *cp;

    if (format != NULL)
    {
        for (cp = format; rv == 0 && *cp != '\0' ; ++cp)
        {
            char c = *cp;
            char actual_c;
            ++priv->in_param_count; /* Count the parameter */

            /* Check the type of the input is supported */
            if (g_strchr(libipm_valid_type_chars, c) == NULL)
            {
                log_parse_error(trans,
                                "Type code '%c' is not supported", c);
                rv = E_LI_UNSUPPORTED_TYPE;
                break;
            }

            /* Check the type of the input matches the stream */
            if (!s_check_rem(s, 1))
            {
                log_parse_error(trans, not_enough_input_msg, c);
                rv = E_LI_BUFFER_OVERFLOW;
                break;
            }
            in_uint8(s, actual_c);
            if (c != actual_c)
            {
                log_parse_error(trans, "Expected '%c', got '%c'", c, actual_c);
                rv = E_LI_UNEXPECTED_TYPE;
                break;
            }

            switch (c)
            {
                case 'y':
                    rv = extract_int8_type(c, trans, argptr);
                    break;

                case 'b':
                    rv = extract_bool_type(c, trans, argptr);
                    break;

                case 'n':
                case 'q':
                    rv = extract_int16_type(c, trans, argptr);
                    break;

                case 'i':
                case 'u':
                    rv = extract_int32_type(c, trans, argptr);
                    break;

                case 'x':
                case 't':
                    rv = extract_int64_type(c, trans, argptr);
                    break;

                case 's':
                    rv = extract_char_ptr_type(c, trans, argptr);
                    break;

                case 'h':
                    rv = extract_fd_type(c, trans, argptr);
                    break;

                case 'B':
                    rv = extract_fsb_type(c, trans, argptr);
                    break;

                default:
                    log_parse_error(trans,
                                    "Reserved type code '%c' "
                                    "is unimplemented", c);
                    rv = E_LI_UNIMPLEMENTED_TYPE;
                    break;
            }
        }
    }

    return rv;
}

/*****************************************************************************/

enum libipm_status
libipm_msg_in_parse(struct trans *trans, const char *format, ...)
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
        rv = libipm_msg_in_parsev(trans, format, &argptr);
        va_end(argptr);
    }

    return rv;
}

/*****************************************************************************/

void
libipm_msg_in_reset(struct trans *trans)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    if (priv == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "uninitialised transport");
    }
    else
    {
        if ((priv->flags & LIBIPM_E_MSG_IN_ERASE_AFTER_USE) != 0)
        {
            struct stream *s = trans->in_s;
            g_memset(s->data, '\0', s->end - s->data);
            priv->flags &= ~LIBIPM_E_MSG_IN_ERASE_AFTER_USE;
        }
        priv->in_msgno = 0;
        priv->in_param_count = 0;
        libipm_msg_in_close_file_descriptors(trans);
    }

    trans->extra_flags = 0;
    trans->header_size = HEADER_SIZE;
    trans->no_stream_init_on_data_in = 1;
    init_stream(trans->in_s, LIBIPM_MAX_MSG_SIZE);
}
