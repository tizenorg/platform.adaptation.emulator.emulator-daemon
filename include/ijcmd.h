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

#ifndef __IJCMD_H__
#define __IJCMD_H__

#include <stdlib.h>
#include <string.h>

#define CMD_SIZE 48

// command
#define IJTYPE_SUSPEND      "suspend"
#define IJTYPE_HDS          "hds"
#define IJTYPE_SYSTEM       "system"
#define IJTYPE_GUEST        "guest"
#define IJTYPE_CMD          "cmd"
#define IJTYPE_PACKAGE      "package"
#define IJTYPE_BOOT         "boot"
#define IJTYPE_VCONF        "vconf"
#define IJTYPE_LOCATION     "location"

// struct
struct fd_info
{
    fd_info() : fd(-1){}
    int fd;
    int fdtype;
};

struct LXT_MESSAGE
{
    unsigned short length;
    unsigned char group;
    unsigned char action;
    void *data;
};

typedef struct LXT_MESSAGE LXT_MESSAGE;

struct ijcommand
{
    ijcommand() : data(NULL)
    {
        memset(cmd, 0, CMD_SIZE);
    }
    ~ijcommand()
    {
        if (data)
        {
            free(data);
            data = NULL;
        }
    }
    char cmd[CMD_SIZE];
    char* data;
    fd_info fdinfo;

    LXT_MESSAGE msg;
};
#endif // __IJCMD_H__
