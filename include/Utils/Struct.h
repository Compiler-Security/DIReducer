#ifndef UTILS_STRUCT_H
#define UTILS_STRUCT_H

#include <string>
#include "Fig/panic.hpp"
#include "libelf/elf.h"

struct dr_Elf64_Shdr {
    std::string name;
    Elf64_Shdr shdr;
};

template<typename T>
class dr_immut {
private:
    T value;
    bool flag_empty;

public:
    dr_immut(T val) :
        value(val), flag_empty(false) {
    }
    dr_immut() :
        flag_empty(true) {
    }

    bool is_empty() const {
        return flag_empty;
    }

    bool is_set() const {
        return !flag_empty;
    }

    void set(T val) {
        if(flag_empty) {
            value      = val;
            flag_empty = false;
        } else {
            FIG_PANIC("Value cannot be modified.");
        }
    }

    T get() const {
        if(!flag_empty) {
            return value;
        } else {
            FIG_PANIC("Value is not set.");
        }
    }
};

#endif