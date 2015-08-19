/*
 * emulator-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song <ch81.song@samsung.com>
 * Jinhyung Choi <jinh0.choi@samsnung.com>
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

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "libemuld.h"

static pthread_mutex_t mutex_evdi = PTHREAD_MUTEX_INITIALIZER;

bool send_to_evdi(evdi_fd fd, const char* data, const int len)
{
    LOGDEBUG("send to evdi client, len = %d", len);
    int ret;

    ret = write(fd, data, len);

    LOGDEBUG("written bytes = %d", ret);

    if (ret == -1)
        return false;
    return true;
}

bool ijmsg_send_to_evdi(evdi_fd fd, const char* cat, const char* data, const int len)
{
    _auto_mutex _(&mutex_evdi);

    LOGDEBUG("ijmsg_send_to_evdi");

    if (fd == -1)
        return false;

    char tmp[ID_SIZE];
    memset(tmp, 0, ID_SIZE);
    strncpy(tmp, cat, ID_SIZE - 1);

    // TODO: need to make fragmented transmission
    if (len + ID_SIZE > __MAX_BUF_SIZE) {
        LOGERR("evdi message len is too large");
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

    LOGDEBUG("ijmsg_send_to_evdi - %s", _msg.buf);

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
        LOGERR("evdi message len is too large");
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

    LOGDEBUG("msg_send_to_evdi - %s", _msg.buf);

    if (!send_to_evdi(fd, (char*)&_msg, sizeof(_msg)))
        return false;

    return true;
}

