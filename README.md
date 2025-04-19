# SnWARQ
A header only C++ 20 efficient implementation of the Stop and Wait ARQ communication protocol (using only the STL).

It is based on the most complete description of the protocol that I found here:

https://www.scaler.in/stop-and-wait-arq/

---------------------------------------------------------------------------------------------------------------------------------

The 'SnWARQ.hpp' file implements the protocol which takes the channel (concept type) as a parameter.

The channel must be a type (class) that implements these 4 methods:

void sendToA(const void* message, const std::size_t& size);

void sendToB(const void* message, const std::size_t& size);

std::string recvFromA();

std::string recvFromB();

The protocol consists of a sender (which spawns 2 background threads (one for sending and one for receiving the ACKs))
and a receiver (which spawns one background thread for receiving the packets and sending the ACKs).

The 'sendMessage()' of the sender is non blocking (it breaks up the message into packets, based on the sizes, and puts them in a buffer).

The 'receiveMessage()' method of the receiver blocks until the whole message of the sender is received.

The header implementation is acompanied by a simulation to show the protocol in action.

It's a simple simulation, and you can control its parameters in the 'Params.hpp' file.

The 'NoisyChannel' class simulates a two way communication channel which adds noise and time delays (built for the simulation).

See the 'main.cpp' for the simulation structure.

------------------------------------------------------------------------------------------

Compile and run: 

g++ -std=c++20 main.cpp -o arq -Ofast -march=native -Wall -Wextra && ./arq

-------------------------------------------------------------------------------------------

Optimization 0: Adjust message packet size and sender timeout (in 'SnWARQ.hpp') accordingly (based on your use case and channel characteristics).

Optimization 1: Use the Boost library instead of the STL (list and fast allocator).

Optimization 2: Replace the conditional variable of the receiver with an atomic_flag.

Optimization 3: Use better (and faster) hash function.

Optimization 4: Find and add more compiler flags for even better performance (like: -fno-rtti -fno-stack-protector)
