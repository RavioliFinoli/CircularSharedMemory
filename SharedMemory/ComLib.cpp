#include "ComLib.h"
#define RETURN_UNLOCK_FALSE(x) {ReleaseMutex(x); return false;}
#define RETURN_UNLOCK_TRUE(x) {ReleaseMutex(x); return true;}

#define RETURN_SAFE_FALSE RETURN_UNLOCK_FALSE(hnd_Mutex)
#define RETURN_SAFE_TRUE RETURN_UNLOCK_TRUE(hnd_Mutex)

#define UNLOCK(x) ReleaseMutex(x)

#pragma region "Utility"

bool MessageFits(size_t head, size_t tail, size_t bufferSize, size_t messageSize)
{
	//Check if message fits in remaining memory, between head and EOB
	//and between head and tail.
	if (bufferSize - head < messageSize)
		return false;
	if (tail > head && (tail - head) - 1 < messageSize)
		return false;
	else return true;
}

ComLib::Header CreateHeader(size_t type, size_t length /*incl null-T*/)
{
	//Create and return a header based on passed values
	ComLib::Header header;
	header.msgId = type;
	header.msgLength = length;
	return header;
}

void ComLib::UpdateRBD(size_t value) const
{
	//Check who wants to update; producer and consumer can only update
	//their respective value (head/tail)
	if (type == ComLib::TYPE::PRODUCER)
	{
		CopyMemory((PVOID)ringBufferData.GetBuffer(), &value, sizeof(size_t)); //First slot is head
	}
	else
	{
		CopyMemory((PVOID)(ringBufferData.GetBuffer() + sizeof(size_t)), &value, sizeof(size_t)); //Second slot is tail
	}
}

size_t ComLib::GetTail() const
{
	//Retrieve tail from RBD
	size_t r;
	CopyMemory(&r, (PVOID)(ringBufferData.GetBuffer() + sizeof(size_t)), sizeof(size_t));
	return r;
}

size_t ComLib::GetHead() const
{
	//Retrieve head from RBD
	size_t r;
	CopyMemory(&r, (PVOID)ringBufferData.GetBuffer(), sizeof(size_t));
	return r;
}


bool ComLib::isConnected() const
{
	//Check if buffers are initialized
	return (ringBuffer.GetBuffer() != nullptr && ringBufferData.GetBuffer() != nullptr);
}

#pragma endregion "Utility"

#pragma region "Constructor/Destructor"

ComLib::ComLib(const std::string& secret, const size_t& buffSize, TYPE type)
	: type(type), head(0), tail(0), ringBufferSize(buffSize)
{
	//Convert string to wide string
	std::wstring widestr = std::wstring(secret.begin(), secret.end());

	//Initialize ring buffer
	pRingBuffer = (PVOID)ringBuffer.Init(secret, buffSize);

	//ringBufferData holds space for head and tail (2x size_t)
	//File Map is called RBD (RingBufferData)
	ringBufferData.Init("RBD", sizeof(size_t) * 2);

	//Initialize RBD and Create mutex, if we are the producer
	{
		if (type == PRODUCER)
		{
			hnd_Mutex = CreateMutex(NULL, FALSE, _T("comlibmtx"));
			CopyMemory(ringBufferData.GetBuffer(), &this->head, sizeof(size_t));
			CopyMemory(ringBufferData.GetBuffer() + sizeof(size_t), &tail, sizeof(size_t));
		}
		//If we are not the producer, open the mutex instead
		else
			hnd_Mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, _T("comlibmtx"));
	}
}

ComLib::~ComLib()
{}

#pragma endregion "Constructor/Destructor"

