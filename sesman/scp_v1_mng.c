/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file scp_v1_mng.c
 * @brief scp version 1 implementation - management
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

#include "libscp.h"

extern struct config_sesman* g_cfg; /* in sesman.c */

static void parseCommonStates(enum SCP_SERVER_STATES_E e, char* f);

/******************************************************************************/
void DEFAULT_CC
scp_v1_mng_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  long data;
  enum SCP_SERVER_STATES_E e;
  struct SCP_DISCONNECTED_SESSION* slist = 0;
  int scount;
  int end = 0;

  data = auth_userpass(s->username, s->password);
  /*LOG_DBG("user: %s\npass: %s", s->username, s->password);*/

  if (!data)
  {
    scp_v1s_mng_deny_connection(c, "Login failed");
    log_message(LOG_LEVEL_INFO,
      "[MNG] Login failed for user %s. Connection terminated", s->username);
    scp_session_destroy(s);
    auth_end(data);
    return;
  }

  /* testing if login is allowed */
  if (0 == access_login_mng_allowed(s->username))
  {
    scp_v1s_mng_deny_connection(c, "Access to Terminal Server not allowed.");
    log_message(LOG_LEVEL_INFO,
      "[MNG] User %s not allowed on TS. Connection terminated", s->username);
    scp_session_destroy(s);
    auth_end(data);
    return;
  }

  e = scp_v1s_mng_allow_connection(c, s);

  end = 1;
  while (end)
  {
    switch (e)
    {
      case SCP_SERVER_STATE_MNG_ACTION:
        log_message(LOG_LEVEL_INFO, "Connection cancelled after session listing");
        break;

      case SCP_SERVER_STATE_MNG_LISTREQ:
        /* list disconnected sessions */
        slist = session_get_byuser(NULL, &scount, SESMAN_SESSION_STATUS_ALL);
        LOG_DBG("sessions on TS: %d (slist: %x)", scount, slist);

        if (0 == slist)
        {
//          e=scp_v1s_connection_error(c, "Internal error");
          log_message(LOG_LEVEL_INFO, "No sessions on Terminal Server");
          end = 0;
        }
        else
        {
          e = scp_v1s_mng_list_sessions(c, s, scount, slist);
          g_free(slist);
        }

        break;
      default:
        /* we check the other errors */
        parseCommonStates(e, "scp_v1s_mng_list_sessions()");
        end = 0;
        break;
    }
  }

  /* cleanup */
  scp_session_destroy(s);
  auth_end(data);
}

static void parseCommonStates(enum SCP_SERVER_STATES_E e, char* f)
{
  switch (e)
  {
    case SCP_SERVER_STATE_VERSION_ERR:
      LOG_DBG("version error")
    case SCP_SERVER_STATE_SIZE_ERR:
      /* an unknown scp version was requested, so we shut down the */
      /* connection (and log the fact)                             */
      log_message(LOG_LEVEL_WARNING,
        "protocol violation. connection closed.");
      break;
    case SCP_SERVER_STATE_NETWORK_ERR:
      log_message(LOG_LEVEL_WARNING, "libscp network error.");
      break;
    case SCP_SERVER_STATE_SEQUENCE_ERR:
      log_message(LOG_LEVEL_WARNING, "libscp sequence error.");
      break;
    case SCP_SERVER_STATE_INTERNAL_ERR:
      /* internal error occurred (eg. malloc() error, ecc.) */
      log_message(LOG_LEVEL_ERROR, "libscp internal error occurred.");
      break;
    default:
      /* dummy: scp_v1s_request_password won't generate any other */
      /* error other than the ones before                         */
      log_message(LOG_LEVEL_ALWAYS, "unknown return from %s", f);
      break;
  }
}
