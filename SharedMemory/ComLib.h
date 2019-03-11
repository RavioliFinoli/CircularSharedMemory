#pragma once
#include <Windows.h>
#include <string>
#include "SharedMemoryBuffer.h"
#include <mutex>
#include <tchar.h>
#include <optional>

class ComLib
{
private:
	SharedMemoryBuffer ringBuffer;
	SharedMemoryBuffer ringBufferData;

	HANDLE hnd_Mutex;

	size_t ringBufferSize;
	size_t head;
	size_t tail;

	/**
	 * Updates head/tail with value, depending on if consumer or producer.*/
	void UpdateRBD(size_t value) const;

	/**
	 * Get circular buffer tail.*/
	size_t GetTail() const;

	/**
	 * Get circular buffer head.*/
	size_t GetHead() const;

	/**
	 * Pointer to the start of the circular buffer.*/
	PVOID pRingBuffer;
public:
	enum TYPE{ PRODUCER, CONSUMER };
	enum MSG_TYPE{ NORMAL, DUMMY };

	const TYPE type;
	/**
	 * Message header.*/
	struct Header
	{
		size_t msgId;
		size_t msgSeq;
		size_t msgLength;
	};

	/**
	* Create a ComLib.*/
	ComLib(const std::string& secret, const size_t& buffSize, TYPE type);

	/**
	 * Check status.*/
	bool isConnected() const;

	/**
	 * Returns "true" if data was sent successfully.
	 * Returns alse if for any reason the data could not be sent.*/
	bool send(const void * msg, const size_t length);


	/**
	*	Returns true if a message was received, false if we cannot read. (no message to read, for example)
	*   'msg' is expected to have enough space, use "nextSize" to check this and allocate as needed.
	*	Length indicates the data read, in case the user does not check ahead of time.
	*	Should never return DUMMY messages.*/
	bool recv(char * msg, size_t & length);

	/** 
	* Returns the length of the next message.*/
	std::optional<size_t> nextSize();

	/**
	* Disconnect and destroy all resources. */
	~ComLib();
};

