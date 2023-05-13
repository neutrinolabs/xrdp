/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include <pthread.h>

#include "gtcp.h"

#define WINDOW_TITLE    "Tcp Proxy Version 1.0"
#define CONTEXT_ID      1
#define MSG_INFO        GTK_MESSAGE_INFO
#define MSG_WARN        GTK_MESSAGE_WARNING
#define MSG_ERROR       GTK_MESSAGE_ERROR
#define MAIN_THREAD_YES 0
#define MAIN_THREAD_NO  1

/* globals */
pthread_t g_tid;
int       g_keep_running   = 1;
int       g_loc_io_count   = 0;  /* bytes read from local port  */
int       g_rem_io_count   = 0;  /* bytes read from remote port */

GtkWidget *g_btn_start;
GtkWidget *g_tbx_loc_port;
GtkWidget *g_tbx_rem_ip;
GtkWidget *g_tbx_rem_port;
GtkWidget *g_tbx_loc_stats;
GtkWidget *g_tbx_rem_stats;
GtkWidget *g_statusbar;
GtkWidget *g_txtvu_loc_port;
GtkWidget *g_txtvu_rem_port;

/* forward declarations */
static void *tcp_proxy(void *arg);

static void show_msg(int not_main_thread, int style,
                     const gchar *title, const gchar *msg);

static void show_status(int not_main_thread, char *msg);
static void clear_status(int not_main_thread);
static void enable_btn_start(int main_thread);
static void disable_btn_start(int main_thread);

/* getters */
static char *get_local_port();
static char *get_remote_ip();
static char *get_remote_port();

/* setters */
static void show_loc_port_stats(int main_thread, int count);
static void show_rem_port_stats(int main_thread, int count);

/* handlers */
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *ev, gpointer data);
static void on_destroy(GtkWidget *widget, gpointer data);
static void on_start_clicked(GtkWidget *widget, gpointer data);
static void on_clear_clicked(GtkWidget *widget, gpointer data);
static void on_quit_clicked(GtkWidget *widget, gpointer data);

int main(int argc, char **argv)
{
    /* init threads */
    g_thread_init(NULL);
    gdk_threads_init();

    /* setup GTK */
    gtk_init(&argc, &argv);

    /* create labels and right justify them */
    GtkWidget *lbl_loc_port    = gtk_label_new("Local port");
    GtkWidget *lbl_rem_ip      = gtk_label_new("Remote IP");
    GtkWidget *lbl_rem_port    = gtk_label_new("Remote port");
    GtkWidget *lbl_loc_stats   = gtk_label_new("Local port recv stats");
    GtkWidget *lbl_rem_stats   = gtk_label_new("Remote port recv stats");

    gtk_misc_set_alignment(GTK_MISC(lbl_loc_port), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(lbl_rem_ip), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(lbl_rem_port), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(lbl_loc_stats), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(lbl_rem_stats), 1.0, 0.5);

    /* create text boxes */
    g_tbx_loc_port   = gtk_entry_new();
    g_tbx_rem_ip     = gtk_entry_new();
    g_tbx_rem_port   = gtk_entry_new();
    g_tbx_loc_stats  = gtk_entry_new();
    g_tbx_rem_stats  = gtk_entry_new();

    /* stat text boxes are read only */
    gtk_entry_set_editable(GTK_ENTRY(g_tbx_loc_stats), FALSE);
    gtk_entry_set_editable(GTK_ENTRY(g_tbx_rem_stats), FALSE);

    /* enable when debugging */
#if 0
    gtk_entry_set_text(GTK_ENTRY(g_tbx_loc_port), "1234");
    gtk_entry_set_text(GTK_ENTRY(g_tbx_rem_ip), "192.168.2.20");
    gtk_entry_set_text(GTK_ENTRY(g_tbx_rem_port), "43222");
#endif

    /* setup buttons inside a HBox */
    g_btn_start          = gtk_button_new_with_label("Start listening");
    GtkWidget *btn_clear = gtk_button_new_with_label("Clear receive stats");
    GtkWidget *btn_quit  = gtk_button_new_with_label("Quit application");

    GtkWidget *hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), g_btn_start);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), btn_clear);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), btn_quit);

#if 0
    g_txtvu_loc_port = gtk_text_view_new();
    g_txtvu_rem_port = gtk_text_view_new();
#endif

    /* create table */
    GtkWidget *table = gtk_table_new(8, 3, FALSE);

    int row = 0;

    /* insert labels into table */
    gtk_table_attach(GTK_TABLE(table), lbl_loc_port, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), lbl_rem_ip, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), lbl_rem_port, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), lbl_loc_stats, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), lbl_rem_stats, 0, 1, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    row = 0;

    /* insert text boxes into table */
    gtk_table_attach(GTK_TABLE(table), g_tbx_loc_port, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), g_tbx_rem_ip, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), g_tbx_rem_port, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), g_tbx_loc_stats, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table), g_tbx_rem_stats, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;
    row++;

    /* insert hbox with buttons into table */
    gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;

#if 0
    /* text view to display hexdumps */
    gtk_table_attach(GTK_TABLE(table), g_txtvu_loc_port, 0, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);
    row++;
#endif

    /* status bar to display messages */
    g_statusbar = gtk_statusbar_new();
    gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(g_statusbar), TRUE);
    gtk_table_attach(GTK_TABLE(table), g_statusbar, 0, 2, row, row + 1,
                     GTK_FILL, GTK_FILL, 5, 0);

    /* setup main window */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
    gtk_window_set_title(GTK_WINDOW(window), WINDOW_TITLE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    gtk_container_add(GTK_CONTAINER(window), table);
    gtk_widget_show_all(window);

    /* setup callbacks */
    g_signal_connect(window, "delete-event", G_CALLBACK(on_delete_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);

    g_signal_connect(g_btn_start, "clicked", G_CALLBACK(on_start_clicked), NULL);
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_quit_clicked), NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    return 0;
}

/**
 * Start listening on specified local socket; when we get a connection,
 * connect to specified remote server and transfer data between local
 * and remote server
 *****************************************************************************/

