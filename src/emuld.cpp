/*
 * emulator-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song <ch81.song@samsung.com>
 * Jinhyung Choi <jinh0.choi@samsnung.com>
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

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <E_DBus.h>
#include <Ecore.h>

#include "emuld.h"

pthread_t tid[MAX_CLIENT + 1];
int g_epoll_fd;
struct epoll_event g_events[MAX_EVENTS];
void* dl_handles[MAX_PLUGINS];
int dl_count;

enum ioctl_cmd {
    IOCTL_CMD_BOOT_DONE,
};

static void emuld_exit(void)
{
    while (dl_count > 0)
        dlclose(dl_handles[--dl_count]);

    msgproc_del(NULL, NULL, MSGPROC_PRIO_HIGH);
    msgproc_del(NULL, NULL, MSGPROC_PRIO_MIDDLE);
    msgproc_del(NULL, NULL, MSGPROC_PRIO_LOW);
}

static void init_plugins(void)
{
    DIR *dirp = NULL;
    struct dirent *dir_ent = NULL;
    struct dirent entry;
    char plugin_path[MAX_PATH] = {0, };
    void* handle = NULL;
    bool (*plugin_init)() = NULL;
    char* error = NULL;

    dirp = opendir(EMULD_PLUGIN_DIR);

    if (!dirp)
    {
        LOGWARN("Dir(%s) open failed. errno = %d\n", EMULD_PLUGIN_DIR, errno);
        return;
    }

    while ((!readdir_r(dirp, &entry, &dir_ent)))
    {
        snprintf(plugin_path, sizeof(plugin_path), "%s/%s", EMULD_PLUGIN_DIR, dir_ent->d_name);

        LOGDEBUG("Try to load plugin (%s)", plugin_path);

        if (dl_count >= MAX_PLUGINS)
        {
            LOGWARN("Cannot load more plugins. (%s)", plugin_path);
            continue;
        }

        handle = dlopen(plugin_path, RTLD_NOW);
        if (!handle)
        {
            LOGWARN("File open failed : %s\n", dlerror());
            continue;
        }

        plugin_init = (bool(*)())dlsym(handle, EMULD_PLUGIN_INIT_FN);
        if ((error = dlerror()) != NULL)
        {
            LOGWARN("Could not found symbol : %s\n", error);
            dlclose(handle);
            continue;
        }

       if (!plugin_init()) {
            LOGWARN("emuld_plugin_init failed (%s)", plugin_path);
            dlclose(handle);
            continue;
        }

        dl_handles[dl_count++] = handle;
    }

    closedir(dirp);
}

static void sig_handler(int signo)
{
    LOGINFO("received signal: %d. EXIT!", signo);

    hds_unmount_all();

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

static void send_to_kernel(void)
{
    if(ioctl(g_fd[fdtype_device], IOCTL_CMD_BOOT_DONE, NULL) == -1) {
        LOGWARN("Failed to send ioctl to kernel");
        return;
    }
    LOGINFO("[DBUS] sent booting done to kernel");
}

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

static void init_fd(void)
{
    register int i;

    for(i = 0 ; i < fdtype_max ; i++)
    {
        g_fd[i] = -1;
    }
}

static void process_evdi_command(ijcommand* ijcmd)
{
    int prio = 0;
    LOGDEBUG("process_evdi_command : cmd = %s\n", ijcmd->cmd);

    for (prio = MSGPROC_PRIO_HIGH; prio != MSGPROC_PRIO_END; prio++)
    {
        LOGDEBUG("process_evdi_command : msgproc_head[%d].next = %p", prio, msgproc_head[prio].next);
        emuld_msgproc *pMsgProc = msgproc_head[prio].next;
        while (pMsgProc)
        {
            LOGDEBUG("pMsgProc->name = %s, pMsgProc->cmd = %s, func = %p", pMsgProc->name, pMsgProc->cmd, pMsgProc->func);
            if (strcmp(pMsgProc->cmd, ijcmd->cmd))
            {
                pMsgProc = pMsgProc->next;
                continue;
            }
            if (!pMsgProc->func(ijcmd))
            {
                LOGINFO("Stopped more message handling by ( Plugin : %s, Command : %s )", pMsgProc->name, pMsgProc->cmd);
                return;
            }
            pMsgProc = pMsgProc->next;
        }
    }
}

static void recv_from_evdi(evdi_fd fd)
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
                LOGERR("read failed. errno = %d", errno);
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

    // TODO : check
    if (readed < ID_SIZE)
        return;

    LOGDEBUG("ij id : %s", ijcmd.cmd);

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
            LOGWARN("received data is insufficient");
        }
    }

    process_evdi_command(&ijcmd);
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
            LOGWARN("unknown request event fd : (%d)", fd_tmp);
        }
    }

    return false;
}

static bool epoll_ctl_add(const int fd)
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

static evdi_fd open_device(void)
{
    evdi_fd fd;

    fd = open(DEVICE_NODE_PATH, O_RDWR); //O_CREAT|O_WRONLY|O_TRUNC.
    LOGDEBUG("evdi open fd is %d", fd);

    if (fd < 0) {
        LOGERR("open %s fail", DEVICE_NODE_PATH);
    }

    return fd;
}

static bool set_nonblocking(evdi_fd fd)
{
    int opts;
    opts= fcntl(fd, F_GETFL);
    if (opts < 0)
    {
        LOGERR("F_GETFL fcntl failed");
        return false;
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opts) < 0)
    {
        LOGERR("NONBLOCK fcntl failed");
        return false;
    }
    return true;
}

static bool init_device(evdi_fd* ret_fd)
{
    evdi_fd fd;

    *ret_fd = -1;

    fd = open_device();
    if (fd < 0)
        return false;

    if (!set_nonblocking(fd))
    {
        close(fd);
        return false;
    }

    if (!epoll_ctl_add(fd))
    {
        LOGERR("Epoll control fails.");
        close(fd);
        return false;
    }

    *ret_fd = fd;

    return true;
}

static void* handling_network(void* data)
{
    int ret = -1;
    bool exit_flag = false;

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
    LOGINFO("check directory '%s' for default fileshare: %d", HDS_DEFAULT_PATH, ret);
    ret = try_mount((char*)HDS_DEFAULT_TAG, (char*)HDS_DEFAULT_PATH);
    LOGINFO("try mount %s for default fileshare: %d", HDS_DEFAULT_PATH, ret);
    if (ret == 0) {
        send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, HDS_ACTION_DEFAULT, (char*)HDS_DEFAULT_TAG);
    }

    add_vconf_map_common();
#ifndef UNKNOWN_PROFILE
    add_vconf_map_profile();
#endif

    set_vconf_cb();

    send_emuld_connection();

    while(!exit_flag)
    {
        exit_flag = server_process();
    }

    hds_unmount_all();

    exit(0);
}

int main( int argc , char *argv[])
{
    int ret;
    void* retval = NULL;
    pthread_t conn_thread_t;
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

    add_msg_proc_common();
#ifndef UNKNOWN_PROFILE
    add_msg_proc_ext();
#endif

    init_plugins();

    if (pthread_create(&conn_thread_t, NULL, register_connection, NULL) < 0) {
        LOGERR("network connection pthread create fail!");
        return -1;
    }

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
        LOGERR("message loop pthread join is failed.");
    }

    ret = pthread_join(conn_thread_t, &retval);
    if (ret < 0) {
        LOGERR("network connection pthread join is failed.");
    }

    emuld_exit();

    return 0;
}
