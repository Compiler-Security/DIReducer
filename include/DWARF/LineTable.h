#ifndef DR_LINE_TABLE_H
#define DR_LINE_TABLE_H

#include <cstdint>
#include <vector>
#include <tuple>
#include <string>
#include <stdint.h>
#include <algorithm>

#include "Fig/panic.hpp"
#include <nlohmann/json.hpp>

struct LineTable {
    std::vector<std::tuple<uint64_t, uint16_t, uint16_t, std::string>> data;

    void insert(uint64_t addr, uint16_t row, uint16_t col, std::string &filename) {
        if(data.empty())
            this->data.emplace_back(addr, row, col, filename);
        else {
            uint64_t pre_addr = std::get<0>(this->data.back());
            if(pre_addr > addr)
                FIG_PANIC("Error in insert!", "addr ", addr, " is less than ", pre_addr);
            this->data.emplace_back(addr, row, col, filename);
        }
    }

    const std::tuple<uint64_t, uint16_t, uint16_t, std::string> &find(uint64_t addr) const {
        auto it = std::lower_bound(
            this->data.begin(),
            this->data.end(), addr,
            [](const auto &t, uint64_t addr) {
                return std::get<0>(t) <= addr;
            });
        if(it == this->data.begin())
            FIG_PANIC("can not find ", addr, " in line table");
        else
            return *(--it);
    }

    bool empty() const {
        return this->data.empty();
    }

    nlohmann::json to_json() const {
        nlohmann::json content = {
            {"lineTableData", nlohmann::json(this->data)},
        };

        return content;
    }
};

#endif