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


#ifndef __MSGPROC_H__
#define __MSGPROC_H__

#include "ijcmd.h"

#define MAX_PATH 4096
#define NAME_LEN 256

typedef bool (*msgproc)(ijcommand*);

enum msgproc_prio {
    MSGPROC_PRIO_HIGH,
    MSGPROC_PRIO_MIDDLE,
    MSGPROC_PRIO_LOW,
    MSGPROC_PRIO_END
};

struct emuld_msgproc {
    char name[NAME_LEN];
    char cmd[CMD_SIZE];
    msgproc func;
    emuld_msgproc *next;
};
typedef emuld_msgproc emuld_msgproc;

extern emuld_msgproc msgproc_head[MSGPROC_PRIO_END];

bool msgproc_add(const char* name, const char* cmd, msgproc func, msgproc_prio priority);
bool msgproc_del(const char* name, const char* cmd, msgproc_prio priority);

#endif // __MSGPROC_H__
