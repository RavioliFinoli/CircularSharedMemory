#include "ComLib.h"
#include <iostream>
#include <string>

int main()
{
	auto producer = ComLib("arbitraryName", 1024 /*bytes*/, ComLib::PRODUCER /*Sends messages*/); 
	auto consumer = ComLib("arbitraryName", 1024 /*bytes*/, ComLib::CONSUMER /*Recieves messages*/);

	//Create a message
	std::string messageToSend = "Hello, Memory!";

	//Send message and check result 
	bool result = producer.send(messageToSend.c_str(), sizeof(char) * messageToSend.size());
	if (!result)
		exit(-1);
	std::cout << "Sending message..\n";

	//Get size of message we are about to recieve
	auto messageLength = consumer.nextSize();
	if (!messageLength.has_value())
		exit(-2);

	std::cout << "Expecting message of length " << messageLength.value() << "..\n";
	
	//Allocate space for message (+ null-terminator) and recieve
	char* recievedMessage = new char[messageLength.value() + 1]{};
	result = consumer.recv(recievedMessage, messageLength.value());
	if (!result)
		exit(-3);

	//Print message
	std::cout << "Recieved message:\n\t" << recievedMessage << '\n';

	std::getchar();
}
