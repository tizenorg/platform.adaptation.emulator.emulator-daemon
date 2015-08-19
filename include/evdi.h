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


#ifndef __EVDI_H__
#define __EVDI_H__

#define __MAX_BUF_SIZE  1024

#include <stdarg.h>
#include <stdint.h>

enum
{
    route_qemu = 0,
    route_control_server = 1,
    route_monitor = 2
};

typedef unsigned int CSCliSN;

struct msg_info {
    char buf[__MAX_BUF_SIZE];

    uint32_t route;
    uint32_t use;
    uint16_t count;
    uint16_t index;

    CSCliSN cclisn;
};

typedef int     evdi_fd;

bool ijmsg_send_to_evdi(evdi_fd fd, const char* cat, const char* data, const int len);
bool send_to_evdi(evdi_fd fd, const char* msg, const int len);
bool msg_send_to_evdi(evdi_fd fd, const char* data, const int len);

#endif
