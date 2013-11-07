

#ifndef __evdi_h__
#define __evdi_h__

typedef int		evdi_fd;

bool init_device(evdi_fd* ret_fd);

evdi_fd open_device(void);

bool ijmsg_send_to_evdi(evdi_fd fd, const char* cat, const char* data, const int len);
bool send_to_evdi(evdi_fd fd, const char* msg, const int len);
bool msg_send_to_evdi(evdi_fd fd, const char* data, const int len);



#endif
