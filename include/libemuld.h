/*
 * emulator-daemon library
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song   <ch81.song@samsung.com>
 * Hakhyun Kim   <haken.kim@samsung.com>
 * Jinhyung choi <jinh0.choi@samsung.com>
 * Sangho Park   <sangho.p@samsung.com>
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


#ifndef __LIBEMULD_H__
#define __LIBEMULD_H__

// include
#include <vconf.h>
#include <pthread.h>
#include <iostream>

#include "ijcmd.h"
#include "evdi.h"
#include "msgproc.h"
#include "emuld_log.h"

#define FDTYPE_MAX          6

#define HEADER_SIZE         4
#define ID_SIZE             10

enum VCONF_TYPE {
    SENSOR    = 0,
    TELEPHONY = 1,
    LOCATION  = 2,
    TV        = 3,
    MEMORY    = 4
};

enum
{
    fdtype_device     = 1,
    fdtype_ij         = 4,
    fdtype_max        = FDTYPE_MAX
};

struct _auto_mutex
{
    _auto_mutex(pthread_mutex_t* t)
    {
        _mutex = t;
        pthread_mutex_lock(_mutex);

    }
    ~_auto_mutex()
    {
        pthread_mutex_unlock(_mutex);
    }

    pthread_mutex_t* _mutex;
};

// function
extern "C" {
bool emuld_plugin_init(void);
}

void add_vconf_map(VCONF_TYPE key, std::string value);
bool check_possible_vconf_key(std::string key);

int get_vconf_status(char** value, vconf_t type, const char* key);
void send_to_ecs(const char* cat, int group, int action, char* data);
void systemcall(const char* param);

extern int g_fd[fdtype_max];
#endif // __LIBEMULD_H__
