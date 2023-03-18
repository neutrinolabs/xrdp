#ifndef XWAIT_H
#define XWAIT_H

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
 * @param display number
 * @return status
 *
 */
enum xwait_status
wait_for_xserver(int display);
#endif
