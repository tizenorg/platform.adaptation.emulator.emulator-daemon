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
#include <stdarg.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "emuld.h"
#include "synbuf.h"

#include <E_DBus.h>
#include <Ecore.h>

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
    if (readed <= 0){
        if (readbuf)
            free(readbuf);
        return 0;
    }
    memcpy((void*) packet, (void*) readbuf, HEADER_SIZE);

    if (readbuf)
    {
        free(readbuf);
        readbuf = NULL;
    }
    return readed;
}

static void process_evdi_command(ijcommand* ijcmd)
{
    if (strcmp(ijcmd->cmd, IJTYPE_SUSPEND) == 0)
    {
        msgproc_suspend(ijcmd);
    }
    else if (strcmp(ijcmd->cmd, IJTYPE_HDS) == 0)
    {
        msgproc_hds(ijcmd);
    }
    else if (strcmp(ijcmd->cmd, IJTYPE_SYSTEM) == 0)
    {
        msgproc_system(ijcmd);
    }
    else if (strcmp(ijcmd->cmd, IJTYPE_PACKAGE) == 0)
    {
        msgproc_package(ijcmd);
    }
    else if (strcmp(ijcmd->cmd, IJTYPE_CMD) == 0)
    {
        msgproc_cmd(ijcmd);
    }
    else if (strcmp(ijcmd->cmd, IJTYPE_VCONF) == 0)
    {
        msgproc_vconf(ijcmd);
    }
    else
    {
        if (!extra_evdi_command(ijcmd)) {
            LOGERR("Unknown packet: %s", ijcmd->cmd);
        }
    }
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

void writelog(const char* fmt, ...)
{
    FILE* logfile = fopen("/tmp/emuld.log", "a+");
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    fprintf(logfile, "\n");
    va_end(args);
    fclose(logfile);
}

static bool server_process(void)
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
        if (fd_tmp == g_fd[fdtype_device])
        {
            recv_from_evdi(fd_tmp);
        }
        else
        {
            LOGERR("unknown request event fd : (%d)", fd_tmp);
        }
    }

    return false;
}

enum ioctl_cmd {
    IOCTL_CMD_BOOT_DONE,
};

void send_to_kernel(void)
{
    if(ioctl(g_fd[fdtype_device], IOCTL_CMD_BOOT_DONE, NULL) == -1) {
        LOGERR("Failed to send ioctl to kernel");
        return;
    }
    LOGINFO("[DBUS] sent booting done to kernel");
}

#define DBUS_PATH_BOOT_DONE  "/Org/Tizen/System/DeviceD/Core"
#define DBUS_IFACE_BOOT_DONE "org.tizen.system.deviced.core"
#define BOOT_DONE_SIGNAL     "BootingDone"

static void boot_done(void *data, DBusMessage *msg)
{
    if (dbus_message_is_signal(msg,
                DBUS_IFACE_BOOT_DONE,
                BOOT_DONE_SIGNAL) != 0) {
        LOGINFO("[DBUS] sending booting done to ecs.");
        send_to_ecs(IJTYPE_BOOT, 0, 0, NULL);
        LOGINFO("[DBUS] sending booting done to kernel for log.");
        send_to_kernel();
    }
}

static void sig_handler(int signo)
{
    LOGINFO("received signal: %d. EXIT!", signo);
    _exit(0);
}

static void add_sig_handler(int signo)
{
    sighandler_t sig;

    sig = signal(signo, sig_handler);
    if (sig == SIG_ERR) {
        LOGERR("adding %d signal failed : %d", signo, errno);
    }
}

void* handling_network(void* data)
{
    int ret = -1;

    init_fd();

    if (!epoll_init())
    {
        exit(0);
    }

    if (!init_device(&g_fd[fdtype_device]))
    {
        close(g_epoll_fd);
        exit(0);
    }

    send_default_suspend_req();

    LOGINFO("[START] epoll & device init success");

    send_default_mount_req();

    ret = valid_hds_path((char*)HDS_DEFAULT_PATH);
    LOGINFO("check directory '/mnt/host' for default fileshare: %d", ret);
    ret = try_mount((char*)HDS_DEFAULT_TAG, (char*)HDS_DEFAULT_PATH);
    LOGINFO("try mount /mnt/host for default fileshare: %d", ret);
    if (ret == 0) {
        send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, HDS_ACTION_DEFAULT, (char*)HDS_DEFAULT_TAG);
    }

    add_vconf_map_common();
    add_vconf_map_profile();
    set_vconf_cb();

    send_emuld_connection();

    while(!exit_flag)
    {
        exit_flag = server_process();
    }

    stop_listen();

    exit(0);
}

int main( int argc , char *argv[])
{
    int ret;
    void* retval = NULL;

    E_DBus_Connection *dbus_conn;
    E_DBus_Signal_Handler *boot_handler = NULL;

    ecore_init();

    dbus_threads_init_default();

    ret = e_dbus_init();
    if (ret == 0) {
        LOGERR("[DBUS] init value : %d", ret);
        exit(-1);
    }

    dbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
    if (!dbus_conn) {
        LOGERR("[DBUS] Failed to get dbus bus.");
        e_dbus_shutdown();
        ecore_shutdown();
        exit(-1);
    }

    boot_handler = e_dbus_signal_handler_add(
            dbus_conn,
            NULL,
            DBUS_PATH_BOOT_DONE,
            DBUS_IFACE_BOOT_DONE,
            BOOT_DONE_SIGNAL,
            boot_done,
            NULL);
    if (!boot_handler) {
        LOGERR("[DBUS] Failed to register handler");
        e_dbus_signal_handler_del(dbus_conn, boot_handler);
        e_dbus_shutdown();
        ecore_shutdown();
        exit(-1);
    }
    LOGINFO("[DBUS] signal handler is added.");

    add_sig_handler(SIGINT);
    add_sig_handler(SIGTERM);

    register_connection();

    if (pthread_create(&tid[TID_NETWORK], NULL, handling_network, NULL) != 0)
    {
        LOGERR("boot noti pthread create fail!");
        return -1;
    }

    ecore_main_loop_begin();

    e_dbus_signal_handler_del(dbus_conn, boot_handler);
    e_dbus_shutdown();
    ecore_shutdown();

    destroy_connection();

    LOGINFO("emuld exit");

    ret = pthread_join(tid[TID_NETWORK], &retval);
    if (ret < 0) {
        LOGERR("validate package pthread join is failed.");
    }

    return 0;
}

