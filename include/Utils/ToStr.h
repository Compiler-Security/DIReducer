#ifndef UTILS_TOSTR_H
#define UTILS_TOSTR_H

#include <cstdint>
#include <stdint.h>

#define CASE_RETURN_DW_TAG_STRING(tag) \
    case tag: return #tag;

#define CASE_RETURN_DW_AT_STRING(attr) \
    case attr: return #attr;

#define CASE_RETURN_DW_FORM_STRING(form) \
    case form: return #form;

#define CASE_RETURN_DW_UT_STRING(ut) \
    case ut: return #ut;

#define CASE_RETURN_DW_OP_STRING(op) \
    case op: return #op;

#define CASE_RETURN_DW_LLE_STRING(lle) \
    case lle: return #lle;

#define CASE_RETURN_DW_LNCT_STRING(lnct) \
    case lnct: return #lnct;

#define CASE_RETURN_DW_LNS_STRING(lns) \
    case lns: return #lns;

#define CASE_RETURN_DW_LNE_STRING(lne) \
    case lne: return #lne;

const char *dw_at_to_str(uint64_t attr);
const char *dw_tag_to_str(uint64_t tag);
const char *dw_form_to_str(uint64_t form);
const char *dw_ut_to_str(uint8_t ut);
const char *dw_op_to_str(uint8_t op);
const char *dw_lle_to_str(uint8_t lle);
const char *dw_lnct_to_str(uint64_t lnct);
const char *dw_lns_to_str(uint64_t lns);
const char *dw_lne_to_str(uint64_t lne);

#endif