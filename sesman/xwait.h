#ifndef XWAIT_H
#define XWAIT_H
/**
 *
 * @brief waits for X to start
 * @param display number
 * @return 0 on error, 1 if X has outputs
 *
 */
int
wait_for_xserver(int display);
#endif
