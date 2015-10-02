#ifndef ssldump_stream_h
#define ssldump_stream_h

class MyStream
{
 public:
	MyStream() {
		m_position = 0;
	}

	virtual ~MyStream() {}

	virtual int GetPosition() const { return m_position; }

	virtual bool Eof() = 0;

	virtual int Read(void *buf, int bufsize) = 0;

	virtual int GetCh() = 0;

 protected:
	int m_position;

};

class MemoryStream : public MyStream
{
 public:
	MemoryStream(const void *buf, int buflen);

	virtual ~MemoryStream();

	bool Eof();

	int Read(void *buf, int bufsize);

	int GetCh();

	MemoryStream *ReadSlice(int len);

 protected:
	const char *m_buf;

	int m_buflen;

};

#endif	// !ssldump_stream_h
