#ifndef QRINGBUFFER_H
#define QRINGBUFFER_H

#include <cstring>

#ifndef RB_MAX_LEN
#define RB_MAX_LEN 2048
#endif

#define min(a, b) (a)<(b)?(a):(b)   //求最小

class QRingBuffer
{
public:
	QRingBuffer(int size = RB_MAX_LEN);
	~QRingBuffer();

	int canRead();    //how much can read
	int canWrite();   //how much can write
	int read(void *data, int count);  //read data frome ringbuffer
	int write(const void *data, int count);
	int size();

private:
	int bufferSize;       //buffer size
	unsigned char *rbBuf = new unsigned char[bufferSize];
	/*环形缓冲区变量*/
	int rbCapacity; //容量
	unsigned char  *rbHead;
	unsigned char  *rbTail;
	unsigned char  *rbBuff;

};

#endif // QRINGBUFFER_H