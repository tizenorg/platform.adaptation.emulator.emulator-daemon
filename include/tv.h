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

#ifndef __TV_H__
#define __TV_H__

#define IJTYPE_GESTURE      "gesture"
#define IJTYPE_VOICE        "voice"
#define IJTYPE_EI           "ei"

#define VCONF_HDMI1 "memory/sysman/hdmi1"
#define VCONF_HDMI2 "memory/sysman/hdmi2"
#define VCONF_HDMI3 "memory/sysman/hdmi3"
#define VCONF_HDMI4 "memory/sysman/hdmi4"
#define VCONF_AV1   "memory/sysman/av1"
#define VCONF_AV2   "memory/sysman/av1"
#define VCONF_COMP1 "memory/sysman/comp1"
#define VCONF_COMP2 "memory/sysman/comp1"

bool msgproc_gesture(ijcommand* ijcmd);
bool msgproc_voice(ijcommand* ijcmd);
bool msgproc_extinput(ijcommand* ijcmd);

#endif
