/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
 * DaiYoung Kim <daiyoung777.kim@samsnung.com>
 * SooYoung Ha <yoosah.ha@samsnung.com>
 * Sungmin Ha <sungmin82.ha@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "emuld.h"
#include "synbuf.h"

#include <queue>

/* global definition */
typedef std::queue<msg_info*> __msg_queue;
__msg_queue g_msgqueue;

int g_epoll_fd;
int g_fd[fdtype_max];
pthread_t tid[MAX_CLIENT + 1];
struct epoll_event g_events[MAX_EVENTS];
bool exit_flag = false;

static void init_fd(void)
{
    register int i;

    for(i = 0 ; i < fdtype_max ; i++)
    {
        g_fd[i] = -1;
    }
}

bool epoll_ctl_add(const int fd)
{
    struct epoll_event events;

    events.events = EPOLLIN;    // check In event
    events.data.fd = fd;

    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &events) < 0 )
    {
        LOGERR("Epoll control fails.");
        return false;
    }

    LOGINFO("[START] epoll events add fd success for server");
    return true;
}

static bool init_server(int svr_port, int* ret_fd)
{
    struct sockaddr_in serv_addr;
    int fd;

    *ret_fd = -1;

    /* Open TCP Socket */
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOGERR("Server Start Fails. : Can't open stream socket");
        return false;
    }

    /* Address Setting */
    memset(&serv_addr , 0 , sizeof(serv_addr)) ;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(svr_port);

    /* Set Socket Option  */
    int nSocketOpt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &nSocketOpt, sizeof(nSocketOpt)) < 0)
    {
        LOGERR("Server Start Fails. : Can't set reuse address");
        goto fail;
    }

    /* Bind Socket */
    if (bind(fd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        LOGERR("Server Start Fails. : Can't bind local address");
        goto fail;
    }

    /* Listening */
    if (listen(fd, 15) < 0) /* connection queue is 15. */
    {
        LOGERR("Server Start Fails. : listen failure");
        goto fail;
    }

	LOGINFO("[START] Now Server listening on port %d, EMdsockfd: %d"
            ,svr_port, fd);

    if (!epoll_ctl_add(fd))
    {
        LOGERR("Epoll control fails.");
        goto fail;
    }

    *ret_fd = fd;

    return true;
fail:
    close(fd);
    return false;
}

static bool epoll_init(void)
{
    g_epoll_fd = epoll_create(MAX_EVENTS); // create event pool
    if(g_epoll_fd < 0)
    {
        LOGERR("Epoll create Fails.");
        return false;
    }

    LOGINFO("[START] epoll creation success");
    return true;
}

int recv_data(int event_fd, char** r_databuf, int size)
{
    int recvd_size = 0;
    int len = 0;
    int getcnt = 0;
    char* r_tmpbuf = NULL;
    const int alloc_size = sizeof(char) * size + 1;

    r_tmpbuf = (char*)malloc(alloc_size);
    if(r_tmpbuf == NULL)
    {
        return -1;
    }

    char* databuf = (char*)malloc(alloc_size);
    if(databuf == NULL)
    {
        free(r_tmpbuf);
        *r_databuf = NULL;
        return -1;
    }

    memset(databuf, '\0', alloc_size);

    while(recvd_size < size)
    {
        memset(r_tmpbuf, '\0', alloc_size);
        len = recv(event_fd, r_tmpbuf, size - recvd_size, 0);
        if (len < 0) {
            break;
        }

        memcpy(databuf + recvd_size, r_tmpbuf, len);
        recvd_size += len;
        getcnt++;
        if(getcnt > MAX_GETCNT) {
            break;
        }
    }
    free(r_tmpbuf);
    r_tmpbuf = NULL;

    *r_databuf = databuf;

    return recvd_size;
}

static int read_header(int fd, LXT_MESSAGE* packet)
{
    char* readbuf = NULL;
    int readed = recv_data(fd, &readbuf, HEADER_SIZE);
    if (readed <= 0)
        return 0;
    memcpy((void*) packet, (void*) readbuf, HEADER_SIZE);

    if (readbuf)
    {
        free(readbuf);
        readbuf = NULL;
    }
    return readed;
}


