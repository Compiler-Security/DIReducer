#include <chrono>               // for duration_cast, operator-, dura...
#include <bits/types/struct_rusage.h>  // for rusage
#include <stddef.h>                    // for size_t
#include <sys/resource.h>              // for getrusage, RUSAGE_SELF
#include <cstdint>                     // for int64_t, uint16_t, uint8_t
#include <fstream>                     // for basic_ifstream, char_traits
#include <sstream>                     // for basic_stringstream
#include <string>                      // for string, operator>>
#include "Fig/panic.hpp"
#include "Utils/Common.h"            // for GenaralTimePoint, dec_to_hex

template<>
std::string dec_to_hex(uint8_t dec) {
    std::string res;
    std::stringstream ss;
    ss << std::hex << static_cast<uint16_t>(dec);
    ss >> res;
    return res;
}

int64_t getMaxMemUsage() {
    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0)
        return rusage.ru_maxrss;
    else
        return -1;
}

int64_t getFileSize(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if(!file.is_open())
        FIG_PANIC("Unable to open file: ", filePath);

    int64_t fileSize = file.tellg();
    file.close();

    return fileSize;
}

std::string removePathPrefix(const std::string &filePath) {
    size_t lastSlashPos = filePath.find_last_of("/");
    return (lastSlashPos != std::string::npos) ? filePath.substr(lastSlashPos + 1) : filePath;
}

double getTimeByns(const GenaralTimePoint &start, const GenaralTimePoint &end) {
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(duration.count());
}

double getTimeByus(const GenaralTimePoint &start, const GenaralTimePoint &end) {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return static_cast<double>(duration.count());
}

double getTimeByms(const GenaralTimePoint &start, const GenaralTimePoint &end) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return static_cast<double>(duration.count());
}