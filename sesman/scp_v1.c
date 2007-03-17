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
   Copyright (C) Jay Sorg 2005-2007
*/

/**
 *
 * @file scp_v1.c
 * @brief scp version 1 implementation
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

//#include "libscp_types.h"
#include "libscp.h"

extern struct config_sesman g_cfg;

static void parseCommonStates(enum SCP_SERVER_STATES_E e, char* f);

/******************************************************************************/
void DEFAULT_CC
scp_v1_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  long data;
  int display;
  int retries;
  int current_try;
  enum SCP_SERVER_STATES_E e;
  struct SCP_DISCONNECTED_SESSION* slist;
  struct session_item* sitem;
  int scount;
  SCP_SID sid;

  retries = g_cfg.sec.login_retry;
  current_try = retries;

  data = auth_userpass(s->username, s->password);
  /*LOG_DBG("user: %s\npass: %s", s->username, s->password);*/

  while ((!data) && ((retries == 0) || (current_try > 0)))
  {
    LOG_DBG("data %d - retry %d - currenttry %d - expr %d", data, retries, current_try, ((!data) && ((retries==0) || (current_try>0))));

    e=scp_v1s_request_password(c,s,"Wrong username and/or password");

    switch (e)
    {
      case SCP_SERVER_STATE_OK:
        /* all ok, we got new username and password */
        data = auth_userpass(s->username, s->password);
        /* one try less */
        if (current_try > 0)
        {
          current_try--;
        }
        break;
      default:
        /* we check the other errors */
        parseCommonStates(e, "scp_v1s_list_sessions()");
        free_session(s);
        return;
        //break;
    }
  }

  if (!data)
  {
    scp_v1s_deny_connection(c, "Login failed");
    log_message(LOG_LEVEL_INFO,
      "Login failed for user %s. Connection terminated", s->username);
    free_session(s);
    return;
  }

  /* testing if login is allowed*/
  if (0 == access_login_allowed(s->username))
  {
    scp_v1s_deny_connection(c, "Access to Terminal Server not allowed.");
    log_message(LOG_LEVEL_INFO,
      "User %s not allowed on TS. Connection terminated", s->username);
    free_session(s);
    return;
  }

  //check if we need password change

  /* list disconnected sessions */
  slist = session_get_byuser(s->username, &scount);

  if (scount == 0)
  {
    /* no disconnected sessions - start a new one */
    log_message(LOG_LEVEL_INFO, "granted TS access to user %s", s->username);
    if (SCP_SESSION_TYPE_XVNC == s->type)
    {
      log_message(LOG_LEVEL_INFO, "starting Xvnc session...");
      display = session_start(s->width, s->height, s->bpp, s->username,
                              s->password, data, SESMAN_SESSION_TYPE_XVNC);
    }
    else
    {
      log_message(LOG_LEVEL_INFO, "starting Xrdp session...");
      display = session_start(s->width, s->height, s->bpp, s->username,
                              s->password, data, SESMAN_SESSION_TYPE_XRDP);
    }

    e = scp_v1s_connect_new_session(c, display);
    switch (e)
    {
      case SCP_SERVER_STATE_OK:
        /* all ok, we got new username and password */
        break;
      default:
        /* we check the other errors */
        parseCommonStates(e, "scp_v1s_connect_new_session()");
        break;
    }
  }
  else
  {
#warning FIXME session_get_by*() should return a malloc()ated struct
#warning FIXME or at least lock the chain
    /* one or more disconnected sessions - listing */
    e = scp_v1s_list_sessions(c, scount, slist, &sid);

    switch (e)
    {
      /*case SCP_SERVER_STATE_FORCE_NEW:*/
      /* we should check for MaxSessions */
      case SCP_SERVER_STATE_SELECTION_CANCEL:
        log_message(LOG_LEVEL_INFO, "Connection cancelled after session listing");
        break;
      case SCP_SERVER_STATE_OK:
        /* ok, reconnecting... */
        sitem=session_get_bypid(sid);
        if (0==sitem)
        {
          e=scp_v1s_connection_error(c, "Internal error");
          log_message(LOG_LEVEL_INFO, "Cannot find session item on the chain");
        }
        else
        {
          display=sitem->display;
          /*e=scp_v1s_reconnect_session(c, sitem, display);*/
          e=scp_v1s_reconnect_session(c, display);
          log_message(LOG_LEVEL_INFO, "User %s reconnected to session %d on port %d", \
                      s->username, sitem->pid, display);
        }
        break;
      default:
        /* we check the other errors */
        parseCommonStates(e, "scp_v1s_list_sessions()");
        break;
    }
    g_free(slist);
  }

  /* resource management */
  if ((e == SCP_SERVER_STATE_OK) && (s->rsr))
  {
    /* here goes scp resource sharing code */
  }

  /* cleanup */
  free_session(s);
  auth_end(data);
}

static void parseCommonStates(enum SCP_SERVER_STATES_E e, char* f)
{
  switch (e)
  {
    case SCP_SERVER_STATE_VERSION_ERR:
      LOG_DBG("version error", 0)
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
