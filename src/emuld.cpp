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
unsigned short sap_port = SAP_PORT;
unsigned short sensord_port = SENSORD_PORT;

/* global server port number */
int g_svr_port;

static int g_vm_connect_status; /* connection status between emuld and vmodem  */
static int g_sap_connect_status;/* connection status between emuld and sap daemon  */

pthread_t tid[MAX_CLIENT + 1];

/* udp socket */
struct sockaddr_in si_sensord_other;

int g_fd[fdtype_max];

typedef std::queue<msg_info*> __msg_queue;

__msg_queue g_msgqueue;

int g_epoll_fd;

static pthread_mutex_t mutex_vmconnect = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_sapconnect = PTHREAD_MUTEX_INITIALIZER;

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
        LOG("system call failure(command = %s)\n", param);
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

bool is_sap_connected(void)
{
    _auto_mutex _(&mutex_sapconnect);

    if (g_sap_connect_status != 1)
        return false;

    return true;
}

void set_sap_connect_status(const int v)
{
    _auto_mutex _(&mutex_sapconnect);

    g_sap_connect_status = v;
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
        LOG("Server Start Fails. : Can't open stream socket \n");
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
        LOG("Server Start Fails. : Can't set reuse address\n");
        goto fail;
    }

    /* Bind Socket */
    if (bind(fd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        LOG("Server Start Fails. : Can't bind local address\n");
        goto fail;
    }

    /* Listening */
    if (listen(fd, 15) < 0) /* connection queue is 15. */
    {
        LOG("Server Start Fails. : listen failure\n");
        goto fail;
    }
    LOG("[START] Now Server listening on port %d, EMdsockfd: %d"
            ,svr_port, fd);
 
    /* notify to qemu that emuld is ready */
    emuld_ready();

    if (!epoll_ctl_add(fd))
    {
        LOG("Epoll control fails.\n");
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
        LOG("failed to get env variable from sdb_port\n");
        return;
    }

    port = strtol(temp_sdbport, &ptr, 10);
    port = port + 3;

    LOG("guest_server port: %d\n", port);

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
        LOG("socket error!\n");
        return;
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    if (inet_aton(SRV_IP, &si_other.sin_addr)==0) {
        LOG("inet_aton() failed\n");
    }

    memset(buf, '\0', sizeof(buf));

    sprintf(buf, "5\n");

    LOG("send message to guest server\n");

    while(sendto(s, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, slen) == -1)
    {
        LOG("sendto error! retry sendto\n");
        usleep(1000);
    }
    LOG("emuld is ready.\n");

    close(s);

}

void* init_vm_connect(void* data)
{
    struct sockaddr_in vm_addr;
    int ret = -1;

    set_vm_connect_status(0);

    LOG("init_vm_connect start\n");

    pthread_detach(pthread_self());
    /* Open TCP Socket */
    if ((g_fd[fdtype_vmodem] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOG("Server Start Fails. : Can't open stream socket \n");
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

        LOG("vm_sockfd: %d, connect ret: %d\n", g_fd[fdtype_vmodem], ret);

        if(ret < 0) {
            LOG("connection failed to vmodem! try \n");
            sleep(1);
        }
    }

    epoll_ctl_add(g_fd[fdtype_vmodem]);

    set_vm_connect_status(1);

    pthread_exit((void *) 0);
}

void* init_sap_connect(void* data)
{
    struct sockaddr_in sap_addr;
    int ret = -1;

    set_sap_connect_status(0);

    LOG("init_sap_connect start\n");

    pthread_detach(pthread_self());
    /* Open TCP Socket */
    if ((g_fd[fdtype_sap] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        LOG("Server Start Fails. : Can't open stream socket \n");
        exit(0);
    }

    /* Address Setting */
    memset( &sap_addr , 0 , sizeof(sap_addr)) ;

    sap_addr.sin_family = AF_INET;
    sap_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sap_addr.sin_port = htons(sap_port);

    while (ret < 0 && !exit_flag)
    {
        ret = connect(g_fd[fdtype_sap], (struct sockaddr *)&sap_addr, sizeof(sap_addr));

        LOG("sap_sockfd: %d, connect ret: %d\n", g_fd[fdtype_sap], ret);

        if(ret < 0) {
            LOG("connection failed to sap! try \n");
            sleep(1);
        }
    }

    epoll_ctl_add(g_fd[fdtype_sap]);

    set_sap_connect_status(1);

    pthread_exit((void *) 0);
}

bool epoll_ctl_add(const int fd)
{
    struct epoll_event events;

    events.events = EPOLLIN;    // check In event
    events.data.fd = fd;

    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &events) < 0 )
    {
        LOG("Epoll control fails.\n");
        return false;
    }

    printf("[START] epoll events set success for server\n");
    return true;
}

