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

#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "emuld.h"

enum
{
    fdtype_pedometer  = 3
};

#define PEDOMETER_PORT      3600

#define IJTYPE_PEDOMETER    "pedometer"

unsigned short pedometer_port = PEDOMETER_PORT;
static int g_pedometer_connect_status;
static pthread_mutex_t mutex_pedometerconnect = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_pedo = PTHREAD_MUTEX_INITIALIZER;

bool is_pedometer_connected(void)
{
    _auto_mutex _(&mutex_pedometerconnect);

    if (g_pedometer_connect_status != 1)
        return false;

    return true;
}

bool msgproc_pedometer(const int sockfd, ijcommand* ijcmd)
{
    _auto_mutex _(&mutex_pedo);

    int sent = 0;
    const char* data = NULL;

    if (!is_pedometer_connected() || !ijcmd->data)
        return false;

    LOGDEBUG("send data state: %s", ijcmd->data);

    sent = send(g_fd[fdtype_pedometer], ijcmd->data, 1, 0);
    if (sent == -1)
    {
        LOGERR("pedometer send error");
    }

    LOGDEBUG("sent to pedometer daemon: %d byte", sent);

    return true;
}

void set_pedometer_connect_status(const int v)
{
    _auto_mutex _(&mutex_pedometerconnect);

    g_pedometer_connect_status = v;
}

void* init_pedometer_connect(void* data)
{
    struct sockaddr_in pedometer_addr;
    int ret = -1;

    set_pedometer_connect_status(0);

    LOGINFO("init_pedometer_connect start\n");

    pthread_detach(pthread_self());
    if ((g_fd[fdtype_pedometer] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOGERR("Server Start Fails. : Can't open stream socket \n");
        exit(0);
    }

    memset(&pedometer_addr , 0 , sizeof(pedometer_addr)) ;

    pedometer_addr.sin_family = AF_INET;
    pedometer_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    pedometer_addr.sin_port = htons(pedometer_port);

    while (ret < 0 && !exit_flag)
    {
        ret = connect(g_fd[fdtype_pedometer], (struct sockaddr *)&pedometer_addr, sizeof(pedometer_addr));

        LOGDEBUG("pedometer_sockfd: %d, connect ret: %d\n", g_fd[fdtype_pedometer], ret);

        if(ret < 0) {
            LOGDEBUG("connection failed to pedometer! try \n");
            sleep(1);
        }
    }

    epoll_ctl_add(g_fd[fdtype_pedometer]);

    set_pedometer_connect_status(1);

    pthread_exit((void *) 0);
}

void recv_from_pedometer(int fd)
{
    printf("recv_from_pedometer\n");

    ijcommand ijcmd;
    if (!read_ijcmd(fd, &ijcmd))
    {
        LOGERR("fail to read ijcmd\n");

        set_pedometer_connect_status(0);

        close(fd);

        if (pthread_create(&tid[3], NULL, init_pedometer_connect, NULL) != 0)
        {
            LOGERR("pthread create fail!");
        }
        return;
    }

    LOGDEBUG("pedometer data length: %d", ijcmd.msg.length);
    const int tmplen = HEADER_SIZE + ijcmd.msg.length;
    char* tmp = (char*) malloc(tmplen);

    if (tmp)
    {
        memcpy(tmp, &ijcmd.msg, HEADER_SIZE);
        if (ijcmd.msg.length > 0)
            memcpy(tmp + HEADER_SIZE, ijcmd.data, ijcmd.msg.length);

        if(!ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_PEDOMETER, (const char*) tmp, tmplen)) {
            LOGERR("msg_send_to_evdi: failed\n");
        }

        free(tmp);
    }
}

void process_evdi_command(ijcommand* ijcmd)
{
    int fd = -1;

    if (strncmp(ijcmd->cmd, "suspend", 7) == 0)
    {
        msgproc_suspend(fd, ijcmd);
    }
    else if (strncmp(ijcmd->cmd, "pedometer", 9) == 0)
    {
        msgproc_pedometer(fd, ijcmd);
    }
    else if (strncmp(ijcmd->cmd, "location", 8) == 0)
    {
        msgproc_location(fd, ijcmd);
    }
    else if (strncmp(ijcmd->cmd, "system", 6) == 0)
    {
        msgproc_system(fd, ijcmd);
    }
    else if (strncmp(ijcmd->cmd, "sdcard", 6) == 0)
    {
        msgproc_sdcard(fd, ijcmd);
    }
    else
    {
        LOGERR("Unknown packet: %s", ijcmd->cmd);
    }
}

bool server_process(void)
{
    int i,nfds;
    int fd_tmp;

    nfds = epoll_wait(g_epoll_fd, g_events, MAX_EVENTS, 100);

    if (nfds == -1 && errno != EAGAIN && errno != EINTR)
    {
        LOGERR("epoll wait(%d)", errno);
        return true;
    }

    for( i = 0 ; i < nfds ; i++ )
    {
        fd_tmp = g_events[i].data.fd;
        if (fd_tmp == g_fd[fdtype_server])
        {
            accept_proc(fd_tmp);
        }
        else if (fd_tmp == g_fd[fdtype_device])
        {
            recv_from_evdi(fd_tmp);
        }
        else if (fd_tmp == g_fd[fdtype_pedometer])
        {
            recv_from_pedometer(fd_tmp);
        }
        else
        {
        	LOGERR("unknown request event fd : (%d)", fd_tmp);
        }
    }

    return false;
}

void init_profile(void)
{
    if(pthread_create(&tid[3], NULL, init_pedometer_connect, NULL) != 0)
    {
        LOGERR("pthread create fail!");
        close(g_epoll_fd);
        exit(0);
    }
}

void exit_profile(void)
{
	int state;
    if (!is_pedometer_connected())
    {
        int status;
        pthread_join(tid[3], (void **)&status);
        LOGINFO("pedometer thread end %d\n", status);
	}

    state = pthread_mutex_destroy(&mutex_pedometerconnect);
    if (state != 0)
    {
        LOGERR("mutex_pedometerconnect is failed to destroy.");
    }
}
