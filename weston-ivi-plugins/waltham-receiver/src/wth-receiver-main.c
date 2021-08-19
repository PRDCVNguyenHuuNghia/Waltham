/*
 * Copyright © 2019 Advanced Driver Information Technology GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*******************************************************************************
**                                                                            **
**  TARGET    : linux                                                         **
**                                                                            **
**  PROJECT   : waltham-receiver                                              **
**                                                                            **
**  PURPOSE   : This file handles connection with remote-client               **
**                                                                            **
*******************************************************************************/

#include <signal.h>
#include "wth-receiver-comm.h"

#define MAX_EPOLL_WATCHES 2

uint16_t tcp_port;

/** Print out the application help
 */
static void usage(void)
{
    printf("Usage: waltham receiver [options]\n");
    printf("Options:\n");
    printf("  -p --port number          TCP port number\n");
    printf("  -h --help                 Usage\n");
    printf("  -v --verbose              Set verbose flag (Default:%d)\n", get_verbosity());
}

static struct option long_options[] = {
    {"port",     required_argument,  0,  'p'},
    {"verbose",  no_argument,    0,  'v'},
    {"help",     no_argument,    0,  'h'},
    {0,          0,              0,   0}
};

/**
 * parse_args
 *
 * Parses the application arguments
 * The arguments are parsed and saved in static structure for future use.
 *
 * @param argc The amount of arguments
 * @param argv The table of arguments
 *
 * @return 0 on success, -1 otherwise
 */
static int parse_args(int argc, char *argv[])
{
    int c = -1;
    int long_index = 0;

    while ((c = getopt_long(argc,
                            argv,
                            "p:vh",
                            long_options,
                            &long_index)) != -1)
    {
        switch (c)
        {
        case 'p':
            tcp_port = atoi(optarg);
            break;
        case 'v':
#if DEBUG
            set_verbosity(1);
#else
            printf("No verbose logs for release mode");
#endif
            break;
        case 'h':
            usage();
            return -1;
        default:
            wth_error("Try %s -h for more information.\n", argv[0]);
            return -1;
        }
    }

    if (tcp_port == 0)
    {
        wth_error("TCP port not set \n");
        wth_error("Try %s -h for more information.\n", argv[0]);
        return -1;
    }

    return 0;
}

static int
watch_ctl(struct watch *w, int op, uint32_t events)
{
    wth_verbose("%s >>> \n",__func__);
    struct epoll_event ee;

    ee.events = events;
    ee.data.ptr = w;
    wth_verbose(" <<< %s \n",__func__);
    return epoll_ctl(w->receiver->epoll_fd, op, w->fd, &ee);
}

/**
* listen_socket_handle_data
*
* Handles all incoming events on socket
*
* @param names        struct watch *w ,uint32_t events
* @param value        pointer to watch connection it holds receiver information, Incoming events information
* @return             none
*/
static void
listen_socket_handle_data(struct watch *w, uint32_t events)
{
    wth_verbose("%s >>> \n",__func__);
    struct receiver *srv = container_of(w, struct receiver, listen_watch);

    if (events & EPOLLERR) {
        wth_error("Listening socket errored out.\n");
        srv->running = false;

        return;
    }

    if (events & EPOLLHUP) {
        wth_error("Listening socket hung up.\n");
        srv->running = false;

        return;
    }

    if (events & EPOLLIN)
    {
        wth_verbose("EPOLLIN evnet received. \n");
        receiver_accept_client(srv);
    }
    wth_verbose(" <<< %s \n",__func__);
}

/**
* receiver_mainloop
*
* This is the main loop, which will flush all pending clients requests and
* listen to input events from socket
*
* @param names        void *data
* @param value        pointer to receiver struct -
*                     struct holds the client connection information
* @return             none
*/
static void
receiver_mainloop(struct receiver *srv)
{
    wth_verbose("%s >>> \n",__func__);

    struct epoll_event ee[MAX_EPOLL_WATCHES];
    struct watch *w;
    int count;
    int i;

    srv->running = true;

    while (srv->running) {
        /* Run any idle tasks at this point. */

        receiver_flush_clients(srv);

        /* Wait for events or signals */
        count = epoll_wait(srv->epoll_fd,
                   ee, ARRAY_LENGTH(ee), -1);
        if (count < 0 && errno != EINTR) {
            perror("Error with epoll_wait");
            break;
        }

        /* Handle all fds, both the listening socket
         * (see listen_socket_handle_data()) and clients
         * (see connection_handle_data()).
         */
        for (i = 0; i < count; i++) {
            w = ee[i].data.ptr;
            w->cb(w, ee[i].events);
        }
    }
    wth_verbose(" <<< %s \n",__func__);
}

static int
receiver_listen(uint16_t tcp_port)
{
    wth_verbose("%s >>> \n",__func__);
    int fd;
    int reuse = 1;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcp_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        wth_error("Failed to bind to port %d", tcp_port);
        close(fd);
        return -1;
    }

    if (listen(fd, 1024) < 0) {
        wth_error("Failed to listen to port %d", tcp_port);
        close (fd);
        return -1;
    }

    wth_verbose(" <<< %s \n",__func__);
    return fd;
}

static bool *signal_int_handler_run_flag;

static void
signal_int_handler(int signum)
{
    wth_verbose("%s >>> \n",__func__);
    if (!*signal_int_handler_run_flag)
        abort();

    *signal_int_handler_run_flag = false;
    wth_verbose(" <<< %s \n",__func__);
}

static void
set_sigint_handler(bool *running)
{
    wth_verbose("%s >>> \n",__func__);
    struct sigaction sigint;

    signal_int_handler_run_flag = running;
    sigint.sa_handler = signal_int_handler;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);
    wth_verbose(" <<< %s \n",__func__);
}

/**
* main
*
* waltham receiver main function, it accepts tcp port number as argument.
* Establishes connection on the port and listen to port for incoming connection
* request from waltham clients
*
* @param names        argv - argument list and argc -argument count
* @param value        tcp port number as argument
* @return             0 on success, -1 on error
*/
int
main(int argc, char *argv[])
{
    struct receiver srv = { 0 };
    struct client *c;

    wth_verbose("%s >>> \n",__func__);

    /* Get command line arguments */
    if (parse_args(argc, argv) != 0)
    {
        return -1;
    }

    set_sigint_handler(&srv.running);

    wl_list_init(&srv.client_list);

    srv.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (srv.epoll_fd == -1) {
        perror("Error on epoll_create1");
        exit(1);
    }

    srv.listen_fd = receiver_listen(tcp_port);
    if (srv.listen_fd < 0) {
        perror("Error setting up listening socket");
        exit(1);
    }

    srv.listen_watch.receiver = &srv;
    srv.listen_watch.cb = listen_socket_handle_data;
    srv.listen_watch.fd = srv.listen_fd;
    if (watch_ctl(&srv.listen_watch, EPOLL_CTL_ADD, EPOLLIN) < 0) {
        perror("Error setting up listen polling");
        exit(1);
    }

    wth_verbose("Waltham receiver listening on TCP port %u...\n",tcp_port);


    receiver_mainloop(&srv);

    /* destroy all things */
    wl_list_last_until_empty(c, &srv.client_list, link)
	    client_destroy(c);

    close(srv.listen_fd);
    close(srv.epoll_fd);

    wth_verbose(" <<< %s \n",__func__);
    return 0;
}
