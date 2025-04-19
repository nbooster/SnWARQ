#ifndef SNW_ARQ_HPP
#define SNW_ARQ_HPP

/* 
Stop and Wait ARQ communication protocol, with hash validation, operating on a two way channel.
Protocol Description here: https://www.scaler.in/stop-and-wait-arq/ 
*/

#include <cstring>
#include <string>
#include <cstdlib>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

constexpr std::size_t PACKET_MESSAGE_SIZE { 1024ULL };

constexpr std::size_t SENDER_DEAFULT_MILLS_TIMEOUT { 64ULL };

inline static const std::string END_COMMUNICATION_MESSAGE { "$END_COMMUNICATION$" };

template<typename T>
concept Channel = requires(T&& channel, const void* message, const std::size_t& size)
{
    { channel.sendToA(message, size) } -> std::same_as<void>;
    { channel.sendToB(message, size) } -> std::same_as<void>;
    { channel.recvFromA() } -> std::same_as<std::string>;
    { channel.recvFromB() } -> std::same_as<std::string>;
};

inline std::size_t hashString(const std::string& input)
{
	static const std::hash<std::string> hasher;

	return hasher(input);
}

inline bool validateStringHash(const std::string& input, const std::size_t& digest)
{
	static const std::hash<std::string> hasher;

	return hasher(input) == digest;
}

class __attribute__ ((packed, aligned(1))) ARQACKPacket
{
	bool acknowledged { false };

	std::size_t packetNumber { 0 };

	std::size_t packetDigest { 0 };

public:

	explicit ARQACKPacket(const bool& ack, const std::size_t& number): 
	acknowledged{ ack }, packetNumber{ number }, 
	packetDigest{ hashString(std::string(static_cast<const char*>(static_cast<const void*>(this)), sizeof(ARQACKPacket) - sizeof(std::size_t))) }
	{}

	explicit ARQACKPacket(const void* bytes)
	{
		this->acknowledged = static_cast<const ARQACKPacket*>(bytes)->getAcknowledged();

		this->packetNumber = static_cast<const ARQACKPacket*>(bytes)->getNumber();

		this->packetDigest = static_cast<const ARQACKPacket*>(bytes)->getDigest();
	}

	bool isValid(const std::size_t& number) const noexcept
	{
		return 
		this->acknowledged and 
		( this->packetNumber == number ) and 
		validateStringHash(std::string(static_cast<const char*>(static_cast<const void*>(this)), sizeof(ARQACKPacket) - sizeof(std::size_t)), this->packetDigest);
	}

	bool getAcknowledged(void) const noexcept
	{
		return this->acknowledged;
	}

	std::size_t getNumber(void) const noexcept
	{
		return this->packetNumber;
	}

	std::size_t getDigest(void) const noexcept
	{
		return this->packetDigest;
	}
};

class __attribute__ ((packed, aligned(1))) ARQMessagePacket
{
	std::size_t packetDigest { 0 };

	std::size_t packetNumber { 0 };

	std::size_t packetMessageSize { 0 };

	char packetMessage[std::max(PACKET_MESSAGE_SIZE, static_cast<std::size_t>(sizeof(std::size_t)))] { 0 };

public:

	explicit ARQMessagePacket(const std::size_t& number, const char* message, const std::size_t& size)
	: packetNumber{ number }, packetMessageSize{ size }
	{
		std::memcpy(this->packetMessage, message, std::min(PACKET_MESSAGE_SIZE, size));

		this->packetDigest = hashString(std::string(static_cast<const char*>(static_cast<const void*>(this)) + sizeof(std::size_t), sizeof(ARQMessagePacket) - sizeof(std::size_t)));
	}

	explicit ARQMessagePacket(const void* bytes)
	{
		this->packetDigest = static_cast<const ARQMessagePacket*>(bytes)->getDigest();

		this->packetNumber = static_cast<const ARQMessagePacket*>(bytes)->getNumber();

		this->packetMessageSize = static_cast<const ARQMessagePacket*>(bytes)->getMessageSize();

		std::memcpy(this->packetMessage, static_cast<const ARQMessagePacket*>(bytes)->getMessagePtr(), PACKET_MESSAGE_SIZE);
	}

	std::size_t getDigest(void) const noexcept
	{
		return this->packetDigest;
	}

	std::size_t getNumber(void) const noexcept
	{
		return this->packetNumber;
	}

	std::size_t getMessageSize(void) const noexcept
	{
		return this->packetMessageSize;
	}

	std::string getMessageString(void) const
	{
		return std::string(this->packetMessage, PACKET_MESSAGE_SIZE);
	}

	const char* getMessagePtr(void) const noexcept
	{
		return this->packetMessage;
	}

	bool isValid(void) const noexcept
	{
		return validateStringHash(std::string(static_cast<const char*>(static_cast<const void*>(this)) + sizeof(std::size_t), sizeof(ARQMessagePacket) - sizeof(std::size_t)), this->packetDigest);
	}
};

class SnWARQSender
{
	std::size_t millsTimeOut { 1'000 };

	std::list<ARQMessagePacket> packetsBuffer;

	std::mutex listMutex;

	std::condition_variable conVar;

	std::jthread sendingRoutineThread;

	std::jthread receivingRoutineThread;

	
	template<Channel C>
	void sendingRoutine(C& channel)
	{
		std::unique_lock ulock(this->listMutex);

		while ( true )
		{
			if ( not this->packetsBuffer.empty() ) [[unlikely]]
			{
				channel.sendToB(static_cast<const void*>(std::addressof(this->packetsBuffer.front())), sizeof(ARQMessagePacket));
				
				if ( this->packetsBuffer.front().getNumber() == 0 ) [[unlikely]]
				{
					ARQACKPacket EndACK { true, 0 };

					channel.sendToA(static_cast<const void*>(std::addressof(EndACK)), sizeof(ARQACKPacket));

					return;
				}
			}

			this->conVar.wait_for(ulock, std::chrono::milliseconds(this->millsTimeOut));
		}
	}

	template<Channel C>
	void receivingRoutine(C& channel)
	{
		std::unique_lock ulock(this->listMutex, std::defer_lock_t{});

		while ( true )
		{
			const auto ackPacket = ARQACKPacket(static_cast<const void*>(channel.recvFromA().data()));

			if ( ackPacket.isValid(0) ) [[unlikely]]
				return;

			ulock.lock();

			if ( ackPacket.isValid(this->packetsBuffer.front().getNumber()) )
			{
				this->packetsBuffer.pop_front();

				ulock.unlock();

				this->conVar.notify_one();
			}

			else
				ulock.unlock();
		}
	}

public:

	template<Channel C>
	explicit SnWARQSender(C& channel, const std::size_t& millsTimeOut = SENDER_DEAFULT_MILLS_TIMEOUT):
	millsTimeOut{ millsTimeOut }, 
	sendingRoutineThread{ &SnWARQSender::sendingRoutine<C>, this, std::ref(channel) },
	receivingRoutineThread{ &SnWARQSender::receivingRoutine<C>, this, std::ref(channel) }
	{}

	void sendMessage(const std::string& messageString)
	{
		this->sendMessage(messageString.data(), messageString.size());
	}

	void sendMessage(const char* message, const std::size_t& size)
	{
		const auto div = std::lldiv(size, PACKET_MESSAGE_SIZE);

		const std::size_t packets = div.quot + static_cast<std::size_t>(div.rem > 0);

		std::unique_lock ulock(this->listMutex);

		if ( message not_eq END_COMMUNICATION_MESSAGE ) [[likely]]
		{
			this->packetsBuffer.emplace_back(1, static_cast<const char*>(static_cast<const void*>(std::addressof(packets))), sizeof(decltype(packets)));

			for ( std::size_t index { 0 } ; index not_eq packets ; ++index )
				this->packetsBuffer.emplace_back(index + 2, message + index * PACKET_MESSAGE_SIZE, index == packets - 1 ? div.rem : PACKET_MESSAGE_SIZE );
		}

		else
			this->packetsBuffer.emplace_back(0, message, 0);

		ulock.unlock();

		this->conVar.notify_one();
	}
};

class SnWARQReceiver
{
	std::size_t currentMessagePackets { 0 };

	std::size_t lastMessageNumber { 0 };

	std::string currentMessage;

	std::list<std::string> receivedMessages;

	std::mutex listMutex;

	std::condition_variable conVar;

	std::jthread receivingRoutineThread;


	void handleMessagePacket(const ARQMessagePacket& messagePacket)
	{
		const auto& packetNumber { messagePacket.getNumber() };

		if ( packetNumber == 0 )
		{
			{
				std::lock_guard lock(this->listMutex);

				this->receivedMessages.emplace_back(END_COMMUNICATION_MESSAGE);
			}

			this->conVar.notify_one();

			return;
		}

		if ( packetNumber == 1 )
		{
			this->lastMessageNumber = 1;
			
			this->currentMessage.clear();

			this->currentMessagePackets = *static_cast<const std::size_t*>(static_cast<const void*>(messagePacket.getMessagePtr()));

			return;
		}

		if ( packetNumber <= this->lastMessageNumber ) [[unlikely]]
			return;

		this->lastMessageNumber = packetNumber;

		this->currentMessage.append(messagePacket.getMessagePtr(), messagePacket.getMessageSize());
		
		if ( this->currentMessagePackets-- == 1 )
		{
			{
				std::lock_guard lock(this->listMutex);

				this->receivedMessages.emplace_back(this->currentMessage);
			}

			this->conVar.notify_one();
		}
	}

	template<Channel C>
	void receivingRoutine(C& channel)
	{
		while ( true )
		{
			if ( const auto messagePacket = ARQMessagePacket(static_cast<const void*>(channel.recvFromB().data())) ; messagePacket.isValid() ) [[likely]]
			{
				const ARQACKPacket ACK { true, messagePacket.getNumber() };

				channel.sendToA(static_cast<const void*>(std::addressof(ACK)), sizeof(ARQACKPacket));

				this->handleMessagePacket(messagePacket);

				if ( messagePacket.getNumber() == 0 ) [[unlikely]]
					return;
			}

			else
			{
				const ARQACKPacket NAK { false, messagePacket.getNumber() };

				channel.sendToA(static_cast<const void*>(std::addressof(NAK)), sizeof(ARQACKPacket));
			}
		}
	}

public:

	template<Channel C>
	explicit SnWARQReceiver(C& channel):
	receivingRoutineThread{ &SnWARQReceiver::receivingRoutine<C>, this, std::ref(channel) }
	{}

	std::string receiveMessage(void)
	{
		std::unique_lock ulock(this->listMutex);

		this->conVar.wait(ulock, [this]{ return not this->receivedMessages.empty(); });

		const auto message { this->receivedMessages.front() };

		this->receivedMessages.pop_front();

		ulock.unlock();

		return message;
	}
};

#endif
