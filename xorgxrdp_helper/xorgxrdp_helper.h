
#ifndef _XORGXRDP_HELPER_H
#define _XORGXRDP_HELPER_H

#define XH_YUV420   1
#define XH_YUV444   2

#define XH_BT601    0
#define XH_BT709FR  1
#define XH_BTRFX    2

struct xh_rect
{
    short x;
    short y;
    short w;
    short h;
};

/* this logging can be replaced with new xrdp logging macros after merge */

#if !defined(LOGLN)

#define LOGS "[%s][%d][%s]:"
#define LOGP __FILE__, __LINE__, __FUNCTION__

#if !defined(__FUNCTION__) && defined(__FUNC__)
#define LOG_PRE const char* __FUNCTION__ = __FUNC__; (void)__FUNCTION__;
#else
#define LOG_PRE
#endif

#if !defined(LOG_LEVEL)
#define LOG_LEVEL 1
#endif
#if LOG_LEVEL > 0
#define LOGLN(_args) do { LOG_PRE log_message _args ; } while (0)
#else
#define LOGLN(_args)
#endif
#if LOG_LEVEL > 10
#define LOGLND(_args) do { LOG_PRE log_message _args ; } while (0)
#else
#define LOGLND(_args)
#endif

#endif

#endif
