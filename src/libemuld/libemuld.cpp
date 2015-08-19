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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "emuld.h"

#include <E_DBus.h>
#include <Ecore.h>
#include <map>
#include <queue>
#include "libemuld.h"

#define MAX_GETCNT 10

int g_fd[fdtype_max];

// Vconf
static std::multimap<int, std::string> vconf_multimap;

void add_vconf_map(VCONF_TYPE key, std::string value)
{
    vconf_multimap.insert(std::pair<int, std::string>(key, value));
}

bool check_possible_vconf_key(std::string key)
{
    std::multimap<int, std::string>::iterator it;
    for(it = vconf_multimap.begin(); it != vconf_multimap.end(); it++) {
        if (it->second.compare(key) == 0) {
            return true;
        }
    }
    return false;
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

void send_to_ecs(const char* cat, int group, int action, char* data)
{
    int datalen = 0;
    int tmplen = HEADER_SIZE;
    if (data != NULL) {
        datalen = strlen(data);
        tmplen += datalen;
    }

    char* tmp = (char*) malloc(tmplen);
    if (!tmp)
        return;

    memcpy(tmp, &datalen, 2);
    memcpy(tmp + 2, &group, 1);
    memcpy(tmp + 3, &action, 1);
    if (data != NULL) {
        memcpy(tmp + 4, data, datalen);
    }

    ijmsg_send_to_evdi(g_fd[fdtype_device], cat, (const char*) tmp, tmplen);

    if (tmp)
        free(tmp);
}

void systemcall(const char* param)
{
    if (!param)
        return;

    if (system(param) == -1)
        LOGERR("system call failure(command = %s)", param);
}
