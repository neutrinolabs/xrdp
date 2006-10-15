
#ifndef LIBSCP_V1S_H
#define LIBSCP_V1S_H

#include "libscp_types.h"
//#include "os_calls.h"
//#include "tcp.h"

/* server API */
/* ... */ enum SCP_SERVER_STATES_E scp_v1s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk);          
/* 002 */ enum SCP_SERVER_STATES_E scp_v1s_deny_connection(struct SCP_CONNECTION* c, char* reason);

/* 020 */ enum SCP_SERVER_STATES_E scp_v1s_request_pwd_change(struct SCP_CONNECTION* c, char* reason, char* npw);
/* 023 */ enum SCP_SERVER_STATES_E scp_v1s_pwd_change_error(struct SCP_CONNECTION* s, char* error, int retry, char* npw);
/* 030 */ enum SCP_SERVER_STATES_E scp_v1s_connect_new_session(struct SCP_CONNECTION* s, SCP_DISPLAY d);
/* 031 */ enum SCP_SERVER_STATES_E scp_v1s_reconnect_session(struct SCP_CONNECTION* s, SCP_DISPLAY d);
/* 032 */ enum SCP_SERVER_STATES_E scp_v1s_connection_error(struct SCP_CONNECTION* s, char* error);
/* 040 */ enum SCP_SERVER_STATES_E scp_v1s_list_sessions(struct SCP_CONNECTION* s, int sescnt, struct SCP_DISCONNECTED_SESSION** ds, SCP_SID* sid);

#endif
