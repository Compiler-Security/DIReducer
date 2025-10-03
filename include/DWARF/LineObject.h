#ifndef DR_LINE_OBJECT_H
#define DR_LINE_OBJECT_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "DWARFBuffer.h"
#include "ELFObject.h"
#include "LineList.h"
#include "LineTable.h"

struct LineObject {
public:
    std::vector<LineList> lls;

public:
    LineObject() = default;
    LineObject(DWARFBuffer &dwarfBuffer, StrTable &lineStrTable, LineTable &lineTable) {
        char *lineBuffer   = dwarfBuffer.lineBuffer.get();
        int64_t lineSize   = dwarfBuffer.lineSize.get();
        int64_t lineOffset = 0;
        int16_t module     = 1;
        while(lineSize - lineOffset >= 12) {
            lls.emplace_back(lineBuffer, lineOffset, module++, lineStrTable, lineTable);
        }
    };

    nlohmann::json to_json() {
        nlohmann::json content;
        for(uint32_t i = 0; i < this->lls.size(); i++) {
            content["Module" + std::to_string(i + 1)] = this->lls[i].to_json();
        }

        return content;
    }
};

#endif