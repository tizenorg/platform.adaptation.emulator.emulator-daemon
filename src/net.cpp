/*
 * emulator-daemon
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Munkyu Im <munkyu.im@samsnung.com>
 * Sangho Park <sangho1206.park@samsung.com>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include "emuld.h"

#define PROC_CMDLINE_PATH "/proc/cmdline"
#define IP_SUFFIX "ip="
#define IJTYPE_GUESTIP "guest_ip"
char g_guest_ip[64];
static void send_guest_ip_req(void)
{
    LXT_MESSAGE *packet = (LXT_MESSAGE *)calloc(1 ,sizeof(LXT_MESSAGE));
    if (packet == NULL) {
        return;
    }

    packet->length = strlen(g_guest_ip);
    packet->group = 0;
    packet->action = STATUS;

    const int tmplen = HEADER_SIZE + packet->length;
    char *tmp = (char *)malloc(tmplen);
    if (!tmp) {
        if (packet)
            free(packet);
        return;
    }

    memcpy(tmp, packet, HEADER_SIZE);
    memcpy(tmp + HEADER_SIZE, g_guest_ip, packet->length);

    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_GUESTIP, (const char*) tmp, tmplen);


    if (tmp)
        free(tmp);
    if (packet)
        free(packet);
}

static char *s_strncpy(char *dest, const char *source, size_t n)
{
    char *start = dest;

    while (n && (*dest++ = *source++)) {
        n--;
    }
    if (n) {
        while (--n) {
            *dest++ = '\0';
        }
    }
    return start;
}


static int get_str_cmdline(char *src, const char *dest, char str[], int str_size)
{
    char *s = strstr(src, dest);
    if (s == NULL) {
        return -1;
    }
    char *e = strstr(s, " ");
    if (e == NULL) {
        return -1;
    }

    int len = e-s-strlen(dest);

    if (len >= str_size) {
        LOGERR("buffer size(%d) should be bigger than %d\n", str_size, len+1);
        return -1;
    }

    s_strncpy(str, s + strlen(dest), len);
    return len;
}

static int get_network_info(char str[], int str_size)
{
    size_t len = 0;
    ssize_t read;
    char *line = NULL;
    FILE *fp = fopen(PROC_CMDLINE_PATH, "r");

    if (fp == NULL) {
        LOGERR("fail to read /proc/cmdline\n");
        return -1;
    }
    while((read = getline(&line, &len, fp)) != -1) {
        LOGERR("line: %s\n", line);
        LOGERR("len: %d\n", len);
    }
    if (get_str_cmdline(line, IP_SUFFIX, str, str_size) < 1) {
        LOGERR("could not get the (%s) value from cmdline\n", IP_SUFFIX);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

void get_host_addr()
{
    int fd;
    struct ifreq ifrq;
    struct sockaddr_in *sin;
    char guest_net[256] = {0,};
    if (get_network_info(guest_net, sizeof guest_net) == 0) {
        char *token;
        int i = 0;
        char *str = strdup(guest_net);
        while ((token = strsep(&str, ":"))) {
            if (i == 0) {
                strncpy(g_guest_ip, token, strlen(token));
                LOGDEBUG("set guest_ip: %s\n", g_guest_ip);
            }
            LOGDEBUG("token[%d]: %s\n",i++, token);
        }
        free(str);
    } else {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            perror("Socket open error");
            return;
        }
        strcpy(ifrq.ifr_name, "eth0");
        if (ioctl(fd, SIOCGIFADDR, &ifrq) < 0) {
            perror("IPADDR Error");
            close(fd);
            return;
        }
        sin = (struct sockaddr_in *)&ifrq.ifr_addr;
        LOGDEBUG("IPADDR : %s\n", inet_ntoa(sin->sin_addr));
        strncpy(g_guest_ip,  inet_ntoa(sin->sin_addr), strlen(inet_ntoa(sin->sin_addr)));
        LOGDEBUG("set guest_ip: %s\n", g_guest_ip);

        close(fd);
    }
    send_guest_ip_req();
}

