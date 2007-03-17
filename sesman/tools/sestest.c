

#include "arch.h"
#include "tcp.h"
#include "libscp.h"
#include "parse.h"

#include <stdio.h>

int inputSession(struct SCP_SESSION* s);
unsigned int menuSelect(unsigned int choices);

int main(int argc, char** argv)
{
  struct SCP_SESSION s;
  struct SCP_CONNECTION c;
  /*struct SCP_DISCONNECTED_SESSION ds;*/
  struct SCP_DISCONNECTED_SESSION* dsl;
  enum SCP_CLIENT_STATES_E e;
  int end;
  int scnt;
  int idx;
  int sel;

  make_stream(c.in_s);
  init_stream(c.in_s, 8192);
  make_stream(c.out_s);
  init_stream(c.out_s, 8192);
  c.in_sck = g_tcp_socket();

  if (0!=g_tcp_connect(c.in_sck, "localhost", "3350"))
  {
    g_printf("error connecting");
    return 1;
  }

  g_printf("001 - send connect request\n");

/*struct SCP_SESSION
{
  uint16_t display;
  char* errstr;
};*/

  s.type=SCP_SESSION_TYPE_XVNC;
  s.version=1;
  s.height=600;
  s.width=800;
  s.bpp=8;
  s.rsr=0;
  g_strncpy(s.locale,"it_IT  0123456789",18);

  s.username=g_malloc(256, 1);
  g_strncpy(s.username,"prog",255);

  s.password=g_malloc(256,1);
  g_strncpy(s.password, "prog", 255);
  g_printf("%s - %s\n", s.username, s.password);


  s.hostname=g_malloc(256,1);
  g_strncpy(s.hostname, "odin", 255);

  s.addr_type=SCP_ADDRESS_TYPE_IPV4;
  s.ipv4addr=0;
  s.errstr=0;

  end=0;
  e=scp_v1c_connect(&c,&s);

  while (!end)
  {
    switch (e)
    {
      case SCP_CLIENT_STATE_OK:
        g_printf("OK : display is %d\n", (short int)s.display);
        end=1;
        break;
      case SCP_CLIENT_STATE_SESSION_LIST:
        g_printf("OK : session list needed\n");
        e=scp_v1c_get_session_list(&c, &scnt, &dsl);
        printf("Sessions: %d\n", scnt);
        for (idx=0; idx <scnt; idx++)
        {
          printf("Session \t%d - %d - %dx%dx%d - %d %d %d\n", (dsl[idx]).SID, (dsl[idx]).type, (dsl[idx]).height, (dsl[idx]).width, (dsl[idx]).bpp, (dsl[idx]).idle_days, (dsl[idx]).idle_hours, (dsl[idx]).idle_minutes);
        }
        break;
      case SCP_CLIENT_STATE_LIST_OK:
        g_printf("OK : selecting a session:\n");
        sel = menuSelect(scnt);
        e=scp_v1c_select_session(&c, &s, dsl[sel-1].SID);
        g_printf("\n return: %d \n", e);
        break;
      case SCP_CLIENT_STATE_RESEND_CREDENTIALS:
        g_printf("ERR: resend credentials - %s\n", s.errstr);
        g_printf("     username:");
        scanf("%255s", s.username);
        g_printf("     password:");
        scanf("%255s", s.password);
        e=scp_v1c_resend_credentials(&c,&s);
        break;
      case SCP_CLIENT_STATE_CONNECTION_DENIED:
        g_printf("ERR: connection denied: %s\n", s.errstr);
        end=1;
        break;
      case SCP_CLIENT_STATE_PWD_CHANGE_REQ:
        g_printf("OK : password change required\n");
        break;
      /*case SCP_CLIENT_STATE_RECONNECT_SINGLE:
        g_printf("OK : reconnect to 1 disconnected session\n");
        e=scp_v1c_retrieve_session(&c, &s, &ds);
        g_printf("Session Type: %d on %d\n", ds.type, s.display);
        g_printf("Session Screen: %dx%dx%d\n", ds.height, ds.width, ds.bpp);*/
        break;
      default:
        g_printf("protocol error: %d\n", e);
        end=1;
    }
  }

  g_tcp_close(c.in_sck);
  free_stream(c.in_s);
  free_stream(c.out_s);

  return 0;
}

int inputSession(struct SCP_SESSION* s)
{
  unsigned int integer;

  g_printf("username: ");
  scanf("%255s", s->username);
  g_printf("password:");
  scanf("%255s", s->password);
  g_printf("hostname:");
  scanf("%255s", s->hostname);

  g_printf("session type:\n");
  g_printf("0: Xvnc\n", SCP_SESSION_TYPE_XVNC);
  g_printf("1: x11rdp\n", SCP_SESSION_TYPE_XRDP);
  integer=menuSelect(1);
  if (integer==1)
  {
    s->type=SCP_SESSION_TYPE_XRDP;
  }
  else
  {
    s->type=SCP_SESSION_TYPE_XVNC;
  }

  s->version=1;
  s->height=600;
  s->width=800;
  s->bpp=8;

  /* fixed for now */
  s->rsr=0;
  g_strncpy(s->locale,"it_IT  0123456789",18);

  return 0;
}

unsigned int menuSelect(unsigned int choices)
{
  unsigned int sel;
  int ret;

  ret=scanf("%u", &sel);

  while ((ret==0) || (sel > choices))
  {
    g_printf("invalid choice.");
    scanf("%u", &sel);
  }

  return sel;
}