bool read_ijcmd(const int fd, ijcommand* ijcmd)
{
    int readed;
    readed = read_header(fd, &ijcmd->msg);

    LOGDEBUG("action: %d", ijcmd->msg.action);
    LOGDEBUG("length: %d", ijcmd->msg.length);

    if (readed <= 0)
        return false;

    // TODO : this code should removed, for telephony
    if (ijcmd->msg.length == 0)
    {
        if (ijcmd->msg.action == 71)    // that's strange packet from telephony initialize
        {
            ijcmd->msg.length = 4;
        }
    }

    if (ijcmd->msg.length <= 0)
        return true;

    if (ijcmd->msg.length > 0)
    {
        readed = recv_data(fd, &ijcmd->data, ijcmd->msg.length);
        if (readed <= 0)
        {
            free(ijcmd->data);
            ijcmd->data = NULL;
            return false;
        }

    }
    return true;
}

void recv_from_evdi(evdi_fd fd)
{
    LOGDEBUG("recv_from_evdi");
    int readed;
	synbuf g_synbuf;

    struct msg_info _msg;
    int to_read = sizeof(struct msg_info);

    memset(&_msg, 0x00, sizeof(struct msg_info));


    while (1)
    {
        readed = read(fd, &_msg, to_read);
        if (readed == -1) // TODO : error handle
        {
            if (errno != EAGAIN)
            {
                LOGERR("EAGAIN");
                return;
            }
        }
        else
        {
            break;
        }
    }

    LOGDEBUG("total readed  = %d, read count = %d, index = %d, use = %d, msg = %s",
            readed, _msg.count, _msg.index, _msg.use, _msg.buf);

    g_synbuf.reset_buf();
    g_synbuf.write(_msg.buf, _msg.use);

    ijcommand ijcmd;
    readed = g_synbuf.read(ijcmd.cmd, ID_SIZE);

    LOGDEBUG("ij id : %s", ijcmd.cmd);

    // TODO : check
    if (readed < ID_SIZE)
        return;

    // read header
    readed = g_synbuf.read((char*)&ijcmd.msg, HEADER_SIZE);
    if (readed < HEADER_SIZE)
        return;

    LOGDEBUG("HEADER : action = %d, group = %d, length = %d",
			ijcmd.msg.action, ijcmd.msg.group, ijcmd.msg.length);

    if (ijcmd.msg.length > 0)
    {
        ijcmd.data = (char*) malloc(ijcmd.msg.length);
        if (!ijcmd.data)
        {
            LOGERR("failed to allocate memory");
            return;
        }
        readed = g_synbuf.read(ijcmd.data, ijcmd.msg.length);

        LOGDEBUG("DATA : %s", ijcmd.data);

        if (readed < ijcmd.msg.length)
        {
            LOGERR("received data is insufficient");
        }
    }

    process_evdi_command(&ijcmd);
}

bool accept_proc(const int server_fd)
{
    struct sockaddr_in cli_addr;
    int cli_sockfd;
    int cli_len = sizeof(cli_addr);

    cli_sockfd = accept(server_fd, (struct sockaddr *)&cli_addr,(socklen_t *)&cli_len);
    if(cli_sockfd < 0)
    {
        LOGERR("accept error");
        return false;
    }
    else
    {
        LOGINFO("[Accept] New client connected. fd:%d, port:%d"
                ,cli_sockfd, cli_addr.sin_port);

        clipool_add(cli_sockfd, cli_addr.sin_port, fdtype_ij);
        epoll_ctl_add(cli_sockfd);
    }
    return true;
}

int main( int argc , char *argv[])
{
	int g_svr_port = DEFAULT_PORT;

    LOGINFO("emuld start");

	/* entry , argument check and process */
    if(argc >= 3 && strcmp("-port", argv[1]) ==  0 ) {
        g_svr_port = atoi(argv[2]);
        if(g_svr_port < 1024) {
            LOGERR("[STOP] port number invalid : %d",g_svr_port);
            exit(0);
        }
    }

    init_fd();

    if (!epoll_init())
    {
        exit(0);
    }

    if (!init_server(g_svr_port, &g_fd[fdtype_server]))
    {
        close(g_epoll_fd);
        exit(0);
    }

    if (!init_device(&g_fd[fdtype_device]))
    {
        close(g_epoll_fd);
        exit(0);
    }

    LOGINFO("[START] epoll events set success for server");

	init_profile();

    send_default_suspend_req();

    while(!exit_flag)
    {
        exit_flag = server_process();
    }

	exit_profile();

    stop_listen();

    if (g_fd[fdtype_server])
        close(g_fd[fdtype_server]);

    LOGINFO("emuld exit");

    return 0;
}

