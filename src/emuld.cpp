/* 
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
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

#include "emuld_common.h"
#include "emuld.h"
#include "synbuf.h"
#include "pmapi.h"

#define PMAPI_RETRY_COUNT       3
#define MAX_CONNECT_TRY_COUNT   (60 * 3)
#define SRV_IP "10.0.2.2"

/* global definition */
unsigned short vmodem_port = VMODEM_PORT;

/* global server port number */
int g_svr_port;

/* connection status between emuld and vmodem  */
static int g_vm_connect_status;

pthread_t tid[MAX_CLIENT + 1];

/* udp socket */
struct sockaddr_in si_sensord_other;

int g_fd[fdtype_max];

typedef std::queue<msg_info*> __msg_queue;

__msg_queue g_msgqueue;

int g_epoll_fd;

static pthread_mutex_t mutex_vmconnect = PTHREAD_MUTEX_INITIALIZER;

struct epoll_event g_events[MAX_EVENTS];

bool exit_flag = false;

/*----------------------------------------------------------------*/
/* FUNCTION PART                                                  */
/* ---------------------------------------------------------------*/

void systemcall(const char* param)
{
    if (!param)
        return;

    if (system(param) == -1)
        LOGERR("system call failure(command = %s)", param);
}

void set_lock_state(int state) {
    int i = 0;
    int ret = 0;
    // Now we blocking to enter "SLEEP".
    while(i < PMAPI_RETRY_COUNT ) {
        if (state == SUSPEND_LOCK) {
            ret = pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
        } else {
            ret = pm_unlock_state(LCD_NORMAL, PM_RESET_TIMER);
        }
        LOGINFO("pm_lock/unlock_state() return: %d", ret);
        if(ret == 0)
        {
            break;
        }
        ++i;
        sleep(10);
    }
    if (i == PMAPI_RETRY_COUNT) {
        LOGERR("Emulator Daemon: Failed to call pm_lock/unlock_state().");
    }
}

/*---------------------------------------------------------------
function : init_data0
io: none
desc: initialize global client structure values
----------------------------------------------------------------*/
void init_data0(void)
{
    register int i;

    for(i = 0 ; i < fdtype_max ; i++)
    {
        g_fd[i] = -1;
    }
}

bool is_vm_connected(void)
{
    _auto_mutex _(&mutex_vmconnect);

    if (g_vm_connect_status != 1)
        return false;

    return true;
}

void set_vm_connect_status(const int v)
{
    _auto_mutex _(&mutex_vmconnect);

    g_vm_connect_status = v;
}


/*-------------------------------------------------------------
function: init_server0
io: input : integer - server port (must be positive)
output: none
desc : tcp/ip listening socket setting with input variable
----------------------------------------------------------------*/

bool init_server0(int svr_port, int* ret_fd)
{
    struct sockaddr_in serv_addr;
    int fd;

    *ret_fd = -1;

    /* Open TCP Socket */
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOGERR("Server Start Fails. : Can't open stream socket");
        return false;
    }

    /* Address Setting */
    memset(&serv_addr , 0 , sizeof(serv_addr)) ;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(svr_port);

    /* Set Socket Option  */
    int nSocketOpt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &nSocketOpt, sizeof(nSocketOpt)) < 0)
    {
        LOGERR("Server Start Fails. : Can't set reuse address");
        goto fail;
    }

    /* Bind Socket */
    if (bind(fd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        LOGERR("Server Start Fails. : Can't bind local address");
        goto fail;
    }

    /* Listening */
    if (listen(fd, 15) < 0) /* connection queue is 15. */
    {
        LOGERR("Server Start Fails. : listen failure");
        goto fail;
    }
    LOGINFO("[START] Now Server listening on port %d, EMdsockfd: %d"
            ,svr_port, fd);
 
    /* notify to qemu that emuld is ready */
    emuld_ready();

    if (!epoll_ctl_add(fd))
    {
        LOGERR("Epoll control fails.");
        goto fail;
    }

    *ret_fd = fd;

    return true;
fail:
    close(fd);
    return false;
}
/*------------------------------- end of function init_server0 */