static void *tcp_proxy(void *arg)
{
    char buf[1024 * 32];

    int lis_skt = -1;
    int acc_skt = -1;
    int con_skt = -1;
    int sel     = 0;
    int rv      = 0;
    int i       = 0;
    int count   = 0;
    int sent    = 0;

    /* create listener socket */
    if ((lis_skt = tcp_socket_create()) < 0)
    {
        show_msg(MAIN_THREAD_NO, MSG_ERROR, "Creating socket",
                 "\nOperation failed. System out of resources");
        enable_btn_start(MAIN_THREAD_NO);
        return NULL;
    }

    /* place it in non blocking mode */
    tcp_set_non_blocking(lis_skt);

    if ((rv = tcp_bind(lis_skt, get_local_port())) != 0)
    {
        show_msg(MAIN_THREAD_NO, MSG_ERROR, "Bind error",
                 "\nUnable to bind socket, cannot continue");
        tcp_close(lis_skt);
        enable_btn_start(MAIN_THREAD_NO);
        return NULL;
    }

    /* listen for incoming connection */
    if (tcp_listen(lis_skt))
    {
        show_msg(MAIN_THREAD_NO, MSG_ERROR, "Listen error",
                 "\nUnable to listen on socket, cannot continue");
        tcp_close(lis_skt);
        enable_btn_start(MAIN_THREAD_NO);
        return NULL;
    }

    show_status(MAIN_THREAD_NO, "Waiting for client connections");

    /* accept incoming connection */
    while (g_keep_running)
    {
        acc_skt = tcp_accept(lis_skt);
        if (acc_skt > 0)
        {
            /* client connected */
            show_status(MAIN_THREAD_NO, "Client connected");
            tcp_close(lis_skt);
            lis_skt = -1;
            break;
        }
        if ((acc_skt < 0) && (tcp_last_error_would_block()))
        {
            /* no connection, try again */
            usleep(250);
            continue;
        }
        else
        {
            tcp_close(lis_skt);
            lis_skt = -1;
            enable_btn_start(MAIN_THREAD_NO);
            return NULL;
        }
    }

    /* we have a client connection, try connecting to server */
    if ((con_skt = tcp_socket()) < 0)
    {
        show_msg(MAIN_THREAD_NO, MSG_ERROR, "Creating socket",
                 "\nOperation failed. System out of resources");
        tcp_close(lis_skt);
        tcp_close(acc_skt);
        enable_btn_start(MAIN_THREAD_NO);
        return NULL;
    }

    /* place it in non blocking mode */
    tcp_set_non_blocking(con_skt);

    rv = tcp_connect(con_skt, get_remote_ip(), get_remote_port());
#if 0
    if (rv < 0)
    {
        show_status(MAIN_THREAD_NO, "Could not connect to server");
        tcp_close(lis_skt);
        tcp_close(acc_skt);
        enable_btn_start(MAIN_THREAD_NO);
        return NULL;
    }
#endif

    if ((rv < 0) && (tcp_last_error_would_block(con_skt)))
    {
        for (i = 0; i < 100; i++)
        {
            if (tcp_can_send(con_skt, 100))
            {
                break;
            }

            usleep(100);
        }

        if (i == 100)
        {
            show_status(MAIN_THREAD_NO, "Could not connect to server");
            tcp_close(lis_skt);
            tcp_close(acc_skt);
            enable_btn_start(MAIN_THREAD_NO);
            return NULL;
        }
    }

    show_status(MAIN_THREAD_NO, "Connected to server");
    rv = 0;

    while (g_keep_running && rv == 0)
    {
        if ((sel = tcp_select(con_skt, acc_skt)) == 0)
        {
            usleep(10);
            continue;
        }

        if (sel & 1)
        {
            /* can read from con_skt without blocking */
            count = tcp_recv(con_skt, buf, 1024 * 16, 0);
            rv = count < 1;

            if (rv == 0)
            {
                g_loc_io_count += count;
                show_loc_port_stats(MAIN_THREAD_NO, g_loc_io_count);

                /* TODO: hexdump data here */

                sent = 0;

                while ((sent < count) && (rv == 0) && (g_keep_running))
                {
                    i = tcp_send(acc_skt, buf + sent, count - sent, 0);

                    if ((i == -1) && tcp_last_error_would_block(acc_skt))
                    {
                        tcp_can_send(acc_skt, 1000);
                    }
                    else if (i < 1)
                    {
                        rv = 1;
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
            /* can read from acc_skt without blocking */
            count = tcp_recv(acc_skt, buf, 1024 * 16, 0);
            rv = count < 1;

            if (rv == 0)
            {
                g_rem_io_count += count;
                show_rem_port_stats(MAIN_THREAD_NO, g_rem_io_count);

                /* TODO: hexdump data here */

                sent = 0;

                while ((sent < count) && (rv == 0) && (g_keep_running))
                {
                    i = tcp_send(con_skt, buf + sent, count - sent, 0);

                    if ((i == -1) && tcp_last_error_would_block(con_skt))
                    {
                        tcp_can_send(con_skt, 1000);
                    }
                    else if (i < 1)
                    {
                        rv = 1;
                    }
                    else
                    {
                        sent += i;
                    }
                }
            }
        }
    }

    tcp_close(lis_skt);
    tcp_close(con_skt);
    tcp_close(acc_skt);

    show_status(MAIN_THREAD_NO, "Connection closed");
    enable_btn_start(MAIN_THREAD_NO);
    return NULL;
}

/**
 * Display a message using specified style dialog
 *
 * @param  style  information, warning or error
 * @param  title  text to be displayed in title bar
 * @param  msg    message to be displayed
 *****************************************************************************/

static void show_msg(int not_main_window, int style,
                     const gchar *title, const gchar *msg)
{
    GtkWidget *dialog;

    if (not_main_window)
    {
        gdk_threads_enter();
    }

    dialog = gtk_message_dialog_new(GTK_WINDOW(NULL),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    style,
                                    GTK_BUTTONS_OK,
                                    "%s", msg);

    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (not_main_window)
    {
        gdk_threads_leave();
    }
}

/**
 * Write message to status bar
 *****************************************************************************/

static void show_status(int not_main_thread, char *msg)
{
    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_statusbar_push(GTK_STATUSBAR(g_statusbar), CONTEXT_ID, msg);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * Clear status bar
 *****************************************************************************/

static void clear_status(int not_main_thread)
{
    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_statusbar_remove_all(GTK_STATUSBAR(g_statusbar), CONTEXT_ID);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * Enable 'Start Listening' button
 *****************************************************************************/

static void enable_btn_start(int not_main_thread)
{
    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_widget_set_sensitive(GTK_WIDGET(g_btn_start), TRUE);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * Disable 'Start Listening' button
 *****************************************************************************/

static void disable_btn_start(int not_main_thread)
{
    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_widget_set_sensitive(GTK_WIDGET(g_btn_start), FALSE);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * Return local port setting
 *****************************************************************************/

static char *get_local_port()
{
    const char *cptr = gtk_entry_get_text(GTK_ENTRY(g_tbx_loc_port));
    return (char *) cptr;
}

/**
 * Return remote IP setting
 *****************************************************************************/

static char *get_remote_ip()
{
    const char *cptr = gtk_entry_get_text(GTK_ENTRY(g_tbx_rem_ip));
    return (char *) cptr;
}

/**
 * Return remote port setting
 *****************************************************************************/

static char *get_remote_port()
{
    const char *cptr = gtk_entry_get_text(GTK_ENTRY(g_tbx_rem_port));
    return (char *) cptr;
}

/**
 * Update local port stat counter
 *****************************************************************************/

static void show_loc_port_stats(int not_main_thread, int count)
{
    char buf[128];

    sprintf(buf, "%d", count);

    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_entry_set_text(GTK_ENTRY(g_tbx_loc_stats), buf);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * Update remote port stat counter
 *****************************************************************************/

static void show_rem_port_stats(int not_main_thread, int count)
{
    char buf[128];

    sprintf(buf, "%d", count);

    if (not_main_thread)
    {
        gdk_threads_enter();
    }

    gtk_entry_set_text(GTK_ENTRY(g_tbx_rem_stats), buf);

    if (not_main_thread)
    {
        gdk_threads_leave();
    }
}

/**
 * User clicked on window close button
 *****************************************************************************/

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *ev, gpointer data)
{
    return FALSE;
}

/**
 * Close application
 *****************************************************************************/

static void on_destroy(GtkWidget *widget, gpointer data)
{
    /* this will destroy all windows and return control to gtk_main() */
    gtk_main_quit();
}

/**
 * Start a thread that listens for incoming connections
 *****************************************************************************/

static void on_start_clicked(GtkWidget *widget, gpointer data)
{
    /* local port must be specified */
    if (gtk_entry_get_text_length(GTK_ENTRY(g_tbx_loc_port)) == 0)
    {
        show_msg(MAIN_THREAD_YES, MSG_ERROR, "Invalid entry",
                 "\nYou must enter a value for Local Port");
        return;
    }

    /* remote IP must be specified */
    if (gtk_entry_get_text_length(GTK_ENTRY(g_tbx_rem_ip)) == 0)
    {
        show_msg(MAIN_THREAD_YES, MSG_ERROR, "Invalid entry",
                 "\nYou must enter a value for Remote IP");
        return;
    }

    /* remote port must be specified */
    if (gtk_entry_get_text_length(GTK_ENTRY(g_tbx_rem_port)) == 0)
    {
        show_msg(MAIN_THREAD_YES, MSG_ERROR, "Invalid entry",
                 "\nYou must enter a value for Remote Port");
        return;
    }

    if (pthread_create(&g_tid, NULL, tcp_proxy, NULL))
    {
        show_msg(MAIN_THREAD_YES, MSG_ERROR, "Starting listener",
                 "\nThread create error. System out of resources");
        return;
    }

    disable_btn_start(MAIN_THREAD_YES);
}

/**
 * Clear stat counters
 *****************************************************************************/

static void on_clear_clicked(GtkWidget *widget, gpointer data)
{
    g_loc_io_count = 0;
    g_rem_io_count = 0;
    show_loc_port_stats(MAIN_THREAD_YES, g_loc_io_count);
    show_rem_port_stats(MAIN_THREAD_YES, g_rem_io_count);
}

/**
 * Quit application
 *****************************************************************************/

static void on_quit_clicked(GtkWidget *widget, gpointer data)
{
    /* this will destroy all windows and return control to gtk_main() */
    gtk_main_quit();
}
