#include "SharedMemoryBuffer.h"

SharedMemoryBuffer::SharedMemoryBuffer()
{
}

SharedMemoryBuffer::SharedMemoryBuffer(std::string name, size_t bufferSize)
{
	//Attempt to open first
	FileMapHandle = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,   // read/write access
		FALSE,                 // do not inherit the name
		_T("default"));        // name of mapping object

	//if NULL, File Mapping doesnt exist; create it.
	if (FileMapHandle == NULL)
	{
			FileMapHandle =
		CreateFileMapping(INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			bufferSize,
			_T(name.c_str()));

			pBuf = (char*)MapViewOfFile(FileMapHandle, // handle to map object
				FILE_MAP_ALL_ACCESS,  // read/write permission
				0,
				0,
				bufferSize);
	}
	else //File Mapping successfully opened; Map view.
	{
		pBuf = (char*)MapViewOfFile(
			FileMapHandle,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			0
		);
	}
}

bool SharedMemoryBuffer::Send(const PVOID& dest, const PVOID data, const size_t & size)
{
	//Copies memory from data to dest
	char* msg = (PCHAR)data;
	bool success = true;
	if (CopyMemory(dest, msg, size) == nullptr)
		success = false;
	return success;
}

bool SharedMemoryBuffer::Send(const size_t bytesIntoBuffer, const PVOID data, const size_t & size)
{
	bool success = true;
	if (CopyMemory((char*)(this->pBuf + bytesIntoBuffer), data, size) == nullptr)
		success = false;
	return success;
}

bool SharedMemoryBuffer::Recieve(PVOID dest, const PVOID buf, const size_t& size)
{
	bool success = true;
	if (CopyMemory(dest, buf, size) == nullptr)
		success = false;
	return success;
}

bool SharedMemoryBuffer::Recieve(PVOID dest, const size_t bytesIntoBuffer, const size_t& size)
{
	bool success = true;
	if (CopyMemory(dest, (char*)(this->pBuf + bytesIntoBuffer), size) == nullptr)
		success = false;
	return success;
}

char * SharedMemoryBuffer::GetBuffer()
{
	//Returns pBuf as char *
	char* p = (char*)pBuf;
	return p;
}


SharedMemoryBuffer::~SharedMemoryBuffer()
{
	//Unmap view and close handle
	UnmapViewOfFile(pBuf);
	CloseHandle(FileMapHandle);
}