void emuld_ready()
{
    char buf[16];

    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    int port;
    char *ptr;
    char *temp_sdbport;
    temp_sdbport = getenv("sdb_port");
    if(temp_sdbport == NULL) {
        LOGERR("failed to get env variable from sdb_port");
        return;
    }

    port = strtol(temp_sdbport, &ptr, 10);
    port = port + 3;

    LOGINFO("guest_server port: %d", port);

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
        LOGERR("socket error!");
        return;
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    if (inet_aton(SRV_IP, &si_other.sin_addr)==0) {
        LOGERR("inet_aton() failed");
    }

    memset(buf, '\0', sizeof(buf));

    sprintf(buf, "5\n");

    LOGINFO("send message 5\\n to guest server");

    while(sendto(s, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, slen) == -1)
    {
        LOGERR("sendto error! retry sendto");
        usleep(1000);
    }
    LOGINFO("emuld is ready.");

    close(s);

}

void* init_vm_connect(void* data)
{
    struct sockaddr_in vm_addr;
    int ret = -1;

    set_vm_connect_status(0);

    LOGINFO("init_vm_connect start");

    pthread_detach(pthread_self());
    /* Open TCP Socket */
    if ((g_fd[fdtype_vmodem] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOGERR("Server Start Fails. : Can't open stream socket.");
        exit(0);
    }

    /* Address Setting */
    memset( &vm_addr , 0 , sizeof(vm_addr));

    vm_addr.sin_family = AF_INET;
    vm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    vm_addr.sin_port = htons(vmodem_port);

    while (ret < 0 && !exit_flag)
    {
        ret = connect(g_fd[fdtype_vmodem], (struct sockaddr *)&vm_addr, sizeof(vm_addr));

        LOGDEBUG("vm_sockfd: %d, connect ret: %d", g_fd[fdtype_vmodem], ret);

        if(ret < 0) {
            LOGERR("connection failed to vmodem! try.");
            sleep(1);
        }
    }

    epoll_ctl_add(g_fd[fdtype_vmodem]);

    set_vm_connect_status(1);

    pthread_exit((void *) 0);
}

bool epoll_ctl_add(const int fd)
{
    struct epoll_event events;

    events.events = EPOLLIN;    // check In event
    events.data.fd = fd;

    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &events) < 0 )
    {
        LOGERR("Epoll control fails.");
        return false;
    }

    LOGINFO("[START] epoll events add fd success for server");
    return true;
}

bool epoll_init(void)
{
    g_epoll_fd = epoll_create(MAX_EVENTS); // create event pool
    if(g_epoll_fd < 0)
    {
        LOGERR("Epoll create Fails.");
        return false;
    }

    LOGINFO("[START] epoll creation success");
    return true;
}



/*------------------------------- end of function epoll_init */


int parse_val(char *buff, unsigned char data, char *parsbuf)
{
    int count=0;
    while(1)
    {
        if(count > 40)
            return -1;
        if(buff[count] == data)
        {
            count++;
            strncpy(parsbuf, buff, count);
            return count;
        }
        count++;
    }

    return 0;
}

int recv_data(int event_fd, char** r_databuf, int size)
{
    int recvd_size = 0;
    int len = 0;
    int getcnt = 0;
    char* r_tmpbuf = NULL;
    const int alloc_size = sizeof(char) * size + 1;

    r_tmpbuf = (char*)malloc(alloc_size);
    if(r_tmpbuf == NULL)
    {
        return -1;
    }

    char* databuf = (char*)malloc(alloc_size);
    if(databuf == NULL)
    {
        free(r_tmpbuf);
        *r_databuf = NULL;
        return -1;
    }

    memset(databuf, '\0', alloc_size);

    while(recvd_size < size)
    {
        memset(r_tmpbuf, '\0', alloc_size);
        len = recv(event_fd, r_tmpbuf, size - recvd_size, 0);
        if (len < 0) {
            break;
        }

        memcpy(databuf + recvd_size, r_tmpbuf, len);
        recvd_size += len;
        getcnt++;
        if(getcnt > MAX_GETCNT) {
            break;
        }
    }
    free(r_tmpbuf);
    r_tmpbuf = NULL;

    *r_databuf = databuf;

    return recvd_size;
}