bool epoll_init(void)
{
    g_epoll_fd = epoll_create(MAX_EVENTS); // create event pool
    if(g_epoll_fd < 0)
    {
        LOG("Epoll create Fails.\n");
        return false;
    }

    LOG("[START] epoll creation success\n");
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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

void udp_init(void)
{
    char emul_ip[HOST_NAME_MAX+1];
    struct addrinfo *res;
    struct addrinfo hints;
    int rc;

    LOG("start");

    memset(emul_ip, 0, sizeof(emul_ip));
    if (gethostname(emul_ip, sizeof(emul_ip)) < 0)
    {
        LOG("gethostname(): %s", strerror(errno));
        assert(0);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if ((rc=getaddrinfo(emul_ip, STR(SENSORD_PORT), &hints, &res)) != 0)
    {
        if (rc == EAI_SYSTEM)
            LOG("getaddrinfo(sensord): %s", strerror(errno));
        else
            LOG("getaddrinfo(sensord): %s", gai_strerror(rc));
        assert(0);
    }

    if ((g_fd[fdtype_sensor] = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        LOG("socket error!\n");
    }

    if (res->ai_addrlen > sizeof(si_sensord_other))
    {
        LOG("sockaddr structure too big");
        /* XXX: if you `return' remember to clean up */
        assert(0);
    }
    memset((char *) &si_sensord_other, 0, sizeof(si_sensord_other));
    memcpy((char *) &si_sensord_other, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
}

#undef STR_HELPER
#undef STR

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

    LOG("action: %d", ijcmd->msg.action);
    LOG("length: %d", ijcmd->msg.length);

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

    LOG("read_id : receive size: %d", readed);

    if (readed <= 0)
    {
        free(readbuf);
        readbuf = NULL;
        return false;
    }

    LOG("identifier: %s", readbuf);

    memset(ijcmd->cmd, '\0', sizeof(ijcmd->cmd));
    int parselen = parse_val(readbuf, 0x0a, ijcmd->cmd);

    LOG("parse_len: %d, buf = %s, fd=%d", parselen, ijcmd->cmd, fd);

    if (readbuf)
    {
        free(readbuf);
        readbuf = NULL;
    }

    return true;
}


void recv_from_vmodem(int fd)
{
    printf("recv_from_vmodem\n");

    ijcommand ijcmd;
    if (!read_ijcmd(fd, &ijcmd))
    {
        LOG("fail to read ijcmd\n");

        set_vm_connect_status(0);

        close(fd);

        if (pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
        {
            LOG("pthread create fail!");
        }
        return;
    }

    LOG("vmodem data length: %d", ijcmd.msg.length);
    const int tmplen = HEADER_SIZE + ijcmd.msg.length;
    char* tmp = (char*) malloc(tmplen);

    if (tmp)
    {
        memcpy(tmp, &ijcmd.msg, HEADER_SIZE);
        if (ijcmd.msg.length > 0)
            memcpy(tmp + HEADER_SIZE, ijcmd.data, ijcmd.msg.length);

        if(!ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_TELEPHONY, (const char*) tmp, tmplen)) {
            LOG("msg_send_to_evdi: failed\n");
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

void recv_from_sap(int fd)
{
    printf("recv_from_sap\n");

    ijcommand ijcmd;
    if (!read_ijcmd(fd, &ijcmd))
    {
        LOG("fail to read ijcmd\n");

        set_sap_connect_status(0);

        close(fd);

        if (pthread_create(&tid[3], NULL, init_sap_connect, NULL) != 0)
        {
            LOG("pthread create fail!");
        }
        return;
    }

    LOG("sap data length: %d", ijcmd.msg.length);
    const int tmplen = HEADER_SIZE + ijcmd.msg.length;
    char* tmp = (char*) malloc(tmplen);

    if (tmp)
    {
        memcpy(tmp, &ijcmd.msg, HEADER_SIZE);
        if (ijcmd.msg.length > 0)
            memcpy(tmp + HEADER_SIZE, ijcmd.data, ijcmd.msg.length);

        if(!ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SAP, (const char*) tmp, tmplen)) {
            LOG("msg_send_to_evdi: failed\n");
        }

        free(tmp);
    }
}

void recv_from_ij(int fd)
{
    printf("recv_from_ij\n");

    ijcommand ijcmd;

    if (!read_id(fd, &ijcmd))
    {
        close_cli(fd);
        return;
    }

    // TODO : if recv 0 then close client

    if (!read_ijcmd(fd, &ijcmd))
    {
        LOG("fail to read ijcmd\n");
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
    else if (strncmp(ijcmd.cmd, "sap", 3) == 0)
    {
        msgproc_sap(fd, &ijcmd, false);
    }
    else
    {
        LOG("Unknown packet: %s", ijcmd.cmd);
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
        LOG("accept error\n");
        return false;
    }
    else
    {
        LOG("[Accpet] New client connected. fd:%d, port:%d"
                ,cli_sockfd, cli_addr.sin_port);

        clipool_add(cli_sockfd, cli_addr.sin_port, fdtype_ij);
        epoll_ctl_add(cli_sockfd);
    }
    return true;
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
    else
    {
        LOG("Unknown packet: %s", ijcmd->cmd);
    }
}

//static long recv_count = 0;

void recv_from_evdi(evdi_fd fd)
{
    printf("recv_from_evdi\n");
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
                perror ("recv_from_evdi : EAGAIN\n");
                LOG("EAGAIN\n");
                return;
            }
        }
        else
        {
            break;
        }
    }

    //LOG("RECV COUNT = %d\n", ++recv_count);
    LOG("total readed  = %d, read count = %d, index = %d, use = %d, msg = %s\n",
            readed, _msg.count, _msg.index, _msg.use, _msg.buf);

    g_synbuf.reset_buf();
    g_synbuf.write(_msg.buf, _msg.use);


    ijcommand ijcmd;
    readed = g_synbuf.read(ijcmd.cmd, ID_SIZE);

    LOG("ij id : %s\n", ijcmd.cmd);

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


    LOG("HEADER : action = %d, group = %d, length = %d\n", act, grp, len);

    if (ijcmd.msg.length > 0)
    {
        ijcmd.data = (char*) malloc(ijcmd.msg.length);
        if (!ijcmd.data)
        {
            LOG("failed to allocate memory\n");
            return;
        }
        readed = g_synbuf.read(ijcmd.data, ijcmd.msg.length);

        LOG("DATA : %s\n", ijcmd.data);

        if (readed < ijcmd.msg.length)
        {
            LOG("received data is insufficient");
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
        LOG("epoll wait(%d)\n", errno);
        return true;
    }

    for( i = 0 ; i < nfds ; i++ )
    {
        fd_tmp = g_events[i].data.fd;
        if (fd_tmp == g_fd[fdtype_server])
        {
            accept_proc(fd_tmp);
        }
        else if (fd_tmp == g_fd[fdtype_sap])
        {
            recv_from_sap(fd_tmp);
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
    close(g_fd[fdtype_sensor]);
    LOG("[SHUTDOWN] Server closed by signal %d",sig);

    exit(0);
}

void set_lock_state() {
    int i = 0;
    int ret = 0;
    // Now we blocking to enter "SLEEP".
    while(i < PMAPI_RETRY_COUNT ) {
        ret = pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
        LOG("pm_lock_state() return: %d", ret);
        if(ret == 0)
        {
            break;
        }
        ++i;
        sleep(10);
    }
    if (i == PMAPI_RETRY_COUNT) {
        LOG("Emulator Daemon: Failed to call pm_lock_state().\n");
    }
}

int main( int argc , char *argv[])
{
    int vm_state;
    int sap_state;

    //if(log_print == 1)
    {
        // for emuld log file
        systemcall("rm /var/log/emuld.log");
        systemcall("touch /var/log/emuld.log");
    }

    LOG("start");
    /* entry , argument check and process */
    if(argc < 3){
        g_svr_port = DEFAULT_PORT;
    }else {
        if(strcmp("-port", argv[1]) ==  0 ) {
            g_svr_port = atoi(argv[2]);
            if(g_svr_port < 1024) {
                LOG("[STOP] port number invalid : %d\n",g_svr_port);
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

    LOG("[START] epoll events set success for server");

    set_vm_connect_status(0);

    if(pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
    {
        LOG("pthread create fail!");
        close(g_epoll_fd);
        exit(0);
    }

    if(pthread_create(&tid[3], NULL, init_sap_connect, NULL) != 0)
    {
        LOG("pthread create fail!");
        close(g_epoll_fd);
        exit(0);
    }
    udp_init();

    set_lock_state();

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
        LOG("vmodem thread end %d\n", status);
    }

    vm_state = pthread_mutex_destroy(&mutex_vmconnect);
    if (vm_state != 0)
    {
        LOG("mutex_vmconnect is failed to destroy.");
    }

    if (!is_sap_connected())
    {
        int status;
        pthread_join(tid[3], (void **)&status);
        LOG("sap thread end %d\n", status);
    }

    sap_state = pthread_mutex_destroy(&mutex_sapconnect);
    if (sap_state != 0)
    {
        LOG("mutex_vmconnect is failed to destroy.");
    }

    stop_listen();

    LOG("emuld exit\n");

    return 0;
}

