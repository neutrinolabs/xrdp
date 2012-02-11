
#include "os_calls.h"

int        g_loc_io_count = 0;  // bytes read from local port
int        g_rem_io_count = 0;  // bytes read from remote port

static int g_terminated = 0;
static char g_buf[1024 * 32];

/*****************************************************************************/
static int
main_loop(char* local_port, char* remote_ip, char* remote_port, int hexdump)
{
    int lis_sck;
    int acc_sck;
    int con_sck;
    int sel;
    int count;
    int sent;
    int error;
    int i;
    int acc_to_con;
    int con_to_acc;

    acc_to_con = 0;
    con_to_acc = 0;
    acc_sck = 0;

    /* create the listening socket and setup options */
    lis_sck = g_tcp_socket();
    g_tcp_set_non_blocking(lis_sck);
    error = g_tcp_bind(lis_sck, local_port);
    if (error != 0)
    {
        g_writeln("bind failed");
    }

    /* listen for an incomming connection */
    if (error == 0)
    {
        error = g_tcp_listen(lis_sck);
        if (error == 0)
        {
            g_writeln("listening for connection");
        }
    }

    /* accept an incomming connection */
    if (error == 0)
    {
        while ((!g_terminated) && (error == 0))
        {
            acc_sck = g_tcp_accept(lis_sck);
            if ((acc_sck == -1) && g_tcp_last_error_would_block(lis_sck))
            {
                g_sleep(100);
            }
            else if (acc_sck == -1)
            {
                error = 1;
            }
            else
            {
                break;
            }
        }
        if (error == 0)
        {
            error = g_terminated;
        }

        /* stop listening */
        g_tcp_close(lis_sck);
        lis_sck = 0;
        if (error == 0)
        {
            g_writeln("got connection");
        }
    }

    /* connect outgoing socket */
    con_sck = 0;
    if (error == 0)
    {
        con_sck = g_tcp_socket();
        g_tcp_set_non_blocking(con_sck);
        error = g_tcp_connect(con_sck, remote_ip, remote_port);
        if ((error == -1) && g_tcp_last_error_would_block(con_sck))
        {
            error = 0;
            i = 0;
            while ((!g_tcp_can_send(con_sck, 100)) && (!g_terminated) && (i < 100))
            {
                g_sleep(100);
                i++;
            }
            if (i > 99)
            {
                g_writeln("timout connecting");
                error = 1;
            }
            if (g_terminated)
            {
                error = 1;
            }
        }
        if ((error != 0) && (!g_terminated))
        {
            g_writeln("error connecting to remote\r\n");
        }
    }
    while ((!g_terminated) && (error == 0))
    {
        sel = g_tcp_select(con_sck, acc_sck);
        if (sel == 0)
        {
            g_sleep(10);
            continue;
        }
        if (sel & 1)
        {
            // can read from con_sck w/o blocking
            count = g_tcp_recv(con_sck, g_buf, 1024 * 16, 0);
            error = count < 1;
            if (error == 0)
            {
                g_loc_io_count += count;
                con_to_acc += count;
                if (hexdump)
                {
                    g_writeln("from remove, the socket from connect");
                    g_hexdump(g_buf, count);
                }
#if 0
                g_writeln("local_io_count: %d\tremote_io_count: %d",
                        g_loc_io_count, g_rem_io_count);
#endif
                sent = 0;
                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(acc_sck, g_buf + sent, count - sent, 0);
                    if ((i == -1) && g_tcp_last_error_would_block(acc_sck))
                    {
                        g_tcp_can_send(acc_sck, 1000);
                    }
                    else if (i < 1)
                    {
                        error = 1;
                    }
                    else
                    {
                        sent += i;
                    }
                }
            }
        }
        if (sel & 2)
        {
            // can read from acc_sck w/o blocking
            count = g_tcp_recv(acc_sck, g_buf, 1024 * 16, 0);
            error = count < 1;
            if (error == 0)
            {
                g_rem_io_count += count;
                acc_to_con += count;
                if (hexdump)
                {
                    g_writeln("from accepted, the socket from accept");
                    g_hexdump(g_buf, count);
                }
#if 0
                g_writeln("local_io_count: %d\tremote_io_count: %d",
                        g_loc_io_count, g_rem_io_count);
#endif
                sent = 0;
                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(con_sck, g_buf + sent, count - sent, 0);
                    if ((i == -1) && g_tcp_last_error_would_block(con_sck))
                    {
                        g_tcp_can_send(con_sck, 1000);
                    }
                    else if (i < 1)
                    {
                        error = 1;
                    }
                    else
                    {
                        sent += i;
                    }
                }
            }
        }
    }
    g_tcp_close(lis_sck);
    g_tcp_close(con_sck);
    g_tcp_close(acc_sck);
    g_writeln("acc_to_con %d", acc_to_con);
    g_writeln("con_to_acc %d", con_to_acc);
    return 0;
}


/*****************************************************************************/
static int
usage(void)
{
    g_writeln("tcp_proxy <local-port> <remote-ip> <remote-port> [dump]");
    return 0;
}


/*****************************************************************************/
void
proxy_shutdown(int sig)
{
    g_writeln("shutting down");
    g_terminated = 1;
}

void
clear_counters(int sig)
{
    g_writeln("cleared counters at: local_io_count: %d remote_io_count: %d", 
              g_loc_io_count, g_rem_io_count);
    g_loc_io_count = 0;
    g_rem_io_count = 0;
}

/*****************************************************************************/
int
main(int argc, char** argv)
{
    int dump;

    if (argc < 4)
    {
        usage();
        return 0;
    }
    g_init();
    g_signal(2, proxy_shutdown); /* SIGINT */
    g_signal(9, proxy_shutdown); /* SIGKILL */
    g_signal(10, clear_counters);/* SIGUSR1*/
    g_signal(15, proxy_shutdown);/* SIGTERM */
    if (argc < 5)
    {
        while (!g_terminated)
        {
            g_loc_io_count = 0;
            g_rem_io_count = 0;
            main_loop(argv[1], argv[2], argv[3], 0);
        }
    }
    else
    {
        dump = g_strcasecmp(argv[4], "dump") == 0;
        while (!g_terminated)
        {
            main_loop(argv[1], argv[2], argv[3], dump);
        }
    }
    g_deinit();
    return 0;
}
