#ifndef QRINGBUFFER_H
#define QRINGBUFFER_H

#include <cstring>

#ifndef RB_MAX_LEN
#define RB_MAX_LEN 2048
#endif

#define min(a, b) (a)<(b)?(a):(b)   //����С

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
	/*���λ���������*/
	int rbCapacity; //����
	unsigned char  *rbHead;
	unsigned char  *rbTail;
	unsigned char  *rbBuff;

};

#endif // QRINGBUFFER_H