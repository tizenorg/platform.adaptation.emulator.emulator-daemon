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


#ifndef __EMULD_LOG_H__
#define __EMULD_LOG_H__

void writelog(const char* fmt, ...);

#if defined(ENABLE_DLOG_OUT)
#  define LOG_TAG           "EMULD"
#  include <dlog/dlog.h>
#  define LOGINFO LOGI
#  define LOGERR LOGE
#  define LOGDEBUG LOGD
#  define LOGWARN LOGW
#  define LOGFAIL(expr, fmt, ...)  \
        do {    \
            if (expr)  \
                LOGW(fmt, ##__VA_ARGS__);  \
        } while (0)
#else
#  define LOG_TAG           "EMULD"
#  include <dlog/dlog.h>
#  define LOGINFO(fmt, ...)     \
        do {    \
            writelog(fmt, ##__VA_ARGS__);   \
            LOGI(fmt, ##__VA_ARGS__);   \
        } while (0)
#  define LOGERR(fmt, ...)  \
        do {    \
            writelog(fmt, ##__VA_ARGS__);   \
            LOGE(fmt, ##__VA_ARGS__);   \
        } while (0)
#  define LOGDEBUG(fmt, ...)    \
        do {    \
            writelog(fmt, ##__VA_ARGS__);   \
            LOGD(fmt, ##__VA_ARGS__);   \
        } while (0)
#  define LOGWARN(fmt, ...)  \
        do {    \
            writelog(fmt, ##__VA_ARGS__);   \
            LOGW(fmt, ##__VA_ARGS__);   \
        } while (0)
#  define LOGFAIL(expr, fmt, ...)  \
        do {    \
            if (expr) {  \
                writelog(fmt, ##__VA_ARGS__);  \
                LOGW(fmt, ##__VA_ARGS__);  \
            }  \
        } while (0)
#endif

#endif // __EMULD_LOG_H__