int read_header(int fd, LXT_MESSAGE* packet)
{
    char* readbuf = NULL;
    int readed = recv_data(fd, &readbuf, HEADER_SIZE);
    if (readed <= 0)
        return 0;
    memcpy((void*) packet, (void*) readbuf, HEADER_SIZE);

    if (readbuf)
    {
        free(readbuf);
        readbuf = NULL;
    }
    return readed;
}


bool read_ijcmd(const int fd, ijcommand* ijcmd)
{
    int readed;
    readed = read_header(fd, &ijcmd->msg);

    LOGDEBUG("action: %d", ijcmd->msg.action);
    LOGDEBUG("length: %d", ijcmd->msg.length);

    if (readed <= 0)
        return false;

    // TODO : this code should removed, for telephony
    if (ijcmd->msg.length == 0)
    {
        if (ijcmd->msg.action == 71)    // that's strange packet from telephony initialize
        {
            ijcmd->msg.length = 4;
        }
    }

    if (ijcmd->msg.length <= 0)
        return true;

    if (ijcmd->msg.length > 0)
    {
        readed = recv_data(fd, &ijcmd->data, ijcmd->msg.length);
        if (readed <= 0)
        {
            free(ijcmd->data);
            ijcmd->data = NULL;
            return false;
        }

    }
    return true;
}

bool read_id(const int fd, ijcommand* ijcmd)
{
    char* readbuf = NULL;
    int readed = recv_data(fd, &readbuf, ID_SIZE);

    LOGDEBUG("read_id : receive size: %d", readed);

    if (readed <= 0)
    {
        free(readbuf);
        readbuf = NULL;
        return false;
    }

    LOGDEBUG("identifier: %s", readbuf);

    memset(ijcmd->cmd, '\0', sizeof(ijcmd->cmd));
    int parselen = parse_val(readbuf, 0x0a, ijcmd->cmd);

    LOGDEBUG("parse_len: %d, buf = %s, fd=%d", parselen, ijcmd->cmd, fd);

    if (readbuf)
    {
        free(readbuf);
        readbuf = NULL;
    }

    return true;
}


void recv_from_vmodem(int fd)
{
    LOGDEBUG("recv_from_vmodem");

    ijcommand ijcmd;
    if (!read_ijcmd(fd, &ijcmd))
    {
        LOGINFO("fail to read ijcmd");

        set_vm_connect_status(0);

        close(fd);

        if (pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
        {
            LOGERR("pthread create fail!");
        }
        return;
    }

    LOGDEBUG("vmodem data length: %d", ijcmd.msg.length);
    const int tmplen = HEADER_SIZE + ijcmd.msg.length;
    char* tmp = (char*) malloc(tmplen);

    if (tmp)
    {
        memcpy(tmp, &ijcmd.msg, HEADER_SIZE);
        if (ijcmd.msg.length > 0)
            memcpy(tmp + HEADER_SIZE, ijcmd.data, ijcmd.msg.length);

        if(!ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_TELEPHONY, (const char*) tmp, tmplen)) {
            LOGERR("msg_send_to_evdi: failed");
        }

        free(tmp);
    }

    // send header to ij
    if (is_ij_exist())
    {
        send_to_all_ij((char*) &ijcmd.msg, HEADER_SIZE);

        if (ijcmd.msg.length > 0)
        {
            send_to_all_ij((char*) ijcmd.data, ijcmd.msg.length);
        }
    }
}


