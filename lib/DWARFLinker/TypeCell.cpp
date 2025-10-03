#include "DWARFLinker/TypeCell.h"
#include "CFG/Type.h"
#include "DWARF/AttrBuffer.h"
#include "DWARFLinker/Linker.h"
#include "Fig/unreach.hpp"
#include "Fig/assert.hpp"
#include "Utils/Buffer.h"
#include "libdwarf/dwarf.h"
#include <cstdint>
#include <utility>

namespace DWARFLinker {
    InfoEntry TypeMember::getInfoEntry() const {
        InfoEntry infoEntry;
        if(isBitLocation) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::BitLocMemberTYPE);
            // | DW_AT_data_member_location, DW_FORM_udata
            AttrBuffer bitLocBuffer  = AttrBuffer();
            uint64_t LEB128Size = get_uint_length_leb128(this->memberLocation);
            bitLocBuffer.buffer = std::make_unique<char[]>(LEB128Size);
            bitLocBuffer.size   = LEB128Size;
            int64_t tmpOffset   = 0;
            write_leb128u(this->memberLocation, bitLocBuffer.buffer.get(), tmpOffset);
            infoEntry.attrBuffers.push_back(std::move(bitLocBuffer));
            // | DW_AT_type, DW_FORM_ref4
            AttrBuffer typeBuffer = AttrBuffer();
            typeBuffer.buffer     = std::make_unique<char[]>(4);
            typeBuffer.size       = 4;
            tmpOffset             = 0;
            write_u32(this->type->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
            infoEntry.attrBuffers.push_back(std::move(typeBuffer));
        } else {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::ByteLocMemberTYPE);
            // | DW_AT_data_bit_offset, DW_FORM_udata
            AttrBuffer byteLocBuffer = AttrBuffer();
            uint64_t LEB128Size      = get_uint_length_leb128(this->memberLocation);
            byteLocBuffer.buffer     = std::make_unique<char[]>(LEB128Size);
            byteLocBuffer.size       = LEB128Size;
            int64_t tmpOffset        = 0;
            write_leb128u(this->memberLocation, byteLocBuffer.buffer.get(), tmpOffset);
            infoEntry.attrBuffers.push_back(std::move(byteLocBuffer));
            // | DW_AT_type, DW_FORM_ref4
            AttrBuffer typeBuffer = AttrBuffer();
            typeBuffer.buffer     = std::make_unique<char[]>(4);
            typeBuffer.size       = 4;
            tmpOffset             = 0;
            write_u32(this->type->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
            infoEntry.attrBuffers.push_back(std::move(typeBuffer));
        }

