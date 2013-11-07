/*
 * synbuf.h
 *
 *  Created on: 2013. 4. 12.
 *      Author: dykim
 */

#ifndef SYNBUF_H_
#define SYNBUF_H_

#include <stdbool.h>


class synbuf
{
public:

	enum
	{
		default_buf_size = 2048
	};
	synbuf()
		: m_buf(NULL), m_size(default_buf_size), m_use(0)
	{
		m_buf = (char*) malloc(default_buf_size);
		memset(m_buf, 0, default_buf_size);
		m_readptr = m_buf;
	}

	void reset_buf()
	{
		freebuf();

		m_buf = (char*) malloc(default_buf_size);
		memset(m_buf, 0, default_buf_size);
		m_readptr = m_buf;

		m_size = default_buf_size;
		m_use = 0;
	}

	int available()
	{
		return m_size - m_use;
	}

	char* get_readptr()
	{
		return m_readptr;
	}

	void set_written(const int written)
	{
		m_use += written;
	}

	void freebuf()
	{
		if (m_buf)
		{
			free(m_buf);
			m_buf = NULL;
		}
	}

	bool realloc_and_move(const int newsize, const int readed)
	{
		char* tmp = (char*) malloc(newsize);
		if (!tmp)
			return false;

		int left = m_use - readed;
		memset(tmp, 0, newsize);
		memcpy(tmp, m_buf + readed, left);

		freebuf();

		m_buf = tmp;
		m_use = left;
		m_size = newsize;
		m_readptr = m_buf;

		return true;
	}

	bool write(const char* buf, const int len)
	{
		if (len >= available())
		{
			if (!realloc_and_move((m_size * 2), 0))
				return false;
		}

		memcpy(m_buf + m_use, buf, len);
		m_use += len;

		return true;
	}

	int read(char* buf, const int len)
	{
		if (m_use < len)
			return 0;

		memcpy(buf, m_buf, len);

		int left = m_use - len;
		if (left > 0)
		{
			realloc_and_move(m_size, len);
		}
		else
		{
			// there is no more readable buffer, reset all variables
			memset(m_buf, 0, m_size);
			m_readptr = m_buf;
			m_use = 0;
		}

		return len;
	}


private:


	char* m_buf;
	char* m_readptr;
	int m_size;
	int m_use;
};



#endif /* SYNBUF_H_ */
