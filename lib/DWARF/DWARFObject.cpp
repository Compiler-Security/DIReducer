#include "DWARF/DWARFObject.h"
#include "DWARF/AbbrevObject.h"   // for abbrev_object
#include "DWARF/DWARFBuffer.h"    // for dwarf_buffer
#include "DWARF/ELFObject.h"      // for elf_object (ptr only), str_table
#include "DWARF/InfoObject.h"     // for info_object
#include "DWARF/LineObject.h"     // for line_object
#include "DWARF/LoclistsObject.h" // for loclists_object
#include "Fig/panic.hpp"           // for FIG_PANIC
#include "Utils/Struct.h"       // for dr_immut

struct LineTable;

DWARFObject::DWARFObject(ELFObjetc &elfObject) {
    this->init(elfObject);
}

void DWARFObject::init(ELFObjetc &elfObject) {
    this->dwarfBuffer.init(elfObject);
    this->abbrevObject   = nullptr;
    this->infoObject     = nullptr;
    this->loclistsObject = nullptr;
    this->lineObject     = nullptr;
}

void DWARFObject::init_line_str() {
    if(this->dwarfBuffer.lineStrBuffer.is_empty())
        FIG_PANIC("can not find .debug_line_str");

    char *buffer = this->dwarfBuffer.lineStrBuffer.get();
    int64_t size = this->dwarfBuffer.lineStrSize.get();
    this->lineStrTable.init(buffer, size);
}

bool DWARFObject::is_abbrev_initialized() {
    return (this->abbrevObject != nullptr);
}

bool DWARFObject::is_info_initialized() {
    return (this->infoObject != nullptr);
}

bool DWARFObject::is_loclists_initialized() {
    return (this->loclistsObject != nullptr);
}

bool DWARFObject::is_line_initialized() {
    return (this->lineObject != nullptr);
}

void DWARFObject::init_abrrev() {
    this->abbrevObject.reset(new AbbrevObject(this->dwarfBuffer));
}

void DWARFObject::init_info() {
    if(!this->is_abbrev_initialized())
        FIG_PANIC("abbrevObject is not initialized");
    this->infoObject.reset(new InfoObject(this->dwarfBuffer, this->abbrevObject));
}

void DWARFObject::init_loclists() {
    if(this->dwarfBuffer.loclistsBuffer.is_set())
        this->loclistsObject.reset(new LoclistsObject(this->dwarfBuffer));
}

void DWARFObject::init_line(LineTable &lineTable) {
    this->lineObject.reset(new LineObject(this->dwarfBuffer, this->lineStrTable, lineTable));
}

void DWARFObject::write_info(char *buffer, int64_t &offset) {
    this->infoObject->write(buffer, offset);
}

void DWARFObject::write_abbrev(char *buffer, int64_t &offset) {
    this->abbrevObject->write(buffer, offset);
}

void DWARFObject::write_loclists(char *buffer, int64_t &offset) {
    this->loclistsObject->write(buffer, offset);
}