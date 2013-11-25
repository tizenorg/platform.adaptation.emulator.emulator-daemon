/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
 * SooYoung Ha <yoosah.ha@samsnung.com>
 * Sungmin Ha <sungmin82.ha@samsung.com>
 * Daiyoung Kim <daiyoung777.kim@samsung.com>
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

#include "evdi.h"
#include "emuld.h"

#define DEVICE_NODE_PATH    "/dev/evdi0"

static pthread_mutex_t mutex_evdi = PTHREAD_MUTEX_INITIALIZER;

evdi_fd open_device(void)
{
    evdi_fd fd;

    fd = open(DEVICE_NODE_PATH, O_RDWR); //O_CREAT|O_WRONLY|O_TRUNC.
    printf("evdi open fd is %d", fd);

    if (fd <= 0) {
        printf("open %s fail", DEVICE_NODE_PATH);
        return fd;
    }

    return fd;
}

bool set_nonblocking(evdi_fd fd)
{
    int opts;
    opts= fcntl(fd, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl failed\n");
        return false;
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opts) < 0)
    {
        perror("fcntl failed\n");
        return false;
    }
    return true;
}

bool init_device(evdi_fd* ret_fd)
{
    evdi_fd fd;

    *ret_fd = -1;

    fd = open_device();
    if (fd <= 0)
        return false;

    if (!set_nonblocking(fd))
    {
        close(fd);
        return false;
    }

    if (!epoll_ctl_add(fd))
    {
        fprintf(stderr, "Epoll control fails.\n");
        close(fd);
        return false;
    }

    *ret_fd = fd;

    return true;
}

bool send_to_evdi(evdi_fd fd, const char* data, const int len)
{
    printf("send to evdi client, len = %d\n", len);
    int ret;

    ret = write(fd, data, len);

    printf("written bytes = %d\n", ret);

    if (ret == -1)
        return false;
    return true;
}

bool ijmsg_send_to_evdi(evdi_fd fd, const char* cat, const char* data, const int len)
{
    _auto_mutex _(&mutex_evdi);

    LOG("ijmsg_send_to_evdi\n");

    if (fd == -1)
        return false;

    char tmp[ID_SIZE];
    memset(tmp, 0, ID_SIZE);
    strncpy(tmp, cat, 10);

    // TODO: need to make fragmented transmission
    if (len + ID_SIZE > __MAX_BUF_SIZE) {
        LOG("evdi message len is too large\n");
        return false;
    }

    msg_info _msg;
    memset(_msg.buf, 0, __MAX_BUF_SIZE);
    memcpy(_msg.buf, tmp, ID_SIZE);
    memcpy(_msg.buf + ID_SIZE, data, len);

    _msg.route = route_control_server;
    _msg.use = len + ID_SIZE;
    _msg.count = 1;
    _msg.index = 0;
    _msg.cclisn = 0;

    LOG("ijmsg_send_to_evdi - %s", _msg.buf);

    if (!send_to_evdi(fd, (char*) &_msg, sizeof(_msg)))
        return false;

    return true;
}

bool msg_send_to_evdi(evdi_fd fd, const char* data, const int len)
{
    _auto_mutex _(&mutex_evdi);

    // TODO: need to make fragmented transmission
    if (len > __MAX_BUF_SIZE)
    {
        LOG("evdi message len is too large\n");
        return false;
    }

    msg_info _msg;
    memset(_msg.buf, 0, __MAX_BUF_SIZE);
    memcpy(_msg.buf, data, len);

    _msg.route = route_control_server;
    _msg.use = len;
    _msg.count = 1;
    _msg.index = 0;
    _msg.cclisn = 0;

    LOG("msg_send_to_evdi - %s", _msg.buf);

    if (!send_to_evdi(fd, (char*)&_msg, sizeof(_msg)))
        return false;

    return true;
}