        return infoEntry;
    }

    /// 1.Void Type
    /// Nothing
    /// 2.Base Type
    /// | DW_AT_byte_size, DW_FORM_data2
    /// | DW_AT_encoding, DW_FORM_data1
    /// 3.Pointer Type
    /// | DW_AT_type, DW_FORM_ref4
    /// 4.Array Type
    /// | DW_AT_type, DW_FORM_ref4
    /// | DW_AT_upper_bound, DW_FORM_udata
    /// 5.Structure Type
    /// | DW_AT_byte_size, DW_FORM_data2
    /// [With SubMember]
    /// 6.Union Type
    /// | DW_AT_byte_size, DW_FORM_data2
    /// [With SubMember]
    InfoEntry TypeCell::getInfoEntry() {
        InfoEntry infoEntry;
        if(this->tag == CFG::TYPETAG::BASE) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::BaseType);
            // | DW_AT_byte_size, DW_FORM_data2
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &byteSizeBuffer  = infoEntry.attrBuffers.back();
            byteSizeBuffer.buffer = std::make_unique<char[]>(2);
            byteSizeBuffer.size   = 2;
            int64_t tmpOffset     = 0;
            write_u16(this->size, byteSizeBuffer.buffer.get(), tmpOffset);
            // | DW_AT_encoding, DW_FORM_data1
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &encodingBuffer  = infoEntry.attrBuffers.back();
            encodingBuffer.buffer = std::make_unique<char[]>(1);
            encodingBuffer.size   = 1;
            tmpOffset             = 0;
            switch(this->basetype) {
            case CFG::BASETYPE::BT_UINT8:
            case CFG::BASETYPE::BT_UINT16:
            case CFG::BASETYPE::BT_UINT32:
            case CFG::BASETYPE::BT_UINT64:
            case CFG::BASETYPE::BT_UINT128: {
                write_u8(DW_ATE_unsigned, encodingBuffer.buffer.get(), tmpOffset);
                break;
            }
            case CFG::BASETYPE::BT_INT8:
            case CFG::BASETYPE::BT_INT16:
            case CFG::BASETYPE::BT_INT32:
            case CFG::BASETYPE::BT_INT64:
            case CFG::BASETYPE::BT_INT128: {
                write_u8(DW_ATE_signed, encodingBuffer.buffer.get(), tmpOffset);
                break;
            }
            case CFG::BASETYPE::BT_FLOAT32:
            case CFG::BASETYPE::BT_FLOAT64:
            case CFG::BASETYPE::BT_FLOAT128: {
                write_u8(DW_ATE_float, encodingBuffer.buffer.get(), tmpOffset);
                break;
            }
            case CFG::BASETYPE::BT_BOOL: {
                write_u8(DW_ATE_boolean, encodingBuffer.buffer.get(), tmpOffset);
                break;
            }
            default: FIG_UNREACH();
            }

        } else if(this->tag == CFG::TYPETAG::POINTER) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::PointerType);
            // | DW_AT_type, DW_FORM_ref4
            FIG_ASSERT(this->refTypeCell.has_value());
            FIG_ASSERT(this->refTypeCell.value().get() != nullptr);
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &typeBuffer  = infoEntry.attrBuffers.back();
            typeBuffer.buffer = std::make_unique<char[]>(4);
            typeBuffer.size   = 4;
            int64_t tmpOffset = 0;
            write_u32(this->refTypeCell.value()->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
        } else if(this->tag == CFG::TYPETAG::ARRAY) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::ArrayType);
            // | DW_AT_type, DW_FORM_ref4
            FIG_ASSERT(this->refTypeCell.has_value());
            FIG_ASSERT(this->refTypeCell.value().get() != nullptr);
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &typeBuffer  = infoEntry.attrBuffers.back();
            typeBuffer.buffer = std::make_unique<char[]>(4);
            typeBuffer.size   = 4;
            int64_t tmpOffset = 0;
            write_u32(this->refTypeCell.value()->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
            // | DW_AT_upper_bound, DW_FORM_udata
            infoEntry.attrBuffers.push_back(AttrBuffer());
            uint64_t boundLEB128Size = get_uint_length_leb128(this->arrayBound);
            auto &boundBuffer        = infoEntry.attrBuffers.back();
            boundBuffer.buffer       = std::make_unique<char[]>(boundLEB128Size);
            boundBuffer.size         = boundLEB128Size;
            tmpOffset                = 0;
            write_leb128u(this->arrayBound, boundBuffer.buffer.get(), tmpOffset);
        } else if(this->tag == CFG::TYPETAG::VOID) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::VoidTYPE);
        } else if(this->tag == CFG::TYPETAG::STRUCTURE) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::StructTYPE);
            // | DW_AT_byte_size, DW_FORM_data2
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &byteSizeBuffer  = infoEntry.attrBuffers.back();
            byteSizeBuffer.buffer = std::make_unique<char[]>(2);
            byteSizeBuffer.size   = 2;
            int64_t tmpOffset     = 0;
            write_u16(this->size, byteSizeBuffer.buffer.get(), tmpOffset);

            auto &children = infoEntry.children;
            FIG_ASSERT(this->typeMember.has_value());
            for(const auto &memberCell : this->typeMember.value()) {
                children.push_back(memberCell.getInfoEntry());
            }
            children.push_back(InfoEntry());
            children.back().abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::EndItem);
        } else if(this->tag == CFG::TYPETAG::UNION) {
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::UnionType);
            // | DW_AT_byte_size, DW_FORM_data2
            infoEntry.attrBuffers.push_back(AttrBuffer());
            auto &byteSizeBuffer  = infoEntry.attrBuffers.back();
            byteSizeBuffer.buffer = std::make_unique<char[]>(2);
            byteSizeBuffer.size   = 2;
            int64_t tmpOffset     = 0;
            write_u16(this->size, byteSizeBuffer.buffer.get(), tmpOffset);

            auto &children = infoEntry.children;
            FIG_ASSERT(this->typeMember.has_value());
            for(const auto &memberCell : this->typeMember.value()) {
                children.push_back(memberCell.getInfoEntry());
            }
            children.push_back(InfoEntry());
            children.back().abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::EndItem);
        } else {
            FIG_UNREACH();
        }

        return infoEntry;
    }

    TypeCell::TypeCell(CFG::TYPETAG tag) {
        this->tag = tag;
    }
} // namespace DWARFLinker
