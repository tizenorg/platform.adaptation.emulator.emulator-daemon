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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "log.h"

#define EMD_DEBUG

struct emuld_log
{
    emuld_log()
    {
        m_isLogout = false;

        char* buf = getenv("EMULD_LOG");
        if (buf != NULL) {
            fprintf(stdout, "env EMULD_LOG is set => print logs \n");
            m_isLogout = true;
        }
    }

    void out_v(const char *fmt, ...)
    {
        if (m_isLogout)
        {
            char buf[4096];
            va_list ap;

            va_start(ap, fmt);
            vsnprintf(buf, sizeof(buf), fmt, ap);
            va_end(ap);

            fprintf(stdout, "%s", buf);
            FILE* log_fd = fopen("/var/log/emuld.log", "a");
            fprintf(log_fd, "%s", buf);
            fclose(log_fd);
        }
    }

    void out(const char* outbuf)
        {
            if (!m_isLogout)
                return;

            char timestr[512];
            make_timestamp(timestr);
            fprintf(stdout, "%s - %s", timestr, outbuf);
            FILE* log_fd = fopen("/var/log/emuld.log", "a");
            fprintf(log_fd, "%s - %s", timestr, outbuf);
            fclose(log_fd);
        }

    bool m_isLogout;
};


static emuld_log g_log;

void log_print_out(const char *fmt, ...)
{

#ifdef EMD_DEBUG

    char buf[4096];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    g_log.out(buf);
#endif

    return;
}

void make_timestamp(char* ret)
{
    time_t ltime;
    ltime = time(NULL);

    struct tm* ts;
    ts = localtime(&ltime);

    strftime(ret, 512, "%a_%Y-%m-%d_%H-%M-%S", ts);
}


