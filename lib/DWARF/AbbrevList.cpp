#include "DWARF/AbbrevList.h"
#include "DWARF/AbbrevItem.h"  // for abbrev_item
#include "Fig/panic.hpp"

AbbrevList::AbbrevList(const char *const buffer, int64_t &offset) {
    this->offset = offset;
    while(true) {
        this->abbrevItemList.emplace_back(buffer, offset);
        if(this->abbrevItemList.back().isNull())
            break;
    }
}

AbbrevItem &AbbrevList::get_abbrev_item_by_number(uint64_t abbrevNumber) {
    if(abbrevNumber == 0) {
        FIG_PANIC("abbrevNumber == 0 means that this is empty infoEntry\n", "You can not use it!");
    } else if(abbrevNumber > this->abbrevItemList.size()) {
        FIG_PANIC("can not find a abbrev for the infoEntry whose abbrevNumber is ", abbrevNumber);
    } else {
        return this->abbrevItemList[abbrevNumber - 1];
    }
}

int64_t AbbrevList::getSize() {
    int64_t res = 0;
    for(auto &item : this->abbrevItemList) {
        res += item.getSize();
    }
    return res;
}

void AbbrevList::write(char *buffer, int64_t &offset) {
    for(auto &item : this->abbrevItemList) {
        item.write(buffer, offset);
    }
}

void AbbrevList::print() {
    for(auto &abbrev : this->abbrevItemList) {
        if(!abbrev.isNull())
            abbrev.print();
    }
}