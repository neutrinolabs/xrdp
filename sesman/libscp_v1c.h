#include "scp.h"

#ifndef SCP_V1C_H
#define SCP_V1C_H

enum SCP_CLIENY_STATES_E
{
  SCP_CLIENT_STATE_NO,
  SCP_CLIENT_STATE_WRONGPWD,
  SCP_CLIENT_STATE_PWDCHG_REQ,
  SCP_CLIENT_STATE_PWDCHG_CANCEL,
  SCP_CLIENT_STATE_,

};

/* client API */
/**
 *
 *
 */
/* 001 */ SCP_CLIENT_STATES_E scp_v1c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s);
	ritorna errore: o il display

/* 021 */ SCP_CLIENT_STATES_E scp_v1c_pwd_change(struct SCP_CONNECTION* c, char* newpass);
/* 022 */ SCP_CLIENT_STATES_E scp_v1c_pwd_change_cancel(struct SCP_CONNECTION* c);

/* ... */ SCP_CLIENT_STATES_E scp_v1c_get_session_list(struct SCP_CONNECTION* c, int* scount, struct SCP_DISCONNECTED_SESSION** s);
/* 041 */ SCP_CLIENT_STATES_E scp_v1c_select_session(struct SCP_CONNECTION* c, SCP_SID sid);
/* 042 */ SCP_CLIENT_STATES_E scp_v1c_select_session_cancel(struct SCP_CONNECTION* c);

#endif
