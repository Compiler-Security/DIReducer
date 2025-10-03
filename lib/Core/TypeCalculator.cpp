#include "Core/TypeCalculator.h"
#include <stdint.h>            // for uint32_t
#include <algorithm>           // for max
#include <optional>            // for optional
#include <set>                 // for allocator, set, _Rb_tree_const_iterator
#include <string>              // for operator+, to_string, char_traits, bas...
#include <utility>             // for pair, make_pair
#include <variant>             // for variant
#include <vector>              // for vector
#include "CFG/Type.h"          // for TYPETAG, VTypePointer, VType
#include "CFG/TypeFactory.h"   // for VTypeFactory
#include "CFG/Utils.h"  // for getUniqueID
#include "Fig/panic.hpp"       // for FIG_PANIC
#include "Fig/unreach.hpp"     // for FIG_UNREACH

namespace SA {
    TypeCalculator::~TypeCalculator() {
        for(auto type : this->typeSet) {
            if(type->typeTag == CFG::TYPETAG::POINTER) {
                auto objType = type->getRefVType();
                objType->ptrType.reset();
            }
            CFG::VTypeFactory::destroyCustomType(type);
        }
        this->typeSet.clear();
    }

    CFG::VTypePointer TypeCalculator::getUnionVType(CFG::VTypePointer vType1, CFG::VTypePointer vType2) {
        using CFG::TYPETAG;
        using CFG::VTypePointer;
        using CFG::VTypeFactory;
        using CFG::getUniqueID;
        if(vType1 == vType2)
            return vType1;

        if((vType1->typeTag == TYPETAG::BASE || vType1->typeTag == TYPETAG::POINTER) && (vType2->typeTag == TYPETAG::BASE || vType2->typeTag == TYPETAG::POINTER)) {
            // create new type
            VTypePointer newUnionType = VTypeFactory::getCustomType(TYPETAG::UNION, "newUnionType" + std::to_string(getUniqueID()), std::max({vType1->size, vType2->size}));
            this->typeSet.insert(newUnionType);
            auto &unionMemberVec = newUnionType->getUnionMemberVec();
            unionMemberVec.push_back(vType1);
            unionMemberVec.push_back(vType2);
            // memoryModel
            newUnionType->memoryModel.push_back(std::make_pair(0, vType1));
            newUnionType->memoryModel.push_back(std::make_pair(0, vType2));

            return newUnionType;
        } else if(vType1->typeTag == TYPETAG::UNION && (vType2->typeTag == TYPETAG::BASE || vType2->typeTag == TYPETAG::POINTER)) {
            // check whether  vtype1 contains vtype2, if so, return vtype1
            for(auto it : vType1->memoryModel) {
                if(it.first == 0 && it.second == vType2)
                    return vType1;
            }
            // create new type
            VTypePointer newUnionType = VTypeFactory::getCustomType(TYPETAG::UNION, "newUnionType" + std::to_string(getUniqueID()), std::max({vType1->size, vType2->size}));
            this->typeSet.insert(newUnionType);
            auto &unionMemberVec = newUnionType->getUnionMemberVec();
            for(auto &vTypeMember : vType1->getUnionMemberVec()) {
                unionMemberVec.push_back(vTypeMember);
            }
            unionMemberVec.push_back(vType2);
            // memoryModel
            for(auto &it : vType1->memoryModel) {
                newUnionType->memoryModel.push_back(it);
            }
            newUnionType->memoryModel.push_back(std::make_pair(0, vType2));

            return newUnionType;
        } else if((vType1->typeTag == TYPETAG::BASE || vType1->typeTag == TYPETAG::POINTER) && vType2->typeTag == TYPETAG::UNION) {
            // check whether  vtype2 contains vtype1, if so, return vtype2
            for(auto it : vType2->memoryModel) {
                if(it.first == 0 && it.second == vType1)
                    return vType2;
            }
            // create new type
            VTypePointer newUnionType = VTypeFactory::getCustomType(TYPETAG::UNION, "newUnionType" + std::to_string(getUniqueID()), std::max({vType1->size, vType2->size}));
            this->typeSet.insert(newUnionType);
            auto &unionMemberVec = newUnionType->getUnionMemberVec();
            for(auto &vTypeMember : vType2->getUnionMemberVec()) {
                unionMemberVec.push_back(vTypeMember);
            }
            unionMemberVec.push_back(vType1);
            // memoryModel
            for(auto &it : vType2->memoryModel) {
                newUnionType->memoryModel.push_back(it);
            }
            newUnionType->memoryModel.push_back(std::make_pair(0, vType1));

            return newUnionType;
        } else if((vType1->typeTag == TYPETAG::UNION && vType2->typeTag == TYPETAG::UNION)) {
            // create new type
            VTypePointer newUnionType = VTypeFactory::getCustomType(TYPETAG::UNION, "newUnionType" + std::to_string(getUniqueID()), std::max({vType1->size, vType2->size}));
            this->typeSet.insert(newUnionType);
            std::set<VTypePointer> tempVTypeSet{};
            for(auto it : vType1->getUnionMemberVec())
                tempVTypeSet.insert(it);
            for(auto it : vType2->getUnionMemberVec())
                tempVTypeSet.insert(it);
            auto &unionMemberVec = newUnionType->getUnionMemberVec();
            for(auto it : tempVTypeSet)
                unionMemberVec.push_back(it);
            // memoryModel
            std::set<std::pair<uint32_t, VTypePointer>> tempMemoryModelSet{};
            for(auto &it : vType1->memoryModel)
                tempMemoryModelSet.insert(it);
            for(auto &it : vType2->memoryModel)
                tempMemoryModelSet.insert(it);
            for(auto &it : tempMemoryModelSet)
                newUnionType->memoryModel.push_back(it);

            return newUnionType;
        } else {
            FIG_UNREACH();
        }
    }

    CFG::VTypePointer TypeCalculator::getPointerVType(CFG::VTypePointer vType) {
        using CFG::TYPETAG;
        using CFG::VTypePointer;
        using CFG::VTypeFactory;
        if(vType->ptrType.has_value())
            return vType->ptrType.value();

        VTypePointer newPointerVType = VTypeFactory::getCustomType(TYPETAG::POINTER, vType->name + "*", 8);
        this->typeSet.insert(newPointerVType);
        newPointerVType->detail = vType;
        vType->ptrType.emplace(newPointerVType);
        // memoryModel
        newPointerVType->memoryModel.push_back(std::make_pair(0, newPointerVType));
        return newPointerVType;
    }

    CFG::VTypePointer TypeCalculator::getObjectVType(CFG::VTypePointer vType) {
        using CFG::TYPETAG;
        if(vType->typeTag != TYPETAG::POINTER)
            FIG_PANIC("can not get object type from a type that is not a pointer type!");

        return vType->getRefVType();
    }

} // namespace SA