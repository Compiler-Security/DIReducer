#ifndef SET_ALGO_H
#define SET_ALGO_H

#include <set>

template <typename T>
bool unionSets(std::set<T>& dst, const std::set<T>& src) {
    bool changed = false;
    
    for (const auto& elem : src) {
        auto it = dst.insert(elem);
        changed = changed || it.second;
    }
    
    return changed;
}

#endif