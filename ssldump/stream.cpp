#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream.h"

MemoryStream::MemoryStream(const void *buf, int buflen)
	: MyStream()
{
	m_buf = (char *)buf;
	m_buflen = buflen;
}

MemoryStream::~MemoryStream()
{
}

bool
MemoryStream::Eof()
{
	return (m_position >= m_buflen);
}

int
MemoryStream::Read(void *dst, int dstsize)
{
	int n;

	n = dstsize;
	if (dstsize > m_buflen - m_position) {
		dstsize = m_buflen - m_position;
	}

	memcpy(dst, m_buf + m_position, n);
	m_position += n;

	return n;
}

int
MemoryStream::GetCh()
{
	if (Eof()) {
		return -1;
	}

	return m_buf[m_position++];
}

MemoryStream *
MemoryStream::ReadSlice(int len)
{
	if (GetPosition() + len >= m_buflen) {
		return NULL;
	}

	MemoryStream *ms = new MemoryStream(m_buf + GetPosition(), len);
	m_position += len;

	return ms;
}
