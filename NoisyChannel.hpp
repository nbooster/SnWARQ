#ifndef NOISY_CHANNEL_HPP
#define NOISY_CHANNEL_HPP

/* A two way communication channel simulator that adds noise and delay. */

#include <cstdlib>
#include <thread>
#include <chrono>
#include <random>
#include <list>
#include <mutex>
#include <condition_variable>

#include "Params.hpp"

// rand() is not perfectly uniform (except if the range is a power of 2), but is used for simplicity here

void printPacket(const char* data, const std::uint32_t length, const bool showText = true) 
{
	std::uint32_t j;

    for ( std::uint32_t i = 0; i < length; ++i )
    {
        std::printf("%02x ", (unsigned char) data[i]);

        if ( ( (i % 16) == 15 ) or ( i == length - 1 ) )
        {
            for ( j = 0; j < 15 - ( i % 16 ); ++j )
                std::printf(" ");

            if ( showText ) 
            {
                while ( j-- > 0 )
                    std::printf("  ");

                std::printf("| ");

                for ( j = ( i - ( i % 16 ) ); j <= i; ++j )
                {
                    auto byte = data[j];
                    
                    if ( ( byte > 31 ) and ( byte < 127 ) )
                        std::printf("%c", byte);
                    
                    else
                        std::printf(".");
                }
	    	}

            std::printf("\n");
        }
    }

    std::printf("\n");

    std::cout.flush();
}

inline void flipRandomBit(std::string& message)
{
	message.data()[rand() % std::max(static_cast<std::string::size_type>(1), message.size())] ^= (1 << (std::rand() % 8)); 
}

class NoisyChannel
{
	std::size_t avgValidBytes { 0 };

	std::size_t totalPacketsSentToA { 0 };

	std::size_t totalPacketsSentToB { 0 };

	std::size_t totalPacketsReceivedFromA { 0 };

	std::size_t totalPacketsReceivedFromB { 0 };

	std::mutex mutexA;

	std::mutex mutexB;

	std::condition_variable conVarA;

	std::condition_variable conVarB;

    std::random_device rd;

    std::mt19937 gen;
 
    std::poisson_distribution<std::size_t> poisson;

	std::list<std::string> bufferA;

	std::list<std::string> bufferB;

public:

	explicit NoisyChannel(const std::size_t& avgValidBytes = CHANNEL_DEFAULT_AVG_VALID_BYTES, const double& avgMilliSecsDelay = CHANNEL_DEFAULT_AVG_MILLS_DELAY)
	: avgValidBytes{ avgValidBytes }, gen{ this->rd() }, poisson{ avgMilliSecsDelay } 
	{
		srand(time(NULL));
	}

	void printStats(void) const
	{
		std::lock_guard lg(globalPrintMutex);

		printf("\nTotal packets sent to endpoint A: %ld", this->totalPacketsSentToA);
		printf("\nTotal packets sent to endpoint B: %ld", this->totalPacketsSentToB);
		printf("\nTotal packets received from endpoint A: %ld", this->totalPacketsReceivedFromA);
		printf("\nTotal packets received from endpoint B: %ld\n\n", this->totalPacketsReceivedFromB);

		std::cout.flush();
	}

	void sendToA(const void* message, const std::size_t& size)
	{
		static std::size_t bytes { this->avgValidBytes };

		std::this_thread::sleep_for(std::chrono::milliseconds(this->poisson(this->gen)));

		{
			std::lock_guard lock(this->mutexA);

			this->bufferA.emplace_back(static_cast<const char*>(message), size);

			if ( bytes > size )
				bytes -= size;

			else
			{
				bytes = this->avgValidBytes - (size - bytes);

				flipRandomBit(this->bufferA.back());
			}

			++(this->totalPacketsSentToA);
		}

		this->conVarA.notify_one();
	}

	std::string recvFromA(void)
	{
		std::string message;

		{
			std::unique_lock lock(this->mutexA);

		    this->conVarA.wait(lock, [this]{ return not this->bufferA.empty(); });

		    message.assign(this->bufferA.front());

		    this->bufferA.pop_front();

		    ++(this->totalPacketsReceivedFromA);
		}

	    if constexpr ( SHOW_CHANNEL_PACKETS )
	    {
	    	std::lock_guard lg(globalPrintMutex);

	    	std::printf("Endpoint 'A' received:\n");

	    	printPacket(message.data(), message.size());
	    }

	    return message;
	}

	void sendToB(const void* message, const std::size_t& size)
	{
		static std::size_t bytes { this->avgValidBytes };

		std::this_thread::sleep_for(std::chrono::milliseconds(this->poisson(this->gen)));

		{
			std::lock_guard lock(this->mutexB);

			this->bufferB.emplace_back(static_cast<const char*>(message), size);

			if ( bytes > size )
				bytes -= size;

			else
			{
				bytes = this->avgValidBytes - (size - bytes);

				flipRandomBit(this->bufferB.back());
			}

			++(this->totalPacketsSentToB);
		}

		this->conVarB.notify_one();
	}

	std::string recvFromB(void)
	{
		std::string message;

		{
			std::unique_lock lock(this->mutexB);

		    this->conVarB.wait(lock, [this]{ return not this->bufferB.empty(); });

		    message.assign(this->bufferB.front());

		    this->bufferB.pop_front();

		    ++(this->totalPacketsReceivedFromB);
		}

	    if constexpr ( SHOW_CHANNEL_PACKETS )
	    {
	    	std::lock_guard lg(globalPrintMutex);
	    	
	    	std::printf("Endpoint 'B' received:\n");

	    	printPacket(message.data(), message.size());
	    }

	    return message;
	}
};

#endif