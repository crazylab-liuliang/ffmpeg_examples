#include "qringbuffer.h"
/**
* @brief QRingBuffer::QRingBuffer
* @param buffersize Byte
*/
QRingBuffer::QRingBuffer(int size)
{
	bufferSize = size;
	rbCapacity = size;
	rbBuff = rbBuf;
	rbHead = rbBuff;
	rbTail = rbBuff;
}

QRingBuffer::~QRingBuffer()
{
	rbBuff = nullptr;
	rbHead = nullptr;
	rbTail = nullptr;
	rbCapacity = 0;
	delete[]rbBuf; //�ͷŻ�����
}
/**
* @brief QRingBuffer::rbCanRead
* @return �������ɶ��ֽ���
*/
int QRingBuffer::canRead()
{
	//ring buufer is null, return -1
	if ((nullptr == rbBuff) || (nullptr == rbHead) || (nullptr == rbTail))
	{
		return -1;
	}

	if (rbHead == rbTail)
	{
		return 0;
	}

	if (rbHead < rbTail)
	{
		return rbTail - rbHead;
	}
	return rbCapacity - (rbHead - rbTail);
}

/**
* @brief QRingBuffer::rbCanWrite  ������ʣ���д�ֽ���
* @return  ��д�ֽ���
*/
int QRingBuffer::canWrite()
{
	if ((nullptr == rbBuff) || (nullptr == rbHead) || (nullptr == rbTail))
	{
		return -1;
	}

	return rbCapacity - canRead();
}

/**
* @brief QRingBuffer::read �ӻ�����������
* @param Ŀ�������ַ
* @param �����ֽ���
* @return
*/
int QRingBuffer::read(void *data, int count)
{
	int copySz = 0;

	if ((nullptr == rbBuff) || (nullptr == rbHead) || (nullptr == rbTail))
	{
		return -1;
	}
	if (nullptr == data)
	{
		return -1;
	}

	if (rbHead < rbTail)
	{
		copySz = min(count, canRead());
		memcpy(data, rbHead, copySz);
		rbHead += copySz;
		return copySz;
	}
	else
	{
		if (count < rbCapacity - (rbHead - rbBuff))
		{
			copySz = count;
			memcpy(data, rbHead, copySz);
			rbHead += copySz;
			return copySz;
		}
		else
		{
			copySz = rbCapacity - (rbHead - rbBuff);
			memcpy(data, rbHead, copySz);
			rbHead = rbBuff;
			copySz += read((unsigned char *)data + copySz, count - copySz);
			return copySz;
		}
	}
}

/**
* @brief QRingBuffer::write
* @param ���ݵ�ַ
* @param Ҫд���ֽ���
* @return д����ֽ���
*/
int QRingBuffer::write(const void *data, int count)
{
	int tailAvailSz = 0;

	if ((nullptr == rbBuff) || (nullptr == rbHead) || (nullptr == rbTail))
	{
		return -1;
	}

	if (nullptr == data)
	{
		return -1;
	}

	if (count >= canWrite())
	{
		return -1;
	}

	if (rbHead <= rbTail)
	{
		tailAvailSz = rbCapacity - (rbTail - rbBuff);
		if (count <= tailAvailSz)
		{
			memcpy(rbTail, data, count);
			rbTail += count;
			if (rbTail == rbBuff + rbCapacity)
			{
				rbTail = rbBuff;
			}
			return count;
		}
		else
		{
			memcpy(rbTail, data, tailAvailSz);
			rbTail = rbBuff;

			return tailAvailSz + write((char*)data + tailAvailSz, count - tailAvailSz);
		}
	}
	else
	{
		memcpy(rbTail, data, count);
		rbTail += count;

		return count;
	}
}

/**
* @brief QRingBuffer::size
* @return ��������С
*/
int QRingBuffer::size()
{
	return bufferSize;
}