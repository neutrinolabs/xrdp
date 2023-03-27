#ifndef XWAIT_H
#define XWAIT_H

#include <sys/types.h>

enum xwait_status
{
    XW_STATUS_OK = 0,
    XW_STATUS_MISC_ERROR,
    XW_STATUS_TIMED_OUT,
    XW_STATUS_FAILED_TO_START
};

/**
 *
 * @brief waits for X to start
 * @param uid User to run program under
 * @param env_names Environment to set for user (names)
 * @param env_values Environment to set for user (values)
 * @param display number
 * @return status
 *
 */
enum xwait_status
wait_for_xserver(uid_t uid,
                 struct list *env_names,
                 struct list *env_values,
                 int display);
#endif
