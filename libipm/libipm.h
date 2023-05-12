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
 * @file    libipm/libipm.h
 * @brief   Inter-Process Messaging building and parsing - public declarations
 *
 * The functions in this file are inspired by the D-Bus type system
 * (see https://dbus.freedesktop.org/doc/dbus-specification.html),
 * and the sd-bus library API.
 */

#if !defined(LIBIPM_H)
#define LIBIPM_H

#include "arch.h"
#include "libipm_facilities.h"

struct trans;

/**
 * Fixed-size block descriptor for sending blocks of data using libipm
 *
 * Use with the type code 'B'
 *
 * See the descriptions of libipm_msg_out_append() and libipm_msg_in_parse()
 * for instructions on how to use this type.
 */
struct libipm_fsb
{
    void *data;
    unsigned int datalen;
};

/**
 * Status code for library calls
 *
 * These are mainly intended so that the test suite can check the expected
 * codepath has been followed.
 *
 * Wrappers around libipm may assume success is 0, and anything else
 * is an error.
 */
enum libipm_status
{
    E_LI_SUCCESS = 0,
    E_LI_PROGRAM_ERROR, /***< Programming error */
    E_LI_NO_MEMORY, /***< Memory allocation failure */
    E_LI_UNSUPPORTED_TYPE, /***< Specified type code is unsupported */
    E_LI_UNIMPLEMENTED_TYPE, /***< Specified type code is unimplemented */
    E_LI_UNEXPECTED_TYPE, /***< Encountered unexpected type on input */
    E_LI_BUFFER_OVERFLOW, /***< End of buffer reached unexpectedly */
    E_LI_TOO_MANY_FDS, /***< Too many file descriptors encountered */
    E_LI_BAD_VALUE, /***< Specified (or incoming) value is out of range */
    E_LI_BAD_HEADER, /***< Bad incoming message header */
    E_LI_TRANSPORT_ERROR /***< Error detected at the transport level */
};

/* Flags values for libipm_[set/clear]_flags() */

/**
 * Input buffer contains sensitive information and must be
 * erased by libipm_msg_in_reset(). The flag is one-shot, and
 * is cleared after use.
 *
 * There is no corresponding flag for the output buffer -
 * use libipm_msg_out_erase() explicitly
 */
#define LIBIPM_E_MSG_IN_ERASE_AFTER_USE (1u<<0)

/**
 * Prepare a standard 'struct trans' for use with libipm
 *
 * @param trans libipm transport
 * @param facility Facility number from libipm_facilities.h
 * @param msgno_to_str Function to turn a message number into a string
 * @return != 0 for error.
 *
 * The msgno_to_str() function is used for error logging. If the routine
 * is not specified, or the routine returns NULL, a numeric code is
 * logged instead.
 */
enum libipm_status
libipm_init_trans(struct trans *trans,
                  enum libipm_facility facility,
                  const char *(*msgno_to_str)(unsigned short msgno));

/**
 * Set the flag(s) specified for the transport
 * @param trans libipm transport
 * @param flags flags to set. Multiple flags should be or'd together.
 */
void
libipm_set_flags(struct trans *trans, unsigned int flags);


/**
 * Clear the flag(s) specified for the transport
 * @param trans libipm transport
 * @param flags flags to clear. Multiple flags should be or'd together.
 */
void
libipm_clear_flags(struct trans *trans, unsigned int flags);


/**
 * Change the facility number for the transport
 * @param trans libipm transport
 * @param old_facility old transport facility
 * @param new_facility new transport facility
 *
 * This call is required if a libipm transport changes a facility number.
 * This can be used to implement switches from one functional server state to
 * another.
 *
 * The caller must be aware of the previous facility to change the facility.
 * In the event of a mismatch, a message is logged and no action is taken.
 */
void
libipm_change_facility(struct trans *trans,
                       enum libipm_facility old_facility,
                       enum libipm_facility new_facility);


/**
 * Initialise an output message
 *
 * @param trans libipm transport
 * @param msgno message number
 * @param format a description of any arguments to add to the buffer, or
 *               NULL if no arguments are to be added at this time.
 *
 * See libipm_msg_out_append() for details of the format string. The format
 * string is followed immediately by the arguments it describes */
enum libipm_status
libipm_msg_out_init(struct trans *trans, unsigned short msgno,
                    const char *format, ...);

/**
 * Append more arguments to an output message
 *
 * @param trans libipm transport
 * @param format a description of any arguments to add to the buffer.
 * @param != 0 if an error occurs
 *
 * The format string is followed immediately by the arguments it
 * describes. The format string may contain these characters (from the
 * D-Bus type system):-
 *
 * Char | C Type  | Description
 * ---- |------   | -----------
 *   y  |uint8_t  | Unsigned 8-bit integer
 *   b  |int      | Boolean value (see below)
 *   n  |int16_t  | Signed (two's complement) 16-bit integer
 *   q  |uint16_t | Unsigned 16-bit integer
 *   i  |int32_t  | Signed (two's complement) 32-bit integer
 *   u  |uint32_t | Unsigned 32-bit integer
 *   x  |int64_t  | Signed (two's complement) 64-bit integer
 *   t  |uint64_t | Unsigned 64-bit integer
 *   s  |char *   | NULL-terminated string
 *   h  |int      | File descriptor
 *   d  |  -      | (reserved)
 *   o  |  -      | (reserved)
 *   g  |  -      | (reserved)
 *
 * For the 'b' type, only values 0 and 1 are allowed. Other values
 * generate an error.
 *
 * The 'h' type can only be used where the underlying transport is a
 * UNIX domain socket.
 *
 *  The following additions to the D-Bus type system are also supported:-
 *
 * Char |C Type                    | Description
 * -----|------                    | -----------
 *   B  |const struct libipm_fsb * | Fixed-size block pointer
 *
 * For the 'B' type, pass in the address of a descriptor containing the
 * address and size of the object. The object is copied to the output
 * stream, along with its length.
 */
enum libipm_status
libipm_msg_out_append(struct trans *trans, const char *format, ...);

/**
 * Marks a message as complete and ready for transmission
 *
 * @param trans libipm transport
 */
void
libipm_msg_out_mark_end(struct trans *trans);

/**
 * Convenience sending function
 *
 * Constructs a simple message and sends it immediately using
 * libipm_msg_out_init() / libipm_msg_out_mark_end() and
 * trans_force_write()
 *
 * @param trans libipm transport
 * @param msgno message number
 * @param format a description of any arguments to add to the buffer, or
 *               NULL if no arguments are to be added at this time.
 *
 * See libipm_msg_out_append() for details of the format string. The format
 * string is followed immediately by the arguments it describes
 */
enum libipm_status
libipm_msg_out_simple_send(struct trans *trans, unsigned short msgno,
                           const char *format, ...);

/**
 * Erase (rather than just ignore) the contents of an output buffer
 *
 * Use after sending a message containing data which should not be
 * kept in memory (e.g. passwords)`
 * @param trans libipm transport
 */
void
libipm_msg_out_erase(struct trans *trans);

/**
 * Checks a transport to see if a complete libipm message is
 * available for parsing
 *
 * @param trans libipm transport
 * @param[out] available != 0 if a complete message is available
 * @return != 0 for error
 *
 * When 'available' becomes set, the buffer is guaranteed to
 * be in a parseable state.
 *
 * The results of calling this function after starting to parse a message
 * and before calling libipm_msg_in_reset() are undefined.
 */
enum libipm_status
libipm_msg_in_check_available(struct trans *trans, int *available);

/**
 * Waits on a single transport for a libipm message to be available for
 * parsing
 *
 * @param trans libipm transport
 * @return != 0 for error
 *
 * While the call is active, data-in callbacks for the transport are
 * disabled.
 *
 * The results of calling this function after starting to parse a message
 * and before calling libipm_msg_in_reset() are undefined.
 *
 * Only use this call if you have nothing to do until a message
 * arrives on the transport. If you have other transports to service, use
 * libipm_msg_in_check_available()
 */
enum libipm_status
libipm_msg_in_wait_available(struct trans *trans);

/**
 * Get the message number for a message in the input buffer.
 *
 * @param trans libipm transport
 * @return message number in the buffer
 *
 * The results of calling this routine after a call to
 * libipm_msg_in_reset() and before a successful call to
 * libipm_msg_in_check_available() (or libipm_msg_wait_available())
 * are undefined.
 */
unsigned short
libipm_msg_in_get_msgno(const struct trans *trans);

/**
 * Returns a letter corresponding to the next available type in the
 * input stream.
 *
 * @param trans libipm transport
 * @return letter of the type, '\0' for end of message, or '?' for
 *          an unrecognised type
 */
char
libipm_msg_in_peek_type(struct trans *trans);

/**
 * Reads arguments from an input message
 *
 * @param trans libipm transport
 * @param format a description of the arguments to read from the buffer.
 * @param != 0 if an error occurs
 *
 * The format string is followed immediately by the arguments it
 * describes. The format string may contain these characters (from the
 * D-Bus type system):-
 *
 * Char | C Type     | Description
 * ---- | ------     | -----------
 *   y  | uint8_t *  | Unsigned 8-bit integer
 *   b  | int *      | Boolean value (0 or 1 only)
 *   n  | int16_t *  | Signed (two's complement) 16-bit integer
 *   q  | uint16_t * | Unsigned 16-bit integer
 *   i  | int32_t *  | Signed (two's complement) 32-bit integer
 *   u  | uint32_t * | Unsigned 32-bit integer
 *   x  | int64_t *  | Signed (two's complement) 64-bit integer
 *   t  | uint64_t * | Unsigned 64-bit integer
 *   s  | char **    | NULL-terminated string
 *   h  | int *      | File descriptor
 *   d  |   -        | (reserved)
 *   o  |   -        | (reserved)
 *   g  |   -        | (reserved)
 *
 *  The following additions to the D-Bus type system are also supported:-
 *
 * Char | C Type                    | Description
 * -----| ------                    | -----------
 *   B  | const struct libipm_fsb * | Fixed-size block pointer
 *
 * For the 's' type, a pointer into the string in the input buffer is
 * returned. This pointer will only be valid until the next call to
 * libipm_msg_in_reset()
 *
 * The 'h' type can only be used where the underlying transport is a
 * UNIX domain socket.
 *
 * For the 'B' type, pass in the address of an initialised descriptor
 * containing the address and size of the object to copy the data
 * to. The size in the descriptor must match the size of the object
 * on-the-wire. Unlike 's', the data is copied out of the input buffer.
 *
 */
enum libipm_status
libipm_msg_in_parse(struct trans *trans, const char *format, ...);

/**
 * Resets a message buffer ready to receive the next message
 *
 * If the LIBIPM_E_MSG_IN_ERASE_AFTER_USE flag is set for the transport,
 * the entire buffer is erased, and the flag is cleared
 *
 * Any file descriptors received from the other end but not parsed
 * by the application are closed.
 *
 * @param trans libipm transport
 */
void
libipm_msg_in_reset(struct trans *trans);


#endif /* LIBIPM_H */
