#include <iostream>
#include <atomic>

#include "SnWARQ.hpp"
#include "NoisyChannel.hpp"

// g++ -std=c++23 main.cpp -o arq -Ofast -march=native -Wall -Wextra && ./arq

int main(void)
{
	std::printf("\nSimulation started...\n");
	std::printf("\n-------------------------------------------------");
	std::printf("\nSimulation Parametres:\n");
	std::printf("\nMESSAGES_TO_SEND = %ld", MESSAGES_TO_SEND);
	std::printf("\nBYTES_PER_MESSAGE = %ld", BYTES_PER_MESSAGE);
	std::printf("\nSHOW_CHANNEL_PACKETS = %s", SHOW_CHANNEL_PACKETS ? "True" : "False");
	std::printf("\nPACKET_MESSAGE_SIZE = %ld", PACKET_MESSAGE_SIZE);
	std::printf("\nSENDER_DEAFULT_MILLS_TIMEOUT = %ld", SENDER_DEAFULT_MILLS_TIMEOUT);
	std::printf("\nCHANNEL_DEFAULT_AVG_VALID_BYTES = %ld", CHANNEL_DEFAULT_AVG_VALID_BYTES);
	std::printf("\nCHANNEL_DEFAULT_AVG_MILLS_DELAY = %.2f", CHANNEL_DEFAULT_AVG_MILLS_DELAY);
	std::printf("\n-------------------------------------------------");
	std::printf("\n\nSender sends to Endpoint 'B' and listens to Endpoint 'A' of the channel.\n");
	std::printf("\nReceiver sends to Endpoint 'A' and listens to Endpoint 'B' of the channel.\n\n");

	NoisyChannel NC;

	SnWARQReceiver ARQReceiver(NC);

	SnWARQSender ARQSender(NC);

	std::jthread receiverThread 
	{ 
		[&ARQReceiver]
		{
			std::cout << "\nReceiver created and listens for messages..." << std::endl;

			while ( true )
			{
				const auto receivedMessage = ARQReceiver.receiveMessage();

				if ( receivedMessage == END_COMMUNICATION_MESSAGE )
					return;

				std::lock_guard lg(globalPrintMutex);

				std::cout << "Receiver: Message received:\n" << receivedMessage << "\n" << std::endl;
			}
		}
	};

	std::cout << "\nSender created and starts sending messages..." << std::endl;

	std::string message(BYTES_PER_MESSAGE, MESSAGE_BYTE_CHAR);

	for ( auto index { 0 } ; index not_eq MESSAGES_TO_SEND ; ++index )
		ARQSender.sendMessage(message + std::to_string(index));

	std::this_thread::sleep_for(std::chrono::milliseconds(TOTAL_SIMULATION_TIME_MILLS));

	ARQSender.sendMessage(END_COMMUNICATION_MESSAGE);

	std::this_thread::sleep_for(std::chrono::seconds(1));

	NC.printStats();

	std::printf("\nSimulation ended.\n\n");
}