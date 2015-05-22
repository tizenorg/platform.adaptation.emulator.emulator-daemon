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

#ifndef __MOBILE_H__
#define __MOBILE_H__

#define TID_SENSOR          5

#define IJTYPE_SENSOR       "sensor"

#define VCONF_DBTAP      "memory/private/sensor/800004"
#define VCONF_SHAKE      "memory/private/sensor/800002"
#define VCONF_SNAP       "memory/private/sensor/800001"
#define VCONF_MOVETOCALL "memory/private/sensor/800020"

#define VCONF_RSSI       "memory/telephony/rssi"

void msgproc_sensor(ijcommand* ijcmd);

#endif
