#ifndef DWARF_OBJECT_H
#define DWARF_OBJECT_H

#include <cstdint>         // for int64_t
#include <memory>          // for shared_ptr
#include "DWARFBuffer.h"  // for dwarf_buffer
#include "ELFObject.h"    // for str_table
struct AbbrevObject;
struct InfoObject;
struct LineObject;
struct LineTable;
struct LoclistsObject;

struct DWARFObject
{
public:
    DWARFBuffer dwarfBuffer;

public:
    std::shared_ptr<AbbrevObject> abbrevObject;
    std::shared_ptr<InfoObject> infoObject;
    std::shared_ptr<LoclistsObject> loclistsObject;
    std::shared_ptr<LineObject> lineObject;
    StrTable lineStrTable;

public:
    DWARFObject()=default;

    DWARFObject(ELFObjetc& elfObject);

    ~DWARFObject()=default;

    /* 初始化dwarfBuffer */
    void init(ELFObjetc& elfObject);

    void init_line_str();

    bool is_abbrev_initialized();

    bool is_info_initialized();

    bool is_loclists_initialized();

    bool is_line_initialized();

    void init_abrrev();

    void init_info();

    void init_loclists();

    /// @brief 解析.debug_line,同时填充lineTable,lineTable可供查询某一个pc值对应的源代码信息(文件名,行号,列号)
    /// @param lineTable: 待填充的lineTable
    void init_line(LineTable& lineTable);

    void write_info(char* buffer, int64_t& offset);

    void write_abbrev(char* buffer, int64_t& offset);
    
    void write_loclists(char* buffer, int64_t& offset);
};

#endif