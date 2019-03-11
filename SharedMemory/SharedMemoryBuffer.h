#pragma once
#include <Windows.h>
#include <string>
#include <iostream>
#include <tchar.h>

/**
 * Helper class for shared memory. Creates/Open File Map upon initialization 
 * and copies data to/from memory.*/
class SharedMemoryBuffer
{
	const char* pBuf;
	HANDLE FileMapHandle;
	size_t bufferSize;
	
public:

	SharedMemoryBuffer();

	/**
	* Call to initialize the ring buffer and Open/Create a File Map. */
	char * Init(std::string name, size_t bufferSize);

	/**
	*_in_	void*	dest
	*_in_	void*	data
	*_in_	size_t	length
	*Assumes enough space in buffer.*/
	static bool Send(const PVOID& dest, const PVOID data, const size_t& size);

	/**
	*_in_	size_t	bytesIntoBuffer
	*_in_	void*	data
	*_in_	size_t	length
	*Assumes enough space in buffer.*/
	bool Send(const size_t, const PVOID data, const size_t& size) const;

	/**
	*_out_	void*	dest
	*_in_	void*	buffer
	*_in_	size_t	size*/
	static bool Recieve(PVOID dest, const PVOID buf, const size_t& size);

	/**
	*_out_	void*	dest
	*_in_	size_t	bytesIntoBuffer
	*_in_	size_t	size*/
	bool Recieve(PVOID dest, const size_t bytesIntoBuffer, const size_t& size) const;

	char* GetBuffer() const;

	~SharedMemoryBuffer();
};