bool ComLib::send(const void * msg, const size_t length)
{
	//length with header
	size_t _length = length + sizeof(ComLib::Header);

	//Acquire mutex
	WaitForSingleObject(hnd_Mutex, INFINITE);

	//Get latest tail
	tail = GetTail();

	//determine if tail is behind head in memory
	bool tailIsBehind = (tail < head);

	//Determine space left until EndOfBuffer
	size_t sizeToEOB = ringBufferSize - head;

	//Determine space left until tail
	size_t sizeToTail;
	tailIsBehind ? sizeToTail = (sizeToEOB + tail - 1) % ringBufferSize : sizeToTail = tail - head - 1;

	//If tail == head, remaining memory is size of buffer minus 1 
	//(head == tail-1 means full, in order to distinguish between empty and full)
	if (tail == head)
	{
		sizeToTail = ringBufferSize - 1;
		tailIsBehind = true;
	}

	//Check if message fits (in remaining memory and between head and tail
	//if it fits, create and send header, then send message and return true
	if (MessageFits(head, tail, ringBufferSize, _length))
	{

		auto hdr = CreateHeader(1, length);
		char* cpMsg = (char*)msg;
		char* pByte = (char*)pRingBuffer + head;

		//Send header and advance head
		ringBuffer.Send(pByte, (PVOID)&hdr, sizeof(hdr));
		pByte += sizeof(hdr);

		//send message
		ringBuffer.Send(pByte, cpMsg, length);
		head = (head + _length) % ringBufferSize;

		//MString string = "Message sent. New head: ";
		//string += (unsigned int)head;
		//MGlobal::displayInfo(string);

		UpdateRBD(head);
		RETURN_SAFE_TRUE;
	}

	//if message doesnt fit and tail is in front of head, we cant pad memory;
	//return and wait for tail to advance
	else if (tail > head)
	{
		RETURN_SAFE_FALSE;
	}
	//we can not fit the message in the remaining memory. Pad and set 
	//head to 0. (Make sure tail is > 0, since that would imply that 
	//the buffer is empty, when it is in fact full. Thus we check for
	//tail < 0.
	else if (tail > 0)
	{
		// *** PAD END ***
		//if we can fit a header in the remaining memory, create a dummy
		//message. If not, just set remaining memory to '\0' and set head = 0 
		//and return false.
		if (sizeToEOB >= sizeof(Header))
		{
			auto hdr = CreateHeader(0, ringBufferSize - head);
			ringBuffer.Send(ringBuffer.GetBuffer() + head, (PVOID)&hdr, sizeof(hdr));
		}
		memset(ringBuffer.GetBuffer() + head, 0, ringBufferSize - head);
		head = 0;
		UpdateRBD(head);
		RETURN_SAFE_FALSE;
	}

	//This should be redundant. Keeping it just in case
	else if (tailIsBehind && sizeToEOB < sizeof(Header) && tail > 0)
	{
		head = 0;
		UpdateRBD(head);
		RETURN_SAFE_FALSE;
	}

	RETURN_SAFE_FALSE;

}

bool ComLib::recv(char * msg, size_t & length)
{
	//Acquire mutex
	WaitForSingleObject(hnd_Mutex, INFINITE);

	//Update head
	this->head = GetHead();

	//Create pointer from where we will start recieving 
	char* pByte = (char*)this->pRingBuffer + tail;

	//Determine if head is in front of tail
	bool headIsInFront = (head >= tail);

	//Determine how much memory is left in the buffer
	size_t sizeToEOB = ringBufferSize - tail;

	//Determine how much memory is left until head
	size_t sizeToHead;
	headIsInFront ? sizeToHead = head - tail : sizeToHead = ringBufferSize - tail + head;

	//Recieve header if there is space
	//for it.
	Header hdr;
	if (sizeToHead >= sizeof(Header) && sizeToEOB >= sizeof(Header))
	{
		CopyMemory(&hdr, pByte, sizeof(Header));
		pByte += sizeof(Header);
	}

	//If the remaining memory cant hold a Header, 
	//roll back to 0.
	else if (sizeToEOB < sizeof(Header))
	{
		//Make sure head is not in front (it should never be, however)
		if (!headIsInFront)
		{
			tail = 0;
			UpdateRBD(tail);
			RETURN_SAFE_FALSE;
		}
		else
		{
			RETURN_SAFE_FALSE;
		}
	}
	else RETURN_SAFE_FALSE;
	
	//If msgId == 1, the message is a normal message. 
	//Recieve.
	if (hdr.msgId == 1)
	{
		CopyMemory(msg, pByte, hdr.msgLength);

		tail = (tail + hdr.msgLength + sizeof(Header)) % ringBufferSize;
		UpdateRBD(tail);
		length = hdr.msgLength;
		RETURN_SAFE_TRUE;
	}
	//If not, message is a dummy message (which means send() function
	//padded the memory). Roll back to 0.
	else
	{
		tail = 0;
		UpdateRBD(tail);
		RETURN_SAFE_FALSE;
	}

	RETURN_SAFE_FALSE;
}

std::optional<size_t> ComLib::nextSize()
{

	WaitForSingleObject(hnd_Mutex, INFINITE);

	this->head = GetHead();

	//Create pointer from where we will start recieving 
	char* pByte = (char*)this->pRingBuffer + tail;

	//Determine if head is in front of tail
	bool headIsInFront = (head >= tail);

	//Determine how much memory is left in the buffer
	size_t sizeToEOB = ringBufferSize - tail;

	//Determine how much memory is left until head
	size_t sizeToHead;
	headIsInFront ? sizeToHead = head - tail : sizeToHead = ringBufferSize - tail + head;

	//Recieve header if there is space for it.
	Header hdr;
	if (sizeToHead >= sizeof(Header) && sizeToEOB >= sizeof(Header))
	{
		CopyMemory(&hdr, pByte, sizeof(Header));
		pByte += sizeof(Header);

		UNLOCK(hnd_Mutex);
		return hdr.msgLength;
	}

	//If the remaining memory cant hold a Header, 
	//roll back to 0.
	else if (sizeToEOB < sizeof(Header))
	{
		//Make sure head is not in front (it should never be, however)
		if (!headIsInFront)
		{
			tail = 0;
			UpdateRBD(tail);
			UNLOCK(hnd_Mutex);
			return std::nullopt;
		}
		else
		{
			UNLOCK(hnd_Mutex);
			return std::nullopt;
		}
	}
	else
	{
		UNLOCK(hnd_Mutex);
		return std::nullopt;
	}
}
