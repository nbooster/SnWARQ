#ifndef PARAMS_HPP
#define PARAMS_HPP

#include <mutex>

std::mutex globalPrintMutex; // just to be sure for a fine printing


/* Simulation Parametres */


constexpr double CHANNEL_DEFAULT_AVG_MILLS_DELAY { 32.0 };

constexpr std::size_t CHANNEL_DEFAULT_AVG_VALID_BYTES { 1024ULL };


constexpr bool SHOW_CHANNEL_PACKETS { true };

constexpr std::size_t TOTAL_SIMULATION_TIME_MILLS { 2'000 };


constexpr std::size_t MESSAGES_TO_SEND { 2ULL };

constexpr std::size_t BYTES_PER_MESSAGE { 32ULL }; // plus the number of message

constexpr char MESSAGE_BYTE_CHAR { 'X' };


#endif