void recv_from_ij(int fd)
{
    LOGDEBUG("recv_from_ij");

    ijcommand ijcmd;

    if (!read_id(fd, &ijcmd))
    {
        close_cli(fd);
        return;
    }

    // TODO : if recv 0 then close client

    if (!read_ijcmd(fd, &ijcmd))
    {
        LOGERR("fail to read ijcmd");
        return;
    }


    if (strncmp(ijcmd.cmd, "telephony", 9) == 0)
    {
        msgproc_telephony(fd, &ijcmd, false);
    }
    else if (strncmp(ijcmd.cmd, "sensor", 6) == 0)
    {
        msgproc_sensor(fd, &ijcmd, false);
    }
    else if (strncmp(ijcmd.cmd, "location", 8) == 0)
    {
        msgproc_location(fd, &ijcmd, false);
    }
    else if (strncmp(ijcmd.cmd, "nfc", 3) == 0)
    {
        msgproc_nfc(fd, &ijcmd, false);
    }
    else if (strncmp(ijcmd.cmd, "system", 6) == 0)
    {
        msgproc_system(fd, &ijcmd, false);
    }
    else if (strncmp(ijcmd.cmd, "sdcard", 6) == 0)
    {
        msgproc_sdcard(fd, &ijcmd, false);
    }
    else
    {
        LOGERR("Unknown packet: %s", ijcmd.cmd);
        close_cli (fd);
    }
}

bool accept_proc(const int server_fd)
{
    struct sockaddr_in cli_addr;
    int cli_sockfd;
    int cli_len = sizeof(cli_addr);

    cli_sockfd = accept(server_fd, (struct sockaddr *)&cli_addr,(socklen_t *)&cli_len);
    if(cli_sockfd < 0)
    {
        LOGERR("accept error");
        return false;
    }
    else
    {
        LOGINFO("[Accept] New client connected. fd:%d, port:%d"
                ,cli_sockfd, cli_addr.sin_port);

        clipool_add(cli_sockfd, cli_addr.sin_port, fdtype_ij);
        epoll_ctl_add(cli_sockfd);
    }
    return true;
}

static void msgproc_suspend(int fd, ijcommand* ijcmd, bool evdi)
{
    if (ijcmd->msg.action == SUSPEND_LOCK) {
        set_lock_state(SUSPEND_LOCK);
    } else {
        set_lock_state(SUSPEND_UNLOCK);
    }

	LOGINFO("[Suspend] Set lock state as %d (1: lock, other: unlock)", ijcmd->msg.action);
}

static void send_default_suspend_req(void)
{
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    if(packet == NULL){
        return;
    }
    memset(packet, 0, sizeof(LXT_MESSAGE));

    packet->length = 0;
    packet->group = 5;
    packet->action = 15;

	int tmplen = HEADER_SIZE;
	char* tmp = (char*) malloc(tmplen);
	if (!tmp)
		return;

    memcpy(tmp, packet, HEADER_SIZE);

    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SUSPEND, (const char*) tmp, tmplen);


	if (tmp)
	    free(tmp);
	if (packet)
		free(packet);
}

static synbuf g_synbuf;

void process_evdi_command(ijcommand* ijcmd)
{
    int fd = -1;

    if (strncmp(ijcmd->cmd, "telephony", 9) == 0)
    {
        msgproc_telephony(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "sensor", 6) == 0)
    {
        msgproc_sensor(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "location", 8) == 0)
    {
        msgproc_location(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "nfc", 3) == 0)
    {
        msgproc_nfc(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "system", 6) == 0)
    {
        msgproc_system(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "sdcard", 6) == 0)
    {
        msgproc_sdcard(fd, ijcmd, true);
    }
    else if (strncmp(ijcmd->cmd, "suspend", 7) == 0)
    {
        msgproc_suspend(fd, ijcmd, true);
    }
    else
    {
        LOGERR("Unknown packet: %s", ijcmd->cmd);
    }
}

void recv_from_evdi(evdi_fd fd)
{
    LOGDEBUG("recv_from_evdi");
    int readed;

    struct msg_info _msg;
    int to_read = sizeof(struct msg_info);

    memset(&_msg, 0x00, sizeof(struct msg_info));


    while (1)
    {
        readed = read(fd, &_msg, to_read);
        if (readed == -1) // TODO : error handle
        {
            if (errno != EAGAIN)
            {
                LOGERR("EAGAIN");
                return;
            }
        }
        else
        {
            break;
        }
    }

    LOGDEBUG("total readed  = %d, read count = %d, index = %d, use = %d, msg = %s",
            readed, _msg.count, _msg.index, _msg.use, _msg.buf);

    g_synbuf.reset_buf();
    g_synbuf.write(_msg.buf, _msg.use);


    ijcommand ijcmd;
    readed = g_synbuf.read(ijcmd.cmd, ID_SIZE);

    LOGDEBUG("ij id : %s", ijcmd.cmd);

    // TODO : check
    if (readed < ID_SIZE)
        return;

    // read header
    readed = g_synbuf.read((char*)&ijcmd.msg, HEADER_SIZE);
    if (readed < HEADER_SIZE)
        return;

    int act = ijcmd.msg.action;
    int grp = ijcmd.msg.group;
    int len = ijcmd.msg.length;


    LOGDEBUG("HEADER : action = %d, group = %d, length = %d", act, grp, len);

    if (ijcmd.msg.length > 0)
    {
        ijcmd.data = (char*) malloc(ijcmd.msg.length);
        if (!ijcmd.data)
        {
            LOGERR("failed to allocate memory");
            return;
        }
        readed = g_synbuf.read(ijcmd.data, ijcmd.msg.length);

        LOGDEBUG("DATA : %s", ijcmd.data);

        if (readed < ijcmd.msg.length)
        {
            LOGERR("received data is insufficient");
            //return;
        }
    }

    process_evdi_command(&ijcmd);
}

bool server_process(void)
{
    int i,nfds;
    int fd_tmp;

    nfds = epoll_wait(g_epoll_fd, g_events, MAX_EVENTS, 100);

    if (nfds == -1 && errno != EAGAIN && errno != EINTR)
    {
        LOGERR("epoll wait(%d)", errno);
        return true;
    }

    for( i = 0 ; i < nfds ; i++ )
    {
        fd_tmp = g_events[i].data.fd;
        if (fd_tmp == g_fd[fdtype_server])
        {
            accept_proc(fd_tmp);
        }
        else if (fd_tmp == g_fd[fdtype_device])
        {
            recv_from_evdi(fd_tmp);
        }
        else if(fd_tmp == g_fd[fdtype_vmodem])
        {
            recv_from_vmodem(fd_tmp);
        }
        else
        {
            recv_from_ij(fd_tmp);
        }
    }

    return false;
}
/*------------------------------- end of function server_process */

void end_server(int sig)
{
    close(g_fd[fdtype_server]); /* close server socket */
    LOGINFO("[SHUTDOWN] Server closed by signal %d",sig);

    exit(0);
}

int main( int argc , char *argv[])
{
    int state;

    LOGINFO("emuld start");
    /* entry , argument check and process */
    if(argc < 3){
        g_svr_port = DEFAULT_PORT;
    }else {
        if(strcmp("-port", argv[1]) ==  0 ) {
            g_svr_port = atoi(argv[2]);
            if(g_svr_port < 1024) {
                LOGERR("[STOP] port number invalid : %d",g_svr_port);
                exit(0);
            }
        }
    }

    init_data0();

    if (!epoll_init())
    {
        exit(0);
    }

    if (!init_server0(g_svr_port, &g_fd[fdtype_server]))
    {
        close(g_epoll_fd);
        exit(0);
    }

    if (!init_device(&g_fd[fdtype_device]))
    {
        close(g_epoll_fd);
        exit(0);
    }

    LOGINFO("[START] epoll events set success for server");

    set_vm_connect_status(0);

    if(pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
    {
        LOGERR("pthread create fail!");
        close(g_epoll_fd);
        exit(0);
    }

    send_default_suspend_req();

    bool is_exit = false;

    while(!is_exit)
    {
        is_exit = server_process();
    }

    exit_flag = true;

    if (!is_vm_connected())
    {
        int status;
        pthread_join(tid[0], (void **)&status);
        LOGINFO("vmodem thread end %d", status);
    }

    state = pthread_mutex_destroy(&mutex_vmconnect);
    if (state != 0)
    {
        LOGERR("mutex_vmconnect is failed to destroy.");
    }

    stop_listen();

    LOGINFO("emuld exit");

    return 0;
}

