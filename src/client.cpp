

#include "emuld.h"
#include "emuld_common.h"

static pthread_mutex_t mutex_climap = PTHREAD_MUTEX_INITIALIZER;


CliMap g_climap;

void clipool_add(int fd, unsigned short port, const int fdtype)
{
	_auto_mutex _(&mutex_climap);

	static CliSN s_id = 0;

	CliSN id = s_id;
	s_id++;
	Cli* cli = new Cli(id, fdtype, fd, port);
	if (!cli)
		return;

	if (!g_climap.insert(CliMap::value_type(fd, cli)).second)
		return;

	LOG("clipool_add fd = %d, port = %d, type = %d \n", fd, port, fdtype);
}


void close_cli(int cli_fd)
{
	clipool_delete(cli_fd);
	close(cli_fd);
}

void clipool_delete(int fd)
{
	_auto_mutex _(&mutex_climap);

	CliMap::iterator it = g_climap.find(fd);

	if (it != g_climap.end())
	{
		Cli* cli = it->second;
		g_climap.erase(it);

		if (cli)
		{
			delete cli;
			cli = NULL;
		}
	}

	LOG("clipool_delete fd = %d\n", fd);
}


Cli* find_cli(const int fd)
{
	_auto_mutex _(&mutex_climap);

	CliMap::iterator it = g_climap.find(fd);
	if (it != g_climap.end())
		return NULL;

	Cli* cli = it->second;
	return cli;
}

// for thread safe
bool send_to_cli(const int fd, char* data, const int len)
{
	_auto_mutex _(&mutex_climap);

	CliMap::iterator it = g_climap.find(fd);
	if (it == g_climap.end())
		return false;

	Cli* cli = it->second;

	if (send(cli->sockfd, data, len, 0) == -1)
		return false;

	return true;
}

bool send_to_all_ij(char* data, const int len)
{
	_auto_mutex _(&mutex_climap);

	bool result = false;
	CliMap::iterator it, itend = g_climap.end();

	for (it = g_climap.begin(); it != itend; it++)
	{
		Cli* cli = it->second;

		if (!cli)
			continue;

		int sent = send(cli->sockfd, data, len, 0);
		result = (sent == -1) ? false : true;
		if (sent == -1)
		{
			perror("failed to send to ij\n");
		}

		LOG("send_len: %d, err= %d\n", sent, errno);
	}
	return result;
}

bool is_ij_exist()
{
	_auto_mutex _(&mutex_climap);

	bool result = (g_climap.size() > 0) ? true : false;
	return result;
}

void stop_listen(void)
{
	pthread_mutex_destroy(&mutex_climap);;
}





