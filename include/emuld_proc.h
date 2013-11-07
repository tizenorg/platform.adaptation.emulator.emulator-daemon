
#ifndef __emuld_proc_h__
#define __emuld_proc_h__


int is_mounted(void);
void* mount_sdcard(void* data);
int umount_sdcard(const int fd);

void send_guest_server(char* databuf);

#endif
