#ifndef UTILS_COMMON_H
#define UTILS_COMMON_H

#include <cstdint>
#include <string>
#include <sstream>
#include <stdint.h>
#include <chrono>

template<typename T>
std::string dec_to_hex(T dec) {
    std::string res;
    std::stringstream ss;
    ss << std::hex << dec;
    ss >> res;
    return res;
}

template<>
std::string dec_to_hex(uint8_t dec);

int64_t getMaxMemUsage();
int64_t getFileSize(const std::string &filePath);
std::string removePathPrefix(const std::string &filePath);

using GenaralTimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
double getTimeByns(const GenaralTimePoint &start, const GenaralTimePoint &end);
double getTimeByus(const GenaralTimePoint &start, const GenaralTimePoint &end);
double getTimeByms(const GenaralTimePoint &start, const GenaralTimePoint &end);

#endif