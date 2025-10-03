#include "Context.h"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "CFG/AsmBlock.h"
#include "CFG/AsmCFG.h"
#include "CFG/Edge.h"
#include "CFG/Func.h"
#include "CFG/Type.h"
#include "CFG/TypeFactory.h"
#include "CFG/Utils.h"
#include "CFG/Var.h"
#include "DWARF/AbbrevEntry.h"
#include "DWARF/AbbrevItem.h"
#include "DWARF/AbbrevList.h"
#include "DWARF/AbbrevObject.h"
#include "DWARF/AbbrevParser.h"
#include "DWARF/AttrBuffer.h"
#include "DWARF/CompilationUnit.h"
#include "DWARF/DWARFBuffer.h"
#include "DWARF/DWARFExprParser.h"
#include "DWARF/DWARFOP.h"
#include "DWARF/InfoEntry.h"
#include "DWARF/InfoObject.h"
#include "Config.h"
#include "Fig/assert.hpp"
#include "Fig/detail/StandardPrint.hpp"
#include "Fig/detail/Utils.hpp"
#include "Fig/log.hpp"
#include "Fig/panic.hpp"
#include "Core/Analysis.h"
#include "Utils/Buffer.h"
#include "Utils/Common.h"
#include "Utils/Struct.h"
#include "Utils/ToStr.h"
#include "capstone/capstone.h"
#include "capstone/x86.h"
#include "libdwarf/dwarf.h"
#include "libelf/elf.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

namespace fs = std::filesystem;

void DIReducerContext::run() {
    this->coreInfo.allStartTime = std::chrono::steady_clock::now();
    this->coreInfo.SPStartTime1 = std::chrono::steady_clock::now();
    if(this->config.is_dwarf_verbose)
        FIG_LOG("Source Program: ", this->config.src_path, ".");
    // Initialize elf
    if(this->config.is_dwarf_verbose)
        FIG_LOG("Initializing ELF Data.");

    this->initSrc();
    if(this->config.is_print_elf) {
        this->elfObject.print();
        return;
    }
    if(this->config.is_print_abbrev) {
        this->dwarfObject.abbrevObject->print();
        return;
    }
    if(this->config.is_print_info) {
        this->dwarfObject.infoObject->print();
        return;
    }

    if(!this->config.is_run)
        return;

    this->initDst();

    this->coreInfo.SPEndTime1 = std::chrono::steady_clock::now();

    this->initSPA();

    this->collectInputInfo();

    this->executeTypeInference();

    this->coreInfo.SPStartTime2 = std::chrono::steady_clock::now();

    this->linkerOpt.emplace(this->redundantVarIdSet, this->absVarsInfo);

    this->initLink();

    this->coreInfo.SPEndTime2 = std::chrono::steady_clock::now();

    auto &abbrevShdr           = this->dstElfObject.getSecByName(".debug_abbrev");
    abbrevShdr.shdr.sh_size    = this->linkerOpt->dstAbbrevObject->getSize();
    auto &infoShdr             = this->dstElfObject.getSecByName(".debug_info");
    infoShdr.shdr.sh_size      = this->linkerOpt->dstInfoObject->getSize();
    this->coreInfo.BRStartTime = std::chrono::steady_clock::now();
    this->dump();
    this->coreInfo.BREndTime                    = std::chrono::steady_clock::now();
    this->coreInfo.allEndTime                   = std::chrono::steady_clock::now();
    this->coreInfo.AllTimeCost                  = getTimeByms(this->coreInfo.allStartTime, this->coreInfo.allEndTime);
    this->coreInfo.DISTimeCost                  = getTimeByms(this->coreInfo.DISStartTime, this->coreInfo.DISEndTime);
    this->coreInfo.TITimeCost                   = getTimeByms(this->coreInfo.TIStartTime, this->coreInfo.TIEndTime);
    this->coreInfo.SPTimeCost                   = getTimeByms(this->coreInfo.SPStartTime1, this->coreInfo.SPEndTime1) + getTimeByms(this->coreInfo.SPStartTime2, this->coreInfo.SPEndTime2);
    this->coreInfo.BRTimeCost                   = getTimeByms(this->coreInfo.BRStartTime, this->coreInfo.BREndTime);
    this->coreInfo.OtherTimeCost                = this->coreInfo.AllTimeCost - this->coreInfo.DISTimeCost - this->coreInfo.TITimeCost - this->coreInfo.SPTimeCost - this->coreInfo.BRTimeCost;
    this->coreInfo.outputFileSizeWithoutSymbols = getFileSize(this->dstElfObject.filePath);
    this->coreInfo.outputDWARFSize              = this->linkerOpt->dstInfoObject->getSize() + this->linkerOpt->dstAbbrevObject->getSize();
    this->coreInfo.maxMemUsage                  = static_cast<double>(getMaxMemUsage()) / 1024;
    this->njson["BaseInfo"]                     = this->getBaseInfo();
    this->njson["Core"]                         = this->getCoreInfo();
    std::cout << this->njson.dump(2) << std::endl;
};

void DIReducerContext::executeTypeInference() {
    this->tiRuntimeInfo.startTime = std::chrono::steady_clock::now();
    if(this->config.is_analysis_verbose)
        FIG_LOG("Running Type Inference.");

    uint32_t numOfThread = this->config.is_single_thread ? 1 : boost::thread::hardware_concurrency();
    boost::asio::thread_pool pool(numOfThread);
    std::vector<std::shared_ptr<SA::TIAnalysis>> analyses;
    std::string programName = fs::path(this->elfObject.filePath).filename();
    uint32_t curIndex       = 1;
    uint32_t funcSum        = std::accumulate(
        this->funcDataBase.begin(), this->funcDataBase.end(), 0,
        [](uint32_t sum, const auto &fMap) { return sum + fMap.size(); });
    boost::mutex analysisMutex;
    this->coreInfo.TIStartTime = std::chrono::steady_clock::now();
    for(auto &fMap : this->funcDataBase) {
        for(auto &[addr, f] : fMap) {
            auto analysis = std::make_shared<SA::TIAnalysis>(programName, f);
            analysis->setFuncTypeMap(this->customFuncMap);
            analysis->setDynLibFuncMap(this->dynLibFuncMap);
            analysis->setGlobalVars(this->globalVars);
            analyses.push_back(analysis);
            // parallel run
            auto work = [&curIndex, &analysisMutex, funcSum, analysis, this]() {
                analysis->run();
                boost::mutex::scoped_lock lock(analysisMutex);
                if(this->config.is_analysis_verbose) {
                    fig::StandardPrint(curIndex, "/", funcSum, " ", analysis->funcName, "\n");
                    fig::StandardFlush();
                    curIndex++;
                }
            };
            boost::asio::post(pool, work);
        }
    }
    pool.join();
    this->coreInfo.TIEndTime = std::chrono::steady_clock::now();

    this->tiRuntimeInfo.endTime = std::chrono::steady_clock::now();
    for(const auto &analysis : analyses) {
        analysis->parameterInference();
    }

    // Only for statistical variable types
    // uint32_t totalPrimitiveVarNum = 0;
    // uint32_t totalCompoundVarNum  = 0;
    // uint32_t PrimitiveVarNum      = 0;
    // uint32_t CompoundVarNum       = 0;
    // for(auto &analysis : analyses) {
    //     analysis->collectResult();
    //     this->tiRuntimeInfo.append(analysis->runtimeInfo);
    //     for(auto id : analysis->getRedundantVarIdSet()) {
    //         this->redundantVarIdSet.insert(id);
    //     }

    //     for(auto &var : analysis->func->variableList) {
    //         if(var.vType->typeTag == CFG::TYPETAG::BASE || var.vType->typeTag == CFG::TYPETAG::POINTER || var.vType->typeTag == CFG::TYPETAG::ENUMERATION) {
    //             totalPrimitiveVarNum++;
    //         } else {
    //             totalCompoundVarNum++;
    //         }

    //         if(analysis->knownVarsId.contains(var.varId)) {
    //             if(var.vType->typeTag == CFG::TYPETAG::BASE || var.vType->typeTag == CFG::TYPETAG::POINTER || var.vType->typeTag == CFG::TYPETAG::ENUMERATION) {
    //                 PrimitiveVarNum++;
    //             } else if(var.vType->typeTag == CFG::TYPETAG::ARRAY) {
    //                 CompoundVarNum++;
    //             }
    //         }
    //     }

    //     for(auto &var : analysis->func->parameterList) {
    //         if(var.vType->typeTag == CFG::TYPETAG::BASE || var.vType->typeTag == CFG::TYPETAG::POINTER || var.vType->typeTag == CFG::TYPETAG::ENUMERATION) {
    //             totalPrimitiveVarNum++;
    //         } else {
    //             totalCompoundVarNum++;
    //         }

    //         if(var.isUsed && !var.isInfered) {
    //             if(var.vType->typeTag == CFG::TYPETAG::BASE || var.vType->typeTag == CFG::TYPETAG::POINTER || var.vType->typeTag == CFG::TYPETAG::ENUMERATION) {
    //                 PrimitiveVarNum++;
    //             } else if(var.vType->typeTag == CFG::TYPETAG::ARRAY) {
    //                 CompoundVarNum++;
    //             }
    //         }
    //     }
    // }

    // std::cout << "Before Primitive: \t" << totalPrimitiveVarNum << std::endl;
    // std::cout << "After Primitive: \t" << PrimitiveVarNum << std::endl;
    // std::cout << "Before Compound: \t" << totalCompoundVarNum << std::endl;
    // std::cout << "After Compound: \t" << CompoundVarNum << std::endl;
}

void DIReducerContext::collectInputInfo() {
    uint64_t symtabSize                       = this->elfObject.getSecByName(".symtab").shdr.sh_size;
    uint64_t strtabSize                       = this->elfObject.getSecByName(".strtab").shdr.sh_size;
    uint64_t symbolsSize                      = symtabSize + strtabSize;
    this->coreInfo.symbolsSize                = symbolsSize;
    this->coreInfo.inputFileSizeWithoutSymols = getFileSize(this->elfObject.filePath) - symbolsSize;
    this->coreInfo.inputDWARFSize             = this->elfObject.getDwarfSize();
}

nlohmann::json DIReducerContext::getFunctionsJson(bool withHeader, bool withInsns, bool withVars) {
    nlohmann::json content;
    for(const auto &fMap : this->funcDataBase) {
        for(const auto &[_, func] : fMap) {
            content[func->UniqueName()] = func->toJson(false, withInsns, withVars);
        }
    }
    if(withHeader)
        return {{"functions", content}};
    else
        return content;
}

nlohmann::json DIReducerContext::getBaseInfo() {
    nlohmann::json content = {
        {"program", fs::path(this->elfObject.filePath).filename()},
    };

    return content;
}

nlohmann::json DIReducerContext::getCoreInfo() {
    const auto specBefore      = this->dwarfObject.infoObject->getSpec();
    const auto specAfter       = this->linkerOpt->getSpec();
    const auto varMinInfo      = this->linkerOpt->linkInfo;
    uint64_t TIAttrsCount      = varMinInfo.redundantVarAttrs + varMinInfo.redundantTypeAttrs;
    uint64_t reducedAttrsCount = specBefore.allAttrsCount - specAfter.allAttrsCount;
    double TotalRatio          = static_cast<double>(reducedAttrsCount) / specBefore.allAttrsCount;
    double TIRatio             = static_cast<double>(TIAttrsCount) / specBefore.allAttrsCount;
    double SPRatio             = static_cast<double>(reducedAttrsCount - TIAttrsCount) / specBefore.allAttrsCount;
    auto double2percent        = [](double num) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (num * 100) << "%";
        return oss.str();
    };

    nlohmann::json before = {
        {"DI Size(KB)", this->coreInfo.inputDWARFSize / 1024},
        {"P(T)", this->dwarfObject.infoObject->getSpec().allAttrsCount},
    };

    nlohmann::json after = {
        {"DI Size(KB)", this->coreInfo.outputDWARFSize / 1024},
        {"P(T)", this->linkerOpt->getSpec().allAttrsCount},
    };
    nlohmann::json memcost = {
        {"MaxMemUsage", this->coreInfo.maxMemUsage},
    };

    nlohmann::json content = {
        {"before", before},
        {"after", after},
        {"memcost", memcost},
    };

    return content;
}

void DIReducerContext::saveJsonToFile(const std::string &targetFilePath, const nlohmann::json &jsonObject) {
    fs::path targetpath = fs::path(targetFilePath);
    if(fs::exists(targetpath) && fs::is_directory(targetpath))
        FIG_PANIC("\"", targetpath.string(), "\"", " is a directory!");
    fs::path parentPath = targetpath.parent_path();
    if(fs::exists(parentPath) && !fs::is_directory(parentPath))
        FIG_PANIC("Can not create dir ", "\"", parentPath.string(), "\"");
    fs::create_directories(parentPath);

    std::ofstream dos(targetpath);
    if(!dos.is_open())
        FIG_PANIC("Can not open file ", "\"", targetpath.string(), "\"");
    dos << jsonObject.dump(2);
    dos.close();
}

void DIReducerContext::convertCGToDot(const std::string &targetDir) {
    fs::path path = fs::path(targetDir);
    if(fs::exists(path) && !fs::is_directory(path))
        FIG_PANIC("can not create dir ", "\"", path.string(), "\"");
    else
        fs::create_directories(path);

    std::string file_name = fs::path(this->elfObject.filePath).filename();
    fs::path dotFilePath  = path / fs::path(file_name + ".cg.dot");
    std::ofstream dos(dotFilePath);
    if(!dos.is_open()) FIG_PANIC("No File!");
    this->cg.toDot(dos);
    dos.close();
}

void DIReducerContext::convertCFGToDot(const std::string &targetDir) {
    fs::path path = fs::path(targetDir);
    if(fs::exists(path) && !fs::is_directory(path))
        FIG_PANIC("can not create dir ", "\"", path.string(), "\"");
    else
        fs::create_directories(path);
    std::string file_name = fs::path(this->elfObject.filePath).filename();
    path /= file_name;
    if(fs::exists(path) && !fs::is_directory(path))
        FIG_PANIC("can not create dir ", "\"", path.string(), "\"");
    else
        fs::create_directory(path);

    for(auto &fMap : this->funcDataBase) {
        for(auto &it : fMap) {
            auto &f = it.second;
            if(!f->hasCFG())
                FIG_PANIC("No CFG!");
            std::ofstream os(path / fs::path(f->dwarfName + ".dot"));
            if(!os.is_open())
                FIG_PANIC("No File!");
            os << "digraph \"CFG for '" << f->cfg->name << "' function\" {" << std::endl;
            os << "label=\"CFG for '" << f->cfg->name << "' function\";" << std::endl;
            for(auto &it : f->cfg->blocks) {
                auto &block = it.second;
                os << "Block" << block->sn;
                os << "[shape=record, color=\"#3d50c3ff\", style=filled, fillcolor=\"#b9d0f970\",";
                if(block->type == CFG::BLOCK_TYPE::ENTRY_BLOCK)
                    os << "label=\"{" << f->cfg->name << "::ENTRY"
                       << "\\l ";
                else if(block->type == CFG::BLOCK_TYPE::EXIT_BLOCK)
                    os << "label=\"{" << f->cfg->name << "::EXIT"
                       << "\\l ";
                else {
                    if(this->lineTable.empty())
                        os << "label=\"{B" << block->sn << "\\l ";
                    else { // For tracking source file
                        os << "label=\"{B" << block->sn;
                        auto t = this->lineTable.find(block->startAddr);
                        os << "  [" << std::get<3>(t) << "  row:" << std::get<1>(t) << "  col:" << std::get<2>(t) << "]";
                        os << "\\l ";
                    }
                }

                for(auto &insn : block->statements) {
                    os << "0x" << dec_to_hex(insn.address) << ":   " << insn.mnemonic << "  " << insn.op_str << "\\l ";
                }
                os << "}\""
                   << "];" << std::endl;
                for(auto &e : block->forward_edges) {
                    if(e->type == CFG::EDGE_TYPE::CALL || e->type == CFG::EDGE_TYPE::RETURN)
                        continue;
                    os << "Block" << e->from->sn << " -> "
                       << "Block" << e->to->sn << ";\n";
                }
            }
            os << "}" << std::endl;

            os.close();
        }
    }
}

void DIReducerContext::initDebugStrTable() {
    if(!this->elfObject.is_section_exist(".debug_str"))
        return;
    if(this->config.is_dwarf_verbose)
        FIG_LOG("Initializing DebugStr Table.");

    dr_Elf64_Shdr shdr = this->elfObject.getSecByName(".debug_str");
    char *buffer       = this->elfObject.buffer.get();
    char *current      = buffer + shdr.shdr.sh_offset;
    this->strTable.init(current, shdr.shdr.sh_size);
}

void DIReducerContext::dump() {
    // ELF Header
    if(this->config.is_dwarf_verbose)
        FIG_LOG("Dumping To File Buffer.");
    std::memcpy(this->dstElfObject.buffer.get(), &this->dstElfObject.ehdr, sizeof(Elf64_Ehdr));
    // Program Header
    std::memcpy(
        this->dstElfObject.buffer.get() + this->dstElfObject.ehdr.e_phoff,
        this->dstElfObject.phdrs.data(),
        this->dstElfObject.phdrs.size() * sizeof(Elf64_Phdr));
    // mapped section
    uint64_t mappingEnd = static_cast<uint64_t>(this->dstElfObject.get_mapping_end());
    for(auto &dstShdr : this->dstElfObject.shdrs) {
        if(dstShdr.shdr.sh_offset + dstShdr.shdr.sh_size <= mappingEnd) {
            std::memcpy(
                this->dstElfObject.buffer.get() + dstShdr.shdr.sh_offset,
                this->elfObject.buffer.get() + dstShdr.shdr.sh_offset,
                dstShdr.shdr.sh_size);
        }
    }

    for(size_t i = 1; i < this->dstElfObject.shdrs.size(); i++) {
        // skip
        auto &curShdr = this->dstElfObject.shdrs[i];
        if(curShdr.shdr.sh_offset + curShdr.shdr.sh_size <= mappingEnd)
            continue;

        auto &preShdr          = this->dstElfObject.shdrs[i - 1];
        uint64_t offsetToWrite = 0;
        uint64_t sizeToWrite   = 0;
        if(preShdr.shdr.sh_type == SHT_NOBITS) {
            offsetToWrite = preShdr.shdr.sh_offset;
        } else {
            offsetToWrite = preShdr.shdr.sh_offset + preShdr.shdr.sh_size;
        }
        if(curShdr.shdr.sh_type == SHT_NOBITS) {
            sizeToWrite = 0;
        } else {
            sizeToWrite = curShdr.shdr.sh_size;
        }
        if(curShdr.shdr.sh_addralign == 0) {
            curShdr.shdr.sh_offset = offsetToWrite;
        } else {
            int64_t remainder = offsetToWrite % curShdr.shdr.sh_addralign;
            if(remainder == 0)
                curShdr.shdr.sh_offset = offsetToWrite;
            else
                curShdr.shdr.sh_offset = offsetToWrite + curShdr.shdr.sh_addralign - remainder;
        }

        if(curShdr.name == ".debug_info") {
            int64_t tmpOffset = 0;
            this->linkerOpt->dstInfoObject->write(this->dstElfObject.buffer.get() + curShdr.shdr.sh_offset, tmpOffset);
        } else if(curShdr.name == ".debug_abbrev") {
            int64_t tmpOffset = 0;
            this->linkerOpt->dstAbbrevObject->write(this->dstElfObject.buffer.get() + curShdr.shdr.sh_offset, tmpOffset);
        } else if(curShdr.name == ".debug_loclists") {
            int64_t tmpOffset = 0;
            this->dstDwarfObject.write_loclists(
                this->dstElfObject.buffer.get() + curShdr.shdr.sh_offset,
                tmpOffset);
        } else {
            auto &srcShdr = this->elfObject.getSecByName(curShdr.name);
            std::memcpy(
                this->dstElfObject.buffer.get() + curShdr.shdr.sh_offset,
                this->elfObject.buffer.get() + srcShdr.shdr.sh_offset,
                sizeToWrite);
        }
    }

    // section header
    int64_t sectionEnd = this->dstElfObject.shdrs.back().shdr.sh_offset + this->dstElfObject.shdrs.back().shdr.sh_size;
    for(size_t i = 0; i < this->dstElfObject.shdrs.size(); i++) {
        std::memcpy(
            this->dstElfObject.buffer.get() + sectionEnd + i * sizeof(Elf64_Shdr),
            &(this->dstElfObject.shdrs[i].shdr),
            sizeof(Elf64_Shdr));
    }

    // tune ELF Header
    Elf64_Ehdr *dst_ehdr = reinterpret_cast<Elf64_Ehdr *>(this->dstElfObject.buffer.get());
    dst_ehdr->e_shoff    = sectionEnd;
    dst_ehdr->e_shnum    = this->dstElfObject.shdrs.size();

    if(this->config.is_dwarf_verbose)
        FIG_LOG("Dumping To Real File.");
    fs::path dirPath = fs::path(this->config.dst_path).parent_path();
    if(!fs::exists(dirPath))
        std::filesystem::create_directories(dirPath);
    std::ofstream file(this->config.dst_path);
    if(!file.is_open())
        FIG_PANIC("Fail to open ", this->config.dst_path);
    int64_t fileEnd = sectionEnd + this->dstElfObject.shdrs.size() * sizeof(Elf64_Shdr);
    file.write(this->dstElfObject.buffer.get(), fileEnd);
    file.close();
}

void DIReducerContext::initLink() {
    this->linkerOpt->initSrcInfoObject(this->dwarfObject.infoObject);
    this->linkerOpt->initAbbrevObject();
    this->linkerOpt->initTypeCells(this->vTypeInfoDataBase);
    this->linkerOpt->startLink();
}

void DIReducerContext::initSrc() {
    this->elfObject.init(this->config.src_path);
    this->dwarfObject.init(this->elfObject);
    this->dwarfObject.init_abrrev();
    this->dwarfObject.init_info();
    this->dwarfObject.init_loclists();
}

void DIReducerContext::initDst() {
    this->dstElfObject.filePath = this->config.dst_path;
    uint64_t maxSize            = this->elfObject.size;
    this->dstElfObject.buffer.reset(new char[maxSize]);
    this->dstElfObject.size = maxSize;
    for(auto &phdr : this->elfObject.phdrs) {
        this->dstElfObject.phdrs.push_back(phdr);
    }
    std::set<std::string> remove_section_set = {
        ".debug_str",
        ".debug_aranges",
        ".debug_line",
        ".debug_line_str",
        ".debug_ranges",
        ".debug_rnglists",
        ".symtab",
        ".strtab",
    };
    for(auto &shdr : this->elfObject.shdrs) {
        if(remove_section_set.find(shdr.name) == remove_section_set.end()) {
            this->dstElfObject.shdrs.push_back(shdr);
        }
    }
    if(this->dstElfObject.is_section_exist(".symtab")) {
        FIG_ASSERT(this->dstElfObject.is_section_exist(".strtab"), ".symtab section exists but.strtab section does not exist.");
        int64_t symtab_index                                = this->dstElfObject.get_section_index_by_name(".symtab");
        int64_t strtab_index                                = this->dstElfObject.get_section_index_by_name(".strtab");
        this->dstElfObject.shdrs[symtab_index].shdr.sh_link = strtab_index;
    }

    this->dstElfObject.ehdr         = this->elfObject.ehdr;
    this->dstElfObject.ehdr.e_shnum = this->dstElfObject.shdrs.size();
    FIG_ASSERT(this->dstElfObject.is_section_exist(".shstrtab"), ".shstrtab section does not exist.");
    this->dstElfObject.ehdr.e_shstrndx         = this->dstElfObject.get_section_index_by_name(".shstrtab");
    this->dstElfObject.ehdr.e_shoff            = this->elfObject.ehdr.e_shoff;
    this->dstElfObject.section_mapping_segment = this->elfObject.section_mapping_segment;
    if(this->config.is_dwarf_verbose)
        FIG_LOG("Initailizing DWARF Data.");
    this->dstDwarfObject.init(this->elfObject);
    this->dstDwarfObject.init_abrrev();
    this->dstDwarfObject.init_info();
    this->dstDwarfObject.init_loclists();
}

nlohmann::json DIReducerContext::getVTypeInfoJson() {
    nlohmann::json content = nlohmann::json::array();
    for(auto &vMap : this->vTypeInfoDataBase) {
        for(auto &it : vMap) {
            content.push_back(it.second->to_json());
        }
    }

    return content;
}

nlohmann::json DIReducerContext::getFuncTypeMapJson() {
    nlohmann::json content;
    for(const auto &[addr, vType] : *this->customFuncMap.get()) {
        content[dec_to_hex(addr)] = vType->dwarfName;
    }

    return content;
}

void DIReducerContext::initSPA() {
    this->initDebugStrTable();
    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing Type Information.");
    if(this->config.is_analysis_verbose)
        FIG_LOG("- initGTypeInfo.");
    this->initGTypeInfo();
    if(this->config.is_analysis_verbose)
        FIG_LOG("- initVTypeInfo.");
    this->initVTypeInfo();
    if(this->config.is_analysis_verbose)
        FIG_LOG("- initVTypeFactory.");
    this->initVTypeFactory();

    this->coreInfo.DISStartTime = std::chrono::steady_clock::now();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing DynLib Function Location.");
    this->initDynLibLoader();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing DynLib Function Database.");
    this->initDynLibDataBase();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Constructing Local DynLib Function Database.");
    this->initDynLibFuncMap();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Collecting Abstract Variables.");
    this->initAbsVarInfo();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Collecting Abstract Functions.");
    this->initAbsFuncInfo();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing Global Variables.");
    this->initGlobalVars();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing Func Desc.");
    this->initFuncDesc();
    this->initCustomFuncMap();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Initializing Func Data.");
    this->initFuncASM();

    this->cfgInfo.startTime = std::chrono::steady_clock::now();
    if(this->config.is_analysis_verbose)
        FIG_LOG("Constructing CFG of Functions.");
    this->initFuncCFG();

    if(this->config.is_analysis_verbose)
        FIG_LOG("Constructing CFG of Program.");
    this->initProgramCFG();

    this->coreInfo.DISEndTime = std::chrono::steady_clock::now();

    this->cfgInfo.endTime         = std::chrono::steady_clock::now();
    this->cfgInfo.timeCost        = getTimeByus(this->cfgInfo.startTime, this->cfgInfo.endTime);
    this->cfgInfo.name            = fs::path(this->elfObject.filePath).filename();
    this->cfgInfo.size            = getFileSize(this->elfObject.filePath);
    this->cfgInfo.numOfInsn       = 0;
    this->cfgInfo.numOfFunc       = 0;
    this->cfgInfo.numOfEdge       = 0;
    this->cfgInfo.numOfBasciBlock = 0;
    for(auto &fMap : this->funcDataBase) {
        this->cfgInfo.numOfFunc = this->cfgInfo.numOfFunc + fMap.size();
        for(const auto &[_, f] : fMap) {
            this->cfgInfo.numOfInsn += f->numOfInsn;
            this->cfgInfo.numOfBasciBlock += f->cfg->blocks.size() - 2;
            for(const auto &[_, block] : f->cfg->blocks) {
                this->cfgInfo.numOfEdge += block->forward_edges.size();
            }
        }
    }
}

void DIReducerContext::initAbsVarInfo() {
    for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
        this->initAbsVarInfoInCompilationUnit(cu);
    }
}

void DIReducerContext::initAbsVarInfoInCompilationUnit(CompilationUnit &cu) {
    this->initAbsVarInfoInInfoEntry(cu.infoEntry);
}

void DIReducerContext::initAbsVarInfoInInfoEntry(InfoEntry &infoEntry) {
    if(infoEntry.isNull())
        return;

    if(DWARF::isAbsVarAbbrev(infoEntry.abbrevItem)) {
        uint64_t type          = 0;
        std::string name       = "unnamedVariable";
        auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter = infoEntry.attrBuffers.begin();
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                type = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_name)
                name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        this->absVarsInfo.insert({infoEntry.get_global_offset(), {name, type}});
    }

    for(auto &child : infoEntry.children) {
        this->initAbsVarInfoInInfoEntry(child);
    }
}

void DIReducerContext::initAbsFuncInfo() {
    for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
        this->initAbsFuncInfoInCompilationUnit(cu);
    }
}

void DIReducerContext::initAbsFuncInfoInCompilationUnit(CompilationUnit &cu) {
    this->initAbsFuncInfoInInfoEntry(cu.infoEntry);
}

void DIReducerContext::initAbsFuncInfoInInfoEntry(InfoEntry &infoEntry) {
    if(infoEntry.isNull())
        return;

    if(DWARF::isAbsFuncAbbrev(infoEntry.abbrevItem)) {
        uint64_t retType = 0;
        std::string dwarfName;
        auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter = infoEntry.attrBuffers.begin();
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name || abbrevEntryIter->at == DW_AT_linkage_name)
                dwarfName = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_type)
                retType = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        this->absFuncsInfo.insert({infoEntry.get_global_offset(), {dwarfName, retType}});
    }

    for(auto &child : infoEntry.children) {
        this->initAbsFuncInfoInInfoEntry(child);
    }
}

void DIReducerContext::initDynLibLoader() {
    char *elfBuffer = this->elfObject.buffer.get();
    {
        // parse .dynstr
        dr_Elf64_Shdr dynstrShdr = this->elfObject.getSecByName(".dynstr");
        char *dynstrBuffer       = elfBuffer + dynstrShdr.shdr.sh_offset;
        this->dynLibLoader.initDynstrTable(dynstrBuffer, dynstrShdr.shdr.sh_size);
    }
    {
        // parse .plt
        dr_Elf64_Shdr PLTShdr = this->elfObject.getSecByName(".plt");
        char *pltBuffer       = elfBuffer + PLTShdr.shdr.sh_offset;
        this->dynLibLoader.initPLTObject(pltBuffer, PLTShdr.shdr.sh_offset, PLTShdr.shdr.sh_size);
    }
    {
        // parse .plt.sec
        dr_Elf64_Shdr PLTSECShdr = this->elfObject.getSecByName(".plt.sec");
        char *pltSecBuffer       = elfBuffer + PLTSECShdr.shdr.sh_offset;
        this->dynLibLoader.initPLTSECObject(pltSecBuffer, PLTSECShdr.shdr.sh_offset, PLTSECShdr.shdr.sh_size);
    }
    {
        // parse .rela.plt
        dr_Elf64_Shdr relaPLTShdr = this->elfObject.getSecByName(".rela.plt");
        char *relaPLTBuffer       = elfBuffer + relaPLTShdr.shdr.sh_offset;
        this->dynLibLoader.initRelaPLTObject(relaPLTBuffer, relaPLTShdr.shdr.sh_size);
    }
    {
        // parse .dynsym
        dr_Elf64_Shdr dynsymShdr = this->elfObject.getSecByName(".dynsym");
        char *dynsymBuffer       = elfBuffer + dynsymShdr.shdr.sh_offset;
        this->dynLibLoader.initDynsymObject(dynsymBuffer, dynsymShdr.shdr.sh_size);
    }
    this->dynLibLoader.initLibFuncAddrInfo();
    // For Debugging
    // std::cout << this->libLoader.toJson().dump(2) << std::endl;
}

void DIReducerContext::initDynLibFuncMap() {
    this->dynLibFuncMap                = std::make_shared<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>>();
    const auto &dynLibLocalFuncAddrMap = this->dynLibLoader.dynLibFuncAddrMap;
    const auto &dynLibGlobalDataBase   = this->dynLibDataBase;
    for(const auto &[name, addr] : dynLibLocalFuncAddrMap) {
        if(!dynLibGlobalDataBase.contains(name))
            continue;
        this->dynLibFuncMap->insert({addr, dynLibGlobalDataBase.at(name)});
    }
}

void DIReducerContext::initDynLibDataBase() {
    using CFG::Var;
    using CFG::Func;
    using factory          = CFG::VTypeFactory;
    const auto &baseT      = factory::baseVType;
    const auto voidT       = baseT.bt_void;
    const auto intT        = baseT.bt_int32;
    const auto uintT       = baseT.bt_uint32;
    const auto sizeT       = baseT.bt_uint64;
    const auto signedsizeT = baseT.bt_int64;
    const auto offT        = baseT.bt_uint64;
    const auto longIntT    = baseT.bt_uint64;
    const auto charT       = baseT.bt_uint8;
    const auto wcharT      = baseT.bt_int32;
    const auto clockT      = baseT.bt_uint64;
    const auto doubleT     = baseT.bt_float64;
    const auto floatT      = baseT.bt_float32;
    const auto uidT        = baseT.bt_uint32;
    const auto gidT        = baseT.bt_uint32;
    const auto timeT       = baseT.bt_int64;
    const auto modeT       = baseT.bt_uint32;
    const auto devT        = baseT.bt_uint32;
    const auto wintT       = baseT.bt_uint32;
    const auto wctypeT     = baseT.bt_uint64;
    const auto clockidT    = baseT.bt_int32;
    const auto uintMaxT    = baseT.bt_uint64;
    const auto intMaxT     = baseT.bt_int64;
    const auto sizePtrT    = factory::getPointerType(sizeT);
    const auto timePtrT    = factory::getPointerType(timeT);
    const auto voidPtrT    = factory::getPointerType(voidT);
    const auto intPtrT     = factory::getPointerType(intT);
    const auto charPtrT    = factory::getPointerType(charT);
    const auto wcharPtrT   = factory::getPointerType(wcharT);

#define V(NAME, TYPE) \
    { NAME, TYPE }
#define VEC(...) std::vector<Var>({__VA_ARGS__})
#define E(NAME, VECTOR, RETTYPE) \
    {NAME, std::make_shared<Func>(NAME, VECTOR, RETTYPE)}
#define INSERT(entry) this->dynLibDataBase.insert(entry)

    INSERT(E("memcpy", VEC(V("to", voidPtrT), V("from", voidPtrT), V("size", sizeT)), voidPtrT));
    INSERT(E("strtoul", VEC(V("string", charPtrT), V("tailptr", charPtrT), V("base", sizeT)), sizeT));
    INSERT(E("lstat", VEC(V("filename,", charPtrT), V("buf", voidPtrT)), intT)); // buf
    INSERT(E("realloc", VEC(V("ptr", voidPtrT), V("newisze", sizeT)), voidPtrT));
    INSERT(E("access", VEC(V("filename,", charPtrT), V("how", intT)), intT));
    INSERT(E("strcspn", VEC(V("string", charPtrT), V("stopset", charPtrT)), sizeT));
    INSERT(E("closedir", VEC(V("dirstream,", voidPtrT)), intT));                  // dirstream
    INSERT(E("getrlimit", VEC(V("resource", intT), V("rlp", voidPtrT)), intT));   // rlp
    INSERT(E("getrlimit64", VEC(V("resource", intT), V("rlp", voidPtrT)), intT)); // rlp
    INSERT(E("strncpy", VEC(V("to", charPtrT), V("from", charPtrT), V("size", sizeT)), charPtrT));
    INSERT(E("freopen", VEC(V("filename", charPtrT), V("opentype", charPtrT), V("stream", voidPtrT)), voidPtrT)); // stream, ret
    INSERT(E("fclose", VEC(V("stream", voidPtrT)), intT));                                                        // stream
    INSERT(E("strncasecmp", VEC(V("s1", charPtrT), V("s2", charPtrT), V("n", sizeT)), intT));
    INSERT(E("strdup", VEC(V("s", charPtrT)), charPtrT));
    INSERT(E("feof", VEC(V("stream", voidPtrT)), intT)); // stream
    INSERT(E("__errno_location", VEC(), intPtrT));
    INSERT(E("memcmp", VEC(V("a1", voidPtrT), V("a2", voidPtrT), V("size", sizeT)), intT));
    INSERT(E("getenv", VEC(V("name", charPtrT)), charPtrT));
    INSERT(E("fread", VEC(V("data", voidPtrT), V("size", sizeT), V("count", sizeT), V("stream", voidPtrT)), sizeT));          // stream
    INSERT(E("fread_unlocked", VEC(V("data", voidPtrT), V("size", sizeT), V("count", sizeT), V("stream", voidPtrT)), sizeT)); // stream
    INSERT(E("read", VEC(V("filedes", intT), V("buffer", voidPtrT), V("size", sizeT)), sizeT));
    INSERT(E("strrchr", VEC(V("string", charPtrT), V("c", intT)), charPtrT));
    INSERT(E("open", VEC(V("filename", charPtrT), V("flags", intT)), intT));
    INSERT(E("strstr", VEC(V("haystack", charPtrT), V("needle", charPtrT)), charPtrT));
    INSERT(E("fstat", VEC(V("filedes", intT), V("buf", voidPtrT)), intT));       // buf
    INSERT(E("fstat64", VEC(V("filedes", intT), V("buf", voidPtrT)), intT));     // buf
    INSERT(E("fflush", VEC(V("stream", voidPtrT)), intT));                       // stream
    INSERT(E("gettimeofday", VEC(V("tp", voidPtrT), V("tzp", voidPtrT)), intT)); // tp
    INSERT(E("ftell", VEC(V("stream", voidPtrT)), offT));                        // stream
    INSERT(E("getcwd", VEC(V("buffer", charPtrT), V("size", sizeT)), charPtrT));
    INSERT(E("puts", VEC(V("s", charPtrT)), intT));
    INSERT(E("strncmp", VEC(V("s1", charPtrT), V("s2", charPtrT), V("n", sizeT)), intT));
    INSERT(E("fileno", VEC(V("stream", voidPtrT)), intT));
    INSERT(E("fputs", VEC(V("s", charPtrT), V("stream", voidPtrT)), intT));       // stream
    INSERT(E("getrusage", VEC(V("process", intT), V("rusage", voidPtrT)), intT)); // rusage
    INSERT(E("ferror", VEC(V("stream", voidPtrT)), intT));
    INSERT(E("close", VEC(V("filedes", intT)), intT));
    INSERT(E("sysconf", VEC(V("parameter", intT)), longIntT));
    INSERT(E("strcat", VEC(V("to", charPtrT), V("from", charPtrT)), charPtrT));
    INSERT(E("frexp", VEC(V("value", doubleT), V("exponent", intPtrT)), doubleT));
    INSERT(E("frexpf", VEC(V("value", floatT), V("exponent", intPtrT)), floatT));
    INSERT(E("readdir", VEC(V("dirstream", voidPtrT)), voidPtrT));
    INSERT(E("exit", VEC(V("status", intT)), voidT));
    INSERT(E("strncat", VEC(V("to", charPtrT), V("from", charPtrT), V("size", sizeT)), charPtrT));
    INSERT(E("ldexp", VEC(V("value", doubleT), V("exponent", intT)), doubleT));
    INSERT(E("ldexpf", VEC(V("value", floatT), V("exponent", intT)), floatT));
    INSERT(E("getpid", VEC(), intT));
    INSERT(E("getppid", VEC(), intT));
    INSERT(E("gettid", VEC(), intT));
    INSERT(E("memmove", VEC(V("to", voidPtrT), V("from", voidPtrT), V("size", sizeT)), voidPtrT));
    INSERT(E("getc", VEC(V("stream", voidPtrT)), intT));                                            // stream
    INSERT(E("fseek", VEC(V("stream", voidPtrT), V("offset", longIntT), V("whence", intT)), intT)); // stream
    INSERT(E("malloc", VEC(V("size", sizeT)), voidPtrT));
    INSERT(E("putchar", VEC(V("c", intT)), intT));
    INSERT(E("abort", VEC(), voidT));
    INSERT(E("fopen", VEC(V("filename", charPtrT), V("opentype", charPtrT)), voidPtrT)); // ret
    INSERT(E("memset", VEC(V("block", voidPtrT), V("c", intT), V("size", sizeT)), voidPtrT));
    INSERT(E("setrlimit", VEC(V("resource", intT), V("rlp", voidPtrT)), intT));   // rlp
    INSERT(E("setrlimit64", VEC(V("resource", intT), V("rlp", voidPtrT)), intT)); // rlp
    INSERT(E("memchr", VEC(V("block", voidPtrT), V("c", intT), V("size", sizeT)), voidPtrT));
    INSERT(E("asctime", VEC(V("brokentime", voidPtrT)), charPtrT));                                                   // brokentime
    INSERT(E("asctime_r", VEC(V("brokentime", voidPtrT), V("buffer", charPtrT)), charPtrT));                          // brokentime
    INSERT(E("_obstack_newchunk", VEC(V("obstack", voidPtrT), V("a", intT)), voidT));                                 // obstack
    INSERT(E("ungetc", VEC(V("c", intT), V("stream", voidPtrT)), intT));                                              // stream
    INSERT(E("fwrite", VEC(V("data", voidPtrT), V("size", sizeT), V("count", sizeT), V("stream", voidPtrT)), sizeT)); // stream
    INSERT(E("fputc", VEC(V("c", intT), V("stream", voidPtrT)), intT));                                               // stream
    INSERT(E("strcasecmp", VEC(V("s1", charPtrT), V("s2", charPtrT)), intT));
    INSERT(E("times", VEC(V("buffer", voidPtrT)), clockT));
    INSERT(E("strcpy", VEC(V("to", charPtrT), V("from", charPtrT)), charPtrT));
    INSERT(E("__stack_chk_fail", VEC(), voidT));
    INSERT(E("bsearch", VEC(V("key", voidPtrT), V("array", voidPtrT), V("count", sizeT), V("size", sizeT), V("compara", voidPtrT)), voidT)); // compara
    INSERT(E("_obstack_memory_used", VEC(V("h", voidPtrT)), sizeT));                                                                         // h
    INSERT(E("strlen", VEC(V("s", charPtrT)), sizeT));
    INSERT(E("atoi", VEC(V("string", charPtrT)), intT));
    INSERT(E("opendir", VEC(V("dirname", charPtrT)), voidPtrT));               // ret
    INSERT(E("stat", VEC(V("filename", charPtrT), V("buf", voidPtrT)), intT)); // buf
    INSERT(E("raise", VEC(V("signum", intT)), intT));
    INSERT(E("__ctype_b_loc", VEC(), factory::getPointerType(factory::getPointerType(baseT.bt_uint16))));
    INSERT(E("fdopen", VEC(V("filedes", intT), V("opentype", charPtrT)), voidPtrT)); // ret
    INSERT(E("lseek", VEC(V("filedes", intT), V("offset", offT), V("whence", intT)), offT));
    INSERT(E("time", VEC(V("result", timePtrT)), timeT));
    INSERT(E("signal", VEC(V("signum", intT), V("action", voidPtrT)), voidPtrT));        // action, ret
    INSERT(E("obstack_free", VEC(V("obstack", voidPtrT), V("block", voidPtrT)), voidT)); // obstack
    INSERT(E("strsignal", VEC(V("signum", intT)), charPtrT));
    INSERT(E("unlink", VEC(V("filename", charPtrT)), intT));
    INSERT(E("putc", VEC(V("c", intT), V("stream", voidPtrT)), intT)); // stream
    INSERT(E("strspn", VEC(V("string", charPtrT), V("skipset", charPtrT)), sizeT));
    INSERT(E("localtime", VEC(V("time", timePtrT)), voidPtrT)); // ret
    INSERT(E("setbuf", VEC(V("stream", voidPtrT), V("buf", charPtrT)), voidPtrT));
    INSERT(E("strchr", VEC(V("string", charPtrT), V("c", intT)), charPtrT));
    INSERT(E("strtoumax", VEC(V("string", charPtrT), V("tailptr", factory::getPointerType(charPtrT)), V("base", intT)), uintMaxT));
    INSERT(E("ferror_unlocked", VEC(V("stream", voidPtrT)), intT));                                                // stream
    INSERT(E("setvbuf", VEC(V("stream", voidPtrT), V("buf", charPtrT), V("mode", intT), V("size", sizeT)), intT)); // stream
    INSERT(E("bindtextdomain", VEC(V("domainname", charPtrT), V("dirname", charPtrT)), charPtrT));
    INSERT(E("__fpending", VEC(V("stream", voidPtrT)), sizeT));                      // stream
    INSERT(E("putc_unlocked", VEC(V("c", intT), V("stream", voidPtrT)), intT));      // stream
    INSERT(E("fputs_unlocked", VEC(V("s", charPtrT), V("stream", voidPtrT)), intT)); // stream
    INSERT(E("fputc_unlocked", VEC(V("c", intT), V("stream", voidPtrT)), intT));     // stream
    INSERT(E("free", VEC(V("ptr", voidPtrT)), voidPtrT));
    INSERT(E("_exit", VEC(V("status", intT)), voidPtrT));
    INSERT(E("textdomain", VEC(V("domainname", charPtrT)), charPtrT));
    INSERT(E("gettext", VEC(V("msgid", charPtrT)), charPtrT));
    INSERT(E("tolower", VEC(V("c", intT)), intT));
    INSERT(E("_tolower", VEC(V("c", intT)), intT));
    INSERT(E("toupper", VEC(V("c", intT)), intT));
    INSERT(E("_toupper", VEC(V("c", intT)), intT));
    INSERT(E("toascii", VEC(V("c", intT)), intT));
    INSERT(E("strcmp", VEC(V("s1", charPtrT), V("s2", charPtrT)), intT));
    INSERT(E("getline", VEC(V("lineptr", factory::getPointerType(charPtrT)), V("n", sizePtrT), V("stream", voidPtrT)), signedsizeT)); // stream
    INSERT(E("__cxa_atexit", VEC(V("actione", voidPtrT), V("arg", voidPtrT), V("dso_handle", voidPtrT)), intT));                      // action
    INSERT(E("calloc", VEC(V("count", sizeT), V("eltsize", sizeT)), voidPtrT));
    INSERT(E("nl_langinfo", VEC(V("item", sizeT)), charPtrT)); // nl_item equal to size_t
    INSERT(E("wctype", VEC(V("property", charPtrT)), wctypeT));
    INSERT(E("iswlower", VEC(V("wc", wintT)), intT));
    INSERT(E("iswprint", VEC(V("wc", wintT)), intT));
    INSERT(E("iswpunct", VEC(V("wc", wintT)), intT));
    INSERT(E("iswspace", VEC(V("wc", wintT)), intT));
    INSERT(E("iswupper", VEC(V("wc", wintT)), intT));
    INSERT(E("iswxdigit", VEC(V("wc", wintT)), intT));
    INSERT(E("iswblank", VEC(V("wc", wintT)), intT));
    INSERT(E("iswcntrl", VEC(V("wc", wintT)), intT));
    INSERT(E("iswdigit", VEC(V("wc", wintT)), intT));
    INSERT(E("iswgraph", VEC(V("wc", wintT)), intT));
    INSERT(E("iswctype", VEC(V("wc", wintT), V("desc", wctypeT)), intT));
    INSERT(E("iswalnum", VEC(V("wc", wintT)), intT));
    INSERT(E("iswalpha", VEC(V("wc", wintT)), intT));
    INSERT(E("mbsinit", VEC(V("ps", voidPtrT)), intT)); // ps
    INSERT(E("putchar_unlocked", VEC(V("c", wintT)), intT));
    INSERT(E("fwrite_unlocked", VEC(V("data", voidPtrT), V("size", sizeT), V("count", sizeT), V("stream", voidPtrT)), sizeT)); // stream
    INSERT(E("setlocale", VEC(V("category", intT), V("locale", charPtrT)), charPtrT));
    INSERT(E("fseeko", VEC(V("stream", voidPtrT), V("offset", offT), V("whence", intT)), intT)); // stream
    INSERT(E("__assert_fail", VEC(V("assertion", charPtrT), V("file", charPtrT), V("line", uintT), V("function", charPtrT)), voidT));
    INSERT(E("wcrtomb", VEC(V("s", charPtrT), V("wc", wcharT), V("ps", voidPtrT)), sizeT)); // ps
    INSERT(E("btowc", VEC(V("c", intT)), wintT));
    INSERT(E("mbrtowc", VEC(V("pwc", wcharPtrT), V("s", charPtrT), V("n", sizeT), V("ps", voidPtrT)), sizePtrT)); // ps
    INSERT(E("__freadable", VEC(V("stream", voidPtrT)), intT));                                                   // stream
    INSERT(E("__fwritable", VEC(V("stream", voidPtrT)), intT));                                                   // stream
    INSERT(E("__freading", VEC(V("stream", voidPtrT)), intT));                                                    // stream
    INSERT(E("__fwriting", VEC(V("stream", voidPtrT)), intT));                                                    // stream
    INSERT(E("__stack_chk_fail", VEC(), voidT));
    INSERT(E("getopt_long", VEC(V("argc", intT), V("argv", charPtrT), V("shortopts", charPtrT), V("longopts", voidPtrT), V("indexptr", intPtrT)), intT)); // longopts
    INSERT(E("strtoimax", VEC(V("string", charPtrT), V("tailptr", factory::getPointerType(charPtrT)), V("base", intT)), intMaxT));
    INSERT(E("strcoll", VEC(V("s1", charPtrT), V("s2", charPtrT)), intT));
    INSERT(E("wstrcoll", VEC(V("w1", wcharPtrT), V("w2", wcharPtrT)), intT));
    INSERT(E("towlower", VEC(V("wc", wintT)), wintT));
    INSERT(E("towupper", VEC(V("wc", wintT)), wintT));
    INSERT(E("getpagesize", VEC(), intT));
    INSERT(E("write", VEC(V("filedes", intT), V("buffer", charPtrT), V("size", sizeT)), signedsizeT));
    INSERT(E("rpmatch", VEC(V("response", charPtrT)), intT));
    INSERT(E("canonicalize_file_name", VEC(V("name", charPtrT)), charPtrT));
    INSERT(E("clock_gettime", VEC(V("clock", clockidT), V("ts", voidPtrT)), intT)); // ts
    INSERT(E("endgrent", VEC(), voidT));
    INSERT(E("mkfifo", VEC(V("filename", charPtrT), V("mode", modeT)), intT));
    INSERT(E("mknod", VEC(V("filename", charPtrT), V("mode", modeT), V("dev", devT)), intT));
    INSERT(E("readlink", VEC(V("filename", charPtrT), V("buffer", charPtrT), V("size", sizeT)), signedsizeT));
    INSERT(E("qsort", VEC(V("array", voidPtrT), V("count", sizeT), V("size", sizeT), V("compare", voidPtrT)), voidT)); // compare
    INSERT(E("getuid", VEC(), uidT));
    INSERT(E("getgid", VEC(), gidT));
    INSERT(E("geteuid", VEC(), uidT));
    INSERT(E("getegid", VEC(), gidT));
    INSERT(E("getgroups", VEC(V("count", intT), V("groups", factory::getPointerType(gidT))), intT));
    INSERT(E("umask", VEC(V("mask", modeT)), modeT));
    INSERT(E("getmask", VEC(), modeT));
    INSERT(E("rmdir", VEC(V("filename", charPtrT)), intT));
    INSERT(E("closedir", VEC(V("dirstream", voidPtrT)), intT));                       // dirstream
    INSERT(E("utimes", VEC(V("filename", charPtrT), V("timeval", voidPtrT)), intT));  // timeval
    INSERT(E("lutimes", VEC(V("filename", charPtrT), V("timeval", voidPtrT)), intT)); // timeval
    INSERT(E("futimes", VEC(V("fd", intT), V("timeval", voidPtrT)), intT));           // timeval
    INSERT(E("fdopendir", VEC(V("fd", intT)), voidPtrT));                             // ret
    INSERT(E("dirfd", VEC(V("dirstream", voidPtrT)), intT));                          // dirstream
    INSERT(E("fpathconf", VEC(V("filedes", intT), V("parameter", intT)), longIntT));
    INSERT(E("getgrnam", VEC(V("name", charPtrT)), voidPtrT)); // ret
    INSERT(E("explicit_bzero", VEC(V("block", voidPtrT), V("len", sizeT)), voidT));

#undef INSERT
#undef ELE
#undef VEC
#undef V
}

// void DIReducerContext::reduce() {
//     // [压缩]
//     // 压缩compilation_unit,删除内部一些不需要的属性,同时会删除abbrev中对应的Attr
//     if(this->config.is_dwarf_verbose)
//         FIG_LOG("Reducing information in Compilation Unit.");
//     for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
//         this->reduceCompilationUnit(cu);
//     }

//     // [修正]
//     // 1.abbrevlist Map中的偏移量
//     // 2.abbrevlist中的偏移量
//     // 3.compilation_unit中对应的abbrevlist偏移量
//     // 4.section header中size属性
//     // 5.compilation_unit以及info_entry中的offset属性,同时填充重定位表
//     // 6.DIE中引用类型的值
//     // 7.DIE使用的loclists部分的数据

//     // [修正]1.abbrevlist Map中的偏移量 && 2.abbrevlist中的偏移量
//     this->dstDwarfObject.abbrevObject->fix_offset();
//     // [修正]3.compilation_unit中的偏移量
//     if(this->config.is_dwarf_verbose)
//         FIG_LOG("Fixing offsets in Compilation Unit.");
//     for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
//         cu.abbrevOffset = cu.abbrevList->offset;
//     }
//     // [修正]4.section header中size属性
//     auto &abbrevShdr        = this->dstElfObject.getSecByName(".debug_abbrev");
//     abbrevShdr.shdr.sh_size = this->dstDwarfObject.abbrevObject->getSize();
//     auto &infoShdr          = this->dstElfObject.getSecByName(".debug_info");
//     infoShdr.shdr.sh_size   = this->dstDwarfObject.infoObject->getSize();
//     // [修正]5.compilation_unit以及info_entry中的offset属性,同时填充重定位表
//     if(this->config.is_dwarf_verbose)
//         FIG_LOG("Constructing Relocation Table.");
//     this->dstDwarfObject.infoObject->fix_offset(this->GlobalOffsetMap, this->DIEOffsetMapVec, this->CUOffsetMap);

//     // [重定位]1.DIE中引用类型的值
//     if(this->config.is_dwarf_verbose)
//         FIG_LOG("Fixing DIE Reference.");
//     this->dstDwarfObject.infoObject->fix_reference_die_attr(
//         this->GlobalOffsetMap, this->DIEOffsetMapVec, this->CUOffsetMap);
//     // [重定位]2.DIE使用的loclists部分的数据
//     if(this->dstDwarfObject.dwarfBuffer.loclistsBuffer.is_set()) {
//         if(this->config.is_dwarf_verbose)
//             FIG_LOG("Fixing DWARF Expression in loclists");
//         this->dstDwarfObject.infoObject->fix_reference_die_loclists(
//             this->GlobalOffsetMap, this->DIEOffsetMapVec, this->dstDwarfObject.loclistsObject);
//     }
// }

// void DIReducerContext::reduceCompilationUnit(CompilationUnit &cu) {
//     // 对info entry进行压缩
//     this->reduceInfoEntry(cu.infoEntry, cu.abbrevList);
//     // 修正cu的length属性
//     cu.length = cu.getSize() - cu.getLengthSize();
// }

// void DIReducerContext::reduceInfoEntry(InfoEntry &infoEntry, std::shared_ptr<AbbrevList> &abbrevList) {
//     if(infoEntry.isNull())
//         return;

//     // 用infoEntry.abbrevItem进行更新以及压缩(因为从abbrevList获取的abbrevItem可能被更新过了...)
//     auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
//     auto attrRawBufferIter = infoEntry.attrBuffers.begin();

//     if(this->redundantVarIdSet.contains(infoEntry.cuOffset + infoEntry.innerOffset)) {
//         // 根据redundantVarIdSet来删除冗余的变量信息
//         while(!abbrevEntryIter->isNull()) {
//             abbrevEntryIter   = infoEntry.abbrevItem.entries.erase(abbrevEntryIter);
//             attrRawBufferIter = infoEntry.attrBuffers.erase(attrRawBufferIter);
//         }
//     } else {
//         // 根据remove_attribute_set来删除不需要的Attr
//         while(!abbrevEntryIter->isNull()) {
//             if(this->remove_attribute_set.find(abbrevEntryIter->at) != this->remove_attribute_set.end()) {
//                 abbrevEntryIter   = infoEntry.abbrevItem.entries.erase(abbrevEntryIter);
//                 attrRawBufferIter = infoEntry.attrBuffers.erase(attrRawBufferIter);
//             } else {
//                 abbrevEntryIter   = std::next(abbrevEntryIter);
//                 attrRawBufferIter = std::next(attrRawBufferIter);
//             }
//         }
//     }

//     // 使用infoEntry.abbrevItem替换abbrevList中的abbrevItem(因为最后写入文件的abbrev数据来自abbrevList)
//     AbbrevItem &abbrevItemFromList = abbrevList->get_abbrev_item_by_number(infoEntry.abbrevNumber);
//     abbrevItemFromList             = infoEntry.abbrevItem;

//     // 递归处理子节点
//     for(auto &child : infoEntry.children) {
//         this->reduceInfoEntry(child, abbrevList);
//     }
// }

CFG::GTypeInfo DIReducerContext::getSingleType(const InfoEntry &infoEntry) {
    CFG::GTypeInfo g_type;
    g_type.dwTag           = infoEntry.abbrevItem.tag;
    g_type.ident           = infoEntry.innerOffset;
    g_type.attr_count      = infoEntry.get_all_attr_count();
    auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
    auto attrRawBufferIter = infoEntry.attrBuffers.begin();
    switch(g_type.dwTag) {
    case DW_TAG_unspecified_type: {
        // breaking
        // null => pointer
        g_type.size  = 8;
        g_type.dwTag = DW_TAG_pointer_type;
        g_type.name  = "null";
        break;
    }
    case DW_TAG_base_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name)
                g_type.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_byte_size)
                g_type.size = get_dw_at_byte_size(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_encoding)
                g_type.detail.base.encoding = get_dw_at_encoding(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
    case DW_TAG_ptr_to_member_type:
    case DW_TAG_rvalue_reference_type: {
        g_type.size = 8; // default pointer size
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_byte_size)
                g_type.size = get_dw_at_byte_size(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_type)
                g_type.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_const_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                g_type.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_typedef: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name)
                g_type.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_type)
                g_type.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_restrict_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                g_type.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_volatile_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                g_type.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    default: FIG_PANIC("unsupported dw_tag for single type"); break;
    }

    return g_type;
}

CFG::GTypeInfo DIReducerContext::getMultiType(const InfoEntry &infoEntry, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap) {
    CFG::GTypeInfo gTypeInfo;
    gTypeInfo.dwTag        = infoEntry.abbrevItem.tag;
    gTypeInfo.ident        = infoEntry.innerOffset;
    gTypeInfo.attr_count   = infoEntry.get_all_attr_count();
    auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
    auto attrRawBufferIter = infoEntry.attrBuffers.begin();
    switch(gTypeInfo.dwTag) {
    case DW_TAG_subroutine_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                gTypeInfo.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_array_type: {
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_type)
                gTypeInfo.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        for(auto &child : infoEntry.children) {
            if(child.abbrevItem.tag != DW_TAG_subrange_type)
                continue;
            auto childEntryIter  = child.abbrevItem.entries.begin();
            auto childBufferIter = child.attrBuffers.begin();
            // default: zero
            gTypeInfo.detail.array.arrayBound = 0;
            while(childEntryIter != child.abbrevItem.entries.end() && !childEntryIter->isNull()) {
                // DW_AT_upper_bound
                if(childEntryIter->at == DW_AT_upper_bound)
                    gTypeInfo.detail.array.arrayBound = get_dw_at_upper_bound(*childEntryIter, *childBufferIter) + 1;

                if(gTypeInfo.detail.array.arrayBound > CFG::MaxArrayBound)
                    gTypeInfo.detail.array.arrayBound = CFG::DefaultArrayBound;

                childEntryIter  = std::next(childEntryIter);
                childBufferIter = std::next(childBufferIter);
            }
        }
        break;
    }
    case DW_TAG_structure_type:
    case DW_TAG_class_type: {
        gTypeInfo.name = "AnonymousStructureType" + std::to_string(CFG::getUniqueID());
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name)
                gTypeInfo.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_byte_size)
                gTypeInfo.size = get_dw_at_byte_size(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        for(auto &child : infoEntry.children) {
            // case 1 : empty entry, the end of children
            if(child.isNull()) break;

            // case 2 : child.abbrevItem.tag != DW_TAG_member, maybe type define
            if(child.abbrevItem.tag != DW_TAG_member) {
                initGTypeInfoInInfoEntry(child, generalTypeMap);
                continue;
            }

            // case 3 : child.abbrevItem.tag == DW_TAG_member
            if(DWARF::isStructCommonMemberAbbrev(child.abbrevItem)) {
                auto childEntryIter     = child.abbrevItem.entries.begin();
                auto childBufferIter    = child.attrBuffers.begin();
                uint64_t memberType     = 0;
                uint32_t memberLocation = 0;
                while(childEntryIter != child.abbrevItem.entries.end() && !childEntryIter->isNull()) {
                    if(childEntryIter->at == DW_AT_type)
                        memberType = get_dw_at_type(*childEntryIter, *childBufferIter);
                    if(childEntryIter->at == DW_AT_data_member_location)
                        memberLocation = get_dw_at_data_member_location(*childEntryIter, *childBufferIter);
                    childEntryIter  = std::next(childEntryIter);
                    childBufferIter = std::next(childBufferIter);
                }
                gTypeInfo.detail.structType.memberTypeInfoVec.push_back(std::make_pair(std::make_pair(memberType, memberLocation), 0));
            } else if(DWARF::isStructBitMemberAbbrev(child.abbrevItem)) {
                auto childEntryIter        = child.abbrevItem.entries.begin();
                auto childBufferIter       = child.attrBuffers.begin();
                uint64_t memberType        = 0;
                uint32_t memberBitLocation = 0;
                uint32_t memberBitSize     = 0;
                while(childEntryIter != child.abbrevItem.entries.end() && !childEntryIter->isNull()) {
                    if(childEntryIter->at == DW_AT_type)
                        memberType = get_dw_at_type(*childEntryIter, *childBufferIter);
                    if(childEntryIter->at == DW_AT_data_bit_offset)
                        memberBitLocation = get_dw_at_data_bit_offset(*childEntryIter, *childBufferIter);
                    if(childEntryIter->at == DW_AT_bit_size)
                        memberBitSize = get_dw_at_bit_size(*childEntryIter, *childBufferIter);
                    childEntryIter  = std::next(childEntryIter);
                    childBufferIter = std::next(childBufferIter);
                }
                gTypeInfo.detail.structType.memberTypeInfoVec.push_back(std::make_pair(std::make_pair(memberType, memberBitLocation), memberBitSize));
            } else {
                FIG_PANIC("Can not identify Structure's member! struct name:", gTypeInfo.name);
            }
        }
        break;
    }
    case DW_TAG_enumeration_type: {
        gTypeInfo.name = "AnonymousEnumerationType" + std::to_string(CFG::getUniqueID());
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name)
                gTypeInfo.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_byte_size)
                gTypeInfo.size = get_dw_at_byte_size(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_type)
                gTypeInfo.reference = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        break;
    }
    case DW_TAG_union_type: {
        gTypeInfo.name = "AnonymousUnionType" + std::to_string(CFG::getUniqueID());
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name)
                gTypeInfo.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_byte_size)
                gTypeInfo.size = get_dw_at_byte_size(*abbrevEntryIter, *attrRawBufferIter);
            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }
        for(auto &child : infoEntry.children) {
            // case 1 : empty entry, the end of children
            if(child.isNull()) break;

            // case 2 : child.abbrevItem.tag != DW_TAG_member, maybe type define
            if(child.abbrevItem.tag != DW_TAG_member) {
                initGTypeInfoInInfoEntry(child, generalTypeMap);
                continue;
            }
            // case 3 : child.abbrevItem.tag == DW_TAG_member
            FIG_ASSERT(DWARF::isUnionMemberAbbrev(child.abbrevItem));
            auto childEntryIter  = child.abbrevItem.entries.begin();
            auto childBufferIter = child.attrBuffers.begin();
            uint64_t memberType  = 0;
            while(childEntryIter != child.abbrevItem.entries.end() && !childEntryIter->isNull()) {
                if(childEntryIter->at == DW_AT_type)
                    memberType = get_dw_at_type(*childEntryIter, *childBufferIter);
                childEntryIter  = std::next(childEntryIter);
                childBufferIter = std::next(childBufferIter);
            }
            gTypeInfo.detail.unionType.memberTypeInfoVec.push_back(memberType);
        }
        break;
    }
    default: FIG_PANIC("unsupported dw_tag for multi type"); break;
    }

    return gTypeInfo;
}

void DIReducerContext::initGTypeInfoInInfoEntry(const InfoEntry &infoEntry, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap) {
    if(infoEntry.isNull())
        return;

    if(DWARF::isSingleType(infoEntry.abbrevItem)) {
        CFG::GTypeInfo g_type = this->getSingleType(infoEntry);
        g_type.module         = this->gTypeInfoDataBase.size();
        generalTypeMap.insert(std::make_pair(g_type.ident, g_type));
        return;
    }

    if(DWARF::isMultiType(infoEntry.abbrevItem)) {
        CFG::GTypeInfo g_type = this->getMultiType(infoEntry, generalTypeMap);
        g_type.module         = this->gTypeInfoDataBase.size();
        generalTypeMap.insert(std::make_pair(g_type.ident, g_type));
        return;
    }

    for(auto &child : infoEntry.children) {
        this->initGTypeInfoInInfoEntry(child, generalTypeMap);
    }
}

void DIReducerContext::initGTypeInfoInCompilationUnit(CompilationUnit &cu, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap) {
    this->initGTypeInfoInInfoEntry(cu.infoEntry, generalTypeMap);
}

void DIReducerContext::initGTypeInfo() {
    for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
        this->gTypeInfoDataBase.push_back(std::unordered_map<uint64_t, CFG::GTypeInfo>());
        this->initGTypeInfoInCompilationUnit(cu, this->gTypeInfoDataBase.back());
    }
}

void DIReducerContext::initGlobalVars() {
    this->globalVars = std::make_shared<std::vector<std::shared_ptr<CFG::Var>>>();
    for(auto &cu : this->dstDwarfObject.infoObject.get()->compilationUnits) {
        this->initGlobalVarsInCompilationUnit(cu);
    }
}

void DIReducerContext::initGlobalVarsInCompilationUnit(CompilationUnit &cu) {
    const auto &TopEntry = cu.infoEntry;
    for(const auto &firstLevelEntry : TopEntry.children) {
        if(!DWARF::isVariableAbbrev(firstLevelEntry.abbrevItem))
            continue;

        auto v                 = std::make_shared<CFG::Var>();
        auto abbrevEntryIter   = firstLevelEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter = firstLevelEntry.attrBuffers.begin();
        uint64_t absVar        = 0;
        while(!abbrevEntryIter->isNull()) {
            uint64_t at   = abbrevEntryIter->at;
            uint64_t form = abbrevEntryIter->form;
            if(at == DW_AT_name)
                v->name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(at == DW_AT_type) {
                uint64_t type = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
                v->vType      = CFG::VTypeFactory::getVTypeByModuleOffset(cu.module, type);
            }
            if(at == DW_AT_abstract_origin || at == DW_AT_specification) {
                absVar = get_dw_at_abs_ori(*abbrevEntryIter, *attrRawBufferIter) + cu.offset;
            }
            if(at == DW_AT_location) {
                int64_t temp = 0;
                if(form != DW_FORM_exprloc)
                    FIG_PANIC("Unexpected!");
                uint8_t leb128u_size    = get_length_leb128(attrRawBufferIter->buffer.get(), temp);
                uint64_t expression_len = read_leb128u(attrRawBufferIter->buffer.get(), temp);
                DWARFExprParser parser;
                parser.init(
                    attrRawBufferIter->buffer.get() + leb128u_size,
                    expression_len,
                    firstLevelEntry.abbrevParser->bitWidth,
                    firstLevelEntry.abbrevParser->addressSize);
                auto &op_list = parser.op_list;
                if(op_list.size() == 1 && op_list[0].operation == DW_OP_fbreg) {
                    FIG_PANIC("Unexpected!");
                } else if(op_list.size() == 1 && op_list[0].operation == DW_OP_addr) {
                    v->addr.type       = CFG::ADDR_TYPE::GLOBAL;
                    v->addr.globalAddr = op_list[0].operand1.u;
                } else if(op_list.size() == 1 && op_list[0].operation >= DW_OP_reg0 && op_list[0].operation <= DW_OP_reg31) {
                    FIG_PANIC("Unexpected!");
                } else {
                    FIG_PANIC("Unexpected dwarf expression for describing the addr of variale. ", dw_op_to_str(op_list[0].operation));
                }
                break;
            }

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        if(absVar != 0) {
            if(!this->absVarsInfo.contains(absVar))
                FIG_PANIC("can not find abstract origin in absVarsInfo. ", "absOri: 0x", dec_to_hex(absVar));

            auto &absVarInfo = this->absVarsInfo[absVar];
            uint64_t type    = absVarInfo.second;
            v->vType         = CFG::VTypeFactory::getVTypeByModuleOffset(cu.module, type);
            v->name          = absVarInfo.first;
        }

        this->globalVars->push_back(v);
    }
}

void DIReducerContext::initFuncDesc() {
    InfoObject *infoObject = this->dstDwarfObject.infoObject.get();
    for(auto &cu : infoObject->compilationUnits) {
        this->funcDataBase.push_back(std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>());
        this->initFuncDescInCompilationUnit(cu, this->funcDataBase.back());
    }
}

void DIReducerContext::initFuncDescInCompilationUnit(CompilationUnit &cu, std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>> &funcMap) {
    this->initFuncDescInInfoEntry(cu.infoEntry, funcMap);
}

void DIReducerContext::initFuncDescInInfoEntry(InfoEntry &infoEntry, std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>> &funcMap) {
    if(infoEntry.isNull())
        return;

    if(DWARF::isFuncAbbrev(infoEntry.abbrevItem)) {
        uint64_t low_pc  = 0;
        uint64_t high_pc = 0;
        uint64_t retType = 0;
        uint64_t absFunc = 0;
        std::string dwarfName;

        auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter = infoEntry.attrBuffers.begin();
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_name || abbrevEntryIter->at == DW_AT_linkage_name)
                dwarfName = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            if(abbrevEntryIter->at == DW_AT_low_pc)
                low_pc = get_dw_at_low_pc(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_high_pc)
                high_pc = get_dw_at_high_pc(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_type)
                retType = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_specification)
                absFunc = get_dw_at_abs_ori(*abbrevEntryIter, *attrRawBufferIter) + infoEntry.cuOffset;

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        if(absFunc != 0) {
            if(!this->absFuncsInfo.contains(absFunc))
                FIG_PANIC("can not find abstract origin in absFuncInfo. ", "absFunc: 0x", dec_to_hex(absFunc));

            auto &absFuncInfo = this->absFuncsInfo[absFunc];
            dwarfName         = absFuncInfo.first;
            retType           = absFuncInfo.second;
        }

        static uint64_t unnamedFunctionId = 1;
        if(dwarfName.empty()) {
            dwarfName = "unnamed_function" + std::to_string(unnamedFunctionId++) + "@" + dec_to_hex(low_pc);
        }

        auto func    = std::make_shared<CFG::Func>(dwarfName, low_pc, high_pc);
        func->module = infoEntry.module;

        // return type
        if(retType == 0) {
            func->retVType = CFG::VTypeFactory::baseVType.bt_void;
        } else {
            func->retVType = CFG::VTypeFactory::getVTypeByModuleOffset(func->module, retType);
        }

        // parameters and variables
        for(auto &child : infoEntry.children) {
            this->fillFuncDescInInfoEntry(func, child, func->low_pc, func->high_pc);
        }

        // special check, for some gcc's bugs, low_pc is 0
        if(func->low_pc != 0)
            funcMap.insert(std::make_pair(func->low_pc, func));
    }

    // deal with children mode
    for(auto &child : infoEntry.children) {
        this->initFuncDescInInfoEntry(child, funcMap);
    }
}

void DIReducerContext::fillFuncDescInInfoEntry(std::shared_ptr<CFG::Func> func, InfoEntry &infoEntry, uint64_t low_pc, uint64_t high_pc) {
    auto &item = infoEntry.abbrevItem;
    if(item.tag == DW_TAG_lexical_block) { // lexical_block
        uint64_t lexical_block_low_pc  = low_pc;
        uint64_t lexical_block_high_pc = high_pc - low_pc;
        auto abbrevEntryIter           = infoEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter         = infoEntry.attrBuffers.begin();
        while(!abbrevEntryIter->isNull()) {
            if(abbrevEntryIter->at == DW_AT_low_pc)
                lexical_block_low_pc = get_dw_at_low_pc(*abbrevEntryIter, *attrRawBufferIter);
            if(abbrevEntryIter->at == DW_AT_high_pc)
                lexical_block_high_pc = get_dw_at_high_pc(*abbrevEntryIter, *attrRawBufferIter);

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        lexical_block_high_pc = lexical_block_low_pc + lexical_block_high_pc;

        for(auto &child : infoEntry.children) {
            this->fillFuncDescInInfoEntry(func, child, lexical_block_low_pc, lexical_block_high_pc);
        }
    } else if(DWARF::isVariableAbbrev(item) || DWARF::isParameterAbbrev(item)) {
        CFG::Var v             = {};
        v.isParameter          = DWARF::isParameterAbbrev(item);
        auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
        auto attrRawBufferIter = infoEntry.attrBuffers.begin();
        uint64_t absVar        = 0;
        while(!abbrevEntryIter->isNull()) {
            uint64_t at   = abbrevEntryIter->at;
            uint64_t form = abbrevEntryIter->form;
            if(at == DW_AT_name) {
                v.name = get_dw_at_name(*abbrevEntryIter, *attrRawBufferIter, this->strTable);
            }
            if(at == DW_AT_type) {
                uint64_t type = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
                v.vType       = CFG::VTypeFactory::getVTypeByModuleOffset(func->module, type);
            }
            if(at == DW_AT_abstract_origin || at == DW_AT_specification) {
                absVar = get_dw_at_abs_ori(*abbrevEntryIter, *attrRawBufferIter) + infoEntry.cuOffset;
            }
            if(at == DW_AT_location) {
                int64_t temp = 0;
                if(form != DW_FORM_exprloc)
                    FIG_PANIC("Unexpected!");
                uint8_t leb128u_size    = get_length_leb128(attrRawBufferIter->buffer.get(), temp);
                uint64_t expression_len = read_leb128u(attrRawBufferIter->buffer.get(), temp);
                DWARFExprParser parser;
                parser.init(
                    attrRawBufferIter->buffer.get() + leb128u_size,
                    expression_len,
                    infoEntry.abbrevParser->bitWidth,
                    infoEntry.abbrevParser->addressSize);
                auto &op_list = parser.op_list;
                if(op_list.size() == 1 && op_list[0].operation == DW_OP_fbreg) {
                    v.addr.type      = CFG::ADDR_TYPE::STACK;
                    v.addr.CFAOffset = op_list[0].operand1.s;
                } else if(op_list.size() == 1 && op_list[0].operation == DW_OP_breg6) {
                    v.addr.type      = CFG::ADDR_TYPE::STACK;
                    v.addr.CFAOffset = op_list[0].operand1.s + 8;
                } else if(op_list.size() == 1 && op_list[0].operation == DW_OP_breg7) {
                    v.addr.type      = CFG::ADDR_TYPE::STACK;
                    v.addr.CFAOffset = op_list[0].operand1.s + 8;
                } else if(op_list.size() == 2 && op_list[0].operation == DW_OP_fbreg && op_list[1].operation == DW_OP_deref) {
                    v.addr.type       = CFG::ADDR_TYPE::GLOBAL;
                    v.addr.globalAddr = 7;
                } else if(op_list.size() == 1 && op_list[0].operation == DW_OP_addr) {
                    v.addr.type       = CFG::ADDR_TYPE::GLOBAL;
                    v.addr.globalAddr = op_list[0].operand1.u;
                } else if(op_list.size() == 1 && op_list[0].operation >= DW_OP_reg0 && op_list[0].operation <= DW_OP_reg31) {
                    v.addr.type = CFG::ADDR_TYPE::REG;
                    v.addr.reg  = DW_OP_reg0 - op_list[0].operation;
                } else {
                    FIG_PANIC("Unexpected dwarf expression for describing the addr of variale. ", dw_op_to_str(op_list[0].operation));
                }
            }

            abbrevEntryIter   = std::next(abbrevEntryIter);
            attrRawBufferIter = std::next(attrRawBufferIter);
        }

        // find abstract origin and fill name&type of variable
        if(absVar != 0) {
            if(!this->absVarsInfo.contains(absVar))
                FIG_PANIC("can not find abstract origin in absVarsInfo. ", "absOri: 0x", dec_to_hex(absVar));

            auto &absVarInfo = this->absVarsInfo[absVar];
            uint64_t type    = absVarInfo.second;
            v.vType          = CFG::VTypeFactory::getVTypeByModuleOffset(func->module, type);
            v.name           = absVarInfo.first;
        }

        // Unique id for variable
        v.varId   = infoEntry.cuOffset + infoEntry.innerOffset;
        v.low_pc  = low_pc;
        v.high_pc = high_pc;
        if(v.isParameter) {
            func->parameterList.push_back(v);
        } else {
            func->variableList.push_back(v);
        }
    }
}

std::shared_ptr<CFG::VTypeInfo> DIReducerContext::getVTypeInfo(uint64_t type, uint16_t module) {
    if(module > this->vTypeInfoDataBase.size())
        FIG_PANIC("moduleID > Max module");

    auto &vTypeMap = this->vTypeInfoDataBase[module - 1];
    if(vTypeMap.find(type) == vTypeMap.end())
        FIG_PANIC("can not find the var type in module ", module);

    return vTypeMap[type];
}

CFG::BASETYPE getBaseTypeofGType(const CFG::GTypeInfo &gTypeInfo) {
    if(gTypeInfo.dwTag != DW_TAG_base_type)
        FIG_PANIC("This is not base type!");
    switch(gTypeInfo.detail.base.encoding) {
    case DW_ATE_address: return CFG::BASETYPE::BT_UINT64; break;
    case DW_ATE_boolean: return CFG::BASETYPE::BT_BOOL; break;
    case DW_ATE_complex_float: return CFG::BASETYPE::BT_FLOAT128; break;
    case DW_ATE_signed: {
        switch(gTypeInfo.size) {
        case 1: return CFG::BASETYPE::BT_INT8; break;
        case 2: return CFG::BASETYPE::BT_INT16; break;
        case 4: return CFG::BASETYPE::BT_INT32; break;
        case 8: return CFG::BASETYPE::BT_INT64; break;
        case 16: return CFG::BASETYPE::BT_INT128; break;
        default: FIG_PANIC("unsupported signed integer size!");
        }
        break;
    }
    case DW_ATE_signed_char: return CFG::BASETYPE::BT_INT8; break;
    case DW_ATE_unsigned: {
        switch(gTypeInfo.size) {
        case 1: return CFG::BASETYPE::BT_UINT8; break;
        case 2: return CFG::BASETYPE::BT_UINT16; break;
        case 4: return CFG::BASETYPE::BT_UINT32; break;
        case 8: return CFG::BASETYPE::BT_UINT64; break;
        case 16: return CFG::BASETYPE::BT_UINT128; break;
        default: FIG_PANIC("unsupported unsigned integer size! size:", gTypeInfo.size);
        }
        break;
    }
    case DW_ATE_unsigned_char: return CFG::BASETYPE::BT_UINT8; break;
    case DW_ATE_float: {
        switch(gTypeInfo.size) {
        case 4: return CFG::BASETYPE::BT_FLOAT32; break;
        case 8: return CFG::BASETYPE::BT_FLOAT64; break;
        case 16: return CFG::BASETYPE::BT_FLOAT128; break;
        default: FIG_PANIC("unsupported float size! size:", gTypeInfo.size);
        }
        break;
    }
    case DW_ATE_UTF: return CFG::BASETYPE::BT_INT16; break;
    default: FIG_PANIC("unsupported base type! encoding:", gTypeInfo.detail.base.encoding);
    }
}

void initVTypeInfoRecursion(
    uint64_t curIdent,
    std::vector<bool> &snVisited,
    std::unordered_map<uint64_t, std::shared_ptr<CFG::VTypeInfo>> &vTypeInfoMap) {
    if(vTypeInfoMap.find(curIdent) == vTypeInfoMap.end())
        FIG_PANIC("can not find curIdent in vTypeInfoMap. ", "curIndet: 0x", dec_to_hex(curIdent));

    auto &vTypeInfo = vTypeInfoMap[curIdent];
    if(snVisited[vTypeInfo->sn] == true)
        return;

    snVisited[vTypeInfo->sn] = true;

    switch(vTypeInfo->dwTag) {
    case DW_TAG_base_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::BASE;
        break;
    }
    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
    case DW_TAG_ptr_to_member_type:
    case DW_TAG_rvalue_reference_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::POINTER;
        if(vTypeInfo->referTypeIdent == 0) {
            vTypeInfo->name = "void*";
        } else {
            initVTypeInfoRecursion(vTypeInfo->referTypeIdent, snVisited, vTypeInfoMap);
            const auto &targerVTypeInfo = vTypeInfoMap[vTypeInfo->referTypeIdent];
            vTypeInfo->referVTypeInfo   = targerVTypeInfo;
            vTypeInfo->name             = targerVTypeInfo->name + "*";
        }
        break;
    }
    case DW_TAG_array_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::ARRAY;
        if(vTypeInfo->referTypeIdent == 0) FIG_PANIC("Why Array Type does not have Sub Type...");
        initVTypeInfoRecursion(vTypeInfo->referTypeIdent, snVisited, vTypeInfoMap);
        const auto &targerVTypeInfo = vTypeInfoMap[vTypeInfo->referTypeIdent];
        vTypeInfo->referVTypeInfo   = targerVTypeInfo;
        if(vTypeInfo->arrayBound > 0) {
            vTypeInfo->name = targerVTypeInfo->name + "[" + std::to_string(vTypeInfo->arrayBound) + "]";
            vTypeInfo->size = targerVTypeInfo->size * vTypeInfo->arrayBound;
        } else {
            vTypeInfo->name = vTypeInfoMap[vTypeInfo->referTypeIdent]->name + "[]";
        }
        break;
    }
    case DW_TAG_enumeration_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::ENUMERATION;
        break;
    }
    case DW_TAG_subroutine_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::FUNCTION;
        if(vTypeInfo->referTypeIdent == 0) {
            vTypeInfo->name = "(void)";
        } else {
            initVTypeInfoRecursion(vTypeInfo->referTypeIdent, snVisited, vTypeInfoMap);
            const auto &targerVTypeInfo = vTypeInfoMap[vTypeInfo->referTypeIdent];
            vTypeInfo->referVTypeInfo   = targerVTypeInfo;
            vTypeInfo->name             = "(" + targerVTypeInfo->name + ")";
        }
        break;
    }
    case DW_TAG_union_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::UNION;
        for(const auto &it : vTypeInfo->unionMemberIdentVec) {
            auto memberRefTypeIdent = it;
            initVTypeInfoRecursion(memberRefTypeIdent, snVisited, vTypeInfoMap);
            const auto &targerVTypeInfo = vTypeInfoMap[memberRefTypeIdent];
            vTypeInfo->unionMemberVTypeInfoVec.push_back(targerVTypeInfo);
        }
        break;
    }
    case DW_TAG_structure_type:
    case DW_TAG_class_type: {
        vTypeInfo->typeTag = CFG::TYPETAG::STRUCTURE;
        for(const auto &it : vTypeInfo->structMemberIdentVec) {
            auto memberRefTypeIdent = it.first.first;
            initVTypeInfoRecursion(memberRefTypeIdent, snVisited, vTypeInfoMap);
            const auto &targerVTypeInfo = vTypeInfoMap[memberRefTypeIdent];
            vTypeInfo->structMemberVTypeInfoVec.push_back(std::make_pair(std::make_pair(targerVTypeInfo, it.first.second), it.second));
        }
        break;
    }
    case DW_TAG_const_type:
    case DW_TAG_volatile_type:
    case DW_TAG_restrict_type:
    case DW_TAG_typedef: {
        if(vTypeInfo->referTypeIdent == 0) break;
        initVTypeInfoRecursion(vTypeInfo->referTypeIdent, snVisited, vTypeInfoMap);
        const auto &targerVTypeInfo = vTypeInfoMap[vTypeInfo->referTypeIdent];
        vTypeInfo->name             = targerVTypeInfo->name;
        vTypeInfo->size             = targerVTypeInfo->size;
        vTypeInfo->typeTag          = targerVTypeInfo->typeTag;
        switch(vTypeInfo->typeTag) {
        case CFG::TYPETAG::BASE: vTypeInfo->baseType = targerVTypeInfo->baseType; break;
        case CFG::TYPETAG::POINTER: vTypeInfo->referVTypeInfo = targerVTypeInfo->referVTypeInfo; break;
        case CFG::TYPETAG::ARRAY: {
            vTypeInfo->referVTypeInfo = targerVTypeInfo->referVTypeInfo;
            vTypeInfo->arrayBound     = targerVTypeInfo->arrayBound;
            break;
        }
        case CFG::TYPETAG::UNION: {
            vTypeInfo->unionMemberIdentVec     = targerVTypeInfo->unionMemberIdentVec;
            vTypeInfo->unionMemberVTypeInfoVec = targerVTypeInfo->unionMemberVTypeInfoVec;
            break;
        }
        case CFG::TYPETAG::STRUCTURE: {
            vTypeInfo->structMemberIdentVec     = targerVTypeInfo->structMemberIdentVec;
            vTypeInfo->structMemberVTypeInfoVec = targerVTypeInfo->structMemberVTypeInfoVec;
            break;
        }
        default: break;
        }
        break;
    }
    default: FIG_PANIC("unsupported DW_Atg for type. DW_TAG:", dw_tag_to_str(vTypeInfo->dwTag));
    }
}

void DIReducerContext::initVTypeInfo() {
    this->vTypeInfoDataBase.resize(this->gTypeInfoDataBase.size());
    for(size_t i = 0; i < this->gTypeInfoDataBase.size(); i++) {
        auto &gTypeInfoMap = this->gTypeInfoDataBase[i];
        auto &vTypeInfoMap = this->vTypeInfoDataBase[i];
        uint32_t sn        = 1;
        for(const auto &it : gTypeInfoMap) {
            auto &gTypeInfo = it.second;
            vTypeInfoMap.insert(std::make_pair(gTypeInfo.ident, new CFG::VTypeInfo()));
            auto &vTypeInfo           = vTypeInfoMap[gTypeInfo.ident];
            vTypeInfo->typeTag        = CFG::TYPETAG::VOID;
            vTypeInfo->module         = gTypeInfo.module;
            vTypeInfo->name           = gTypeInfo.name;
            vTypeInfo->dwTag          = gTypeInfo.dwTag;
            vTypeInfo->ident          = gTypeInfo.ident;
            vTypeInfo->sn             = sn++;
            vTypeInfo->size           = gTypeInfo.size;
            vTypeInfo->referTypeIdent = gTypeInfo.reference;
            vTypeInfo->attr_count     = gTypeInfo.attr_count;
            if(gTypeInfo.dwTag == DW_TAG_base_type) {
                vTypeInfo->baseType = getBaseTypeofGType(gTypeInfo);
                vTypeInfo->name     = CFG::BaseTypeToString(vTypeInfo->baseType);
            } else if(gTypeInfo.dwTag == DW_TAG_array_type) {
                vTypeInfo->arrayBound = gTypeInfo.detail.array.arrayBound;
            } else if(gTypeInfo.dwTag == DW_TAG_structure_type || gTypeInfo.dwTag == DW_TAG_class_type) {
                for(const auto &it : gTypeInfo.detail.structType.memberTypeInfoVec) {
                    vTypeInfo->structMemberIdentVec.push_back(it);
                }
            } else if(gTypeInfo.dwTag == DW_TAG_union_type) {
                for(const auto &it : gTypeInfo.detail.unionType.memberTypeInfoVec) {
                    vTypeInfo->unionMemberIdentVec.push_back(it);
                }
            }
        }
        for(auto &it : vTypeInfoMap) {
            auto &vTypeInfo = it.second;
            if(vTypeInfo->referTypeIdent == 0)
                continue;
            if(vTypeInfoMap.find(vTypeInfo->referTypeIdent) == vTypeInfoMap.end()) {
                uint64_t cuOffset = this->dstDwarfObject.infoObject->compilationUnits[vTypeInfo->module - 1].offset;
                FIG_PANIC("can not find the VTypeInfo;", " referTypeIdent: 0x", dec_to_hex(cuOffset + vTypeInfo->referTypeIdent), " cur ident: 0x", dec_to_hex(cuOffset + vTypeInfo->ident));
            }
            vTypeInfo->referTypeSn = vTypeInfoMap[vTypeInfo->referTypeIdent]->sn;
        }
        std::vector<bool> snVisited(sn, false);
        for(auto &it : vTypeInfoMap) {
            initVTypeInfoRecursion(it.first, snVisited, vTypeInfoMap);
        }
    }
}

void DIReducerContext::initCustomFuncMap() {
    if(this->customFuncMap) {
        this->customFuncMap->clear();
    } else {
        this->customFuncMap = std::make_shared<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>>();
    }

    for(const auto &fMap : this->funcDataBase) {
        for(const auto &[addr, func] : fMap) {
            this->customFuncMap->insert({addr, func});
        }
    }
}

void DIReducerContext::initVTypeFactory() {
    CFG::VTypeFactory::init(this->vTypeInfoDataBase.size());
    for(const auto &vTypeInfoMap : this->vTypeInfoDataBase) {
        for(const auto &it : vTypeInfoMap) {
            auto &vTypeInfo = it.second;
            CFG::VTypeFactory::getVTypeByVTypeInfo(*vTypeInfo);
        }
    }
}

void DIReducerContext::initFuncASM() {
    for(auto &fMap : this->funcDataBase) {
        for(auto &[addr, funcPtr] : fMap) {
            funcPtr->setInsnbuffer(this->elfObject.buffer.get() + funcPtr->low_pc - this->elfObject.baseAddr);
            funcPtr->disasm();
        }
    }
}

void DIReducerContext::initFuncCFG() {
    for(auto &fMap : this->funcDataBase) {
        for(auto &[addr, funcPtr] : fMap) {
            funcPtr->setELFBuffer(this->elfObject.buffer);
            funcPtr->constructCFG();
        }
    }
}

void DIReducerContext::initProgramCFG() {
    for(auto &fMap : this->funcDataBase) {
        for(auto &it : fMap) {
            auto &cur_func = it.second;
            if(!cur_func->hasCFG())
                continue;
            /* Traverse blocks */
            for(auto &entry : cur_func->cfg.value().blocks) {
                auto &block = entry.second;
                if(block->type != CFG::BLOCK_TYPE::COMMON_BLOCK)
                    continue;

                const auto &last_insn = block->statements.back();
                const auto &detail    = last_insn.detail->x86;
                if(!(last_insn.id == X86_INS_CALL && detail.op_count == 1 && detail.operands[0].type == X86_OP_IMM))
                    continue;

                auto target_addr = detail.operands[0].imm;
                for(auto &in_fMap : this->funcDataBase) {
                    if(in_fMap.find(target_addr) != in_fMap.end()) {
                        auto &target_func = in_fMap[target_addr];
                        if(!target_func->hasCFG())
                            continue;

                        auto target_entry         = target_func->cfg->getEntryBlock();
                        auto target_exit_optional = target_func->cfg->getExitBlock();
                        auto call_edge            = std::make_shared<CFG::Edge>(block, target_entry, CFG::EDGE_TYPE::CALL);
                        block->forward_edges.push_back(call_edge);
                        target_entry->backward_edges.push_back(call_edge);
                        if(block->next_block.has_value() && target_exit_optional.has_value()) {
                            auto target_exit = target_exit_optional.value();
                            auto nextBlock   = block->next_block.value();
                            auto return_edge = std::make_shared<CFG::Edge>(target_exit, nextBlock, CFG::EDGE_TYPE::RETURN);
                            nextBlock->backward_edges.push_back(return_edge);
                            target_exit->forward_edges.push_back(return_edge);
                        }
                    }
                }
            }
        }
    }
}

void DIReducerContext::initCG() {
    this->cgInfo.name      = fs::path(this->elfObject.filePath).filename();
    this->cgInfo.startTime = std::chrono::steady_clock::now();
    for(auto &fMap : this->funcDataBase) {
        for(const auto &[funcAddr, func] : fMap) {
            if(!func->hasCFG())
                continue;
            auto newCGNode = std::make_shared<CFG::CGNode>(func);
            this->cg.nodes.insert(std::make_pair(newCGNode->f->low_pc, newCGNode));
        }
    }

    for(auto &fMap : this->funcDataBase) {
        for(auto &[funcAddr, func] : fMap) {
            if(!func->hasCFG())
                continue;

            for(auto &[sn, block] : func->cfg.value().blocks) {
                if(block->type != CFG::BLOCK_TYPE::COMMON_BLOCK)
                    continue;

                auto &last_insn    = block->statements.back();
                const auto &detail = last_insn.detail->x86;
                if(!(last_insn.id == X86_INS_CALL && detail.op_count == 1 && detail.operands[0].type == X86_OP_IMM))
                    continue;

                auto targetAddr = detail.operands[0].imm;
                for(auto &inFMap : this->funcDataBase) {
                    if(inFMap.contains(targetAddr)) {
                        auto curAddr                                 = func->low_pc;
                        this->cg.nodes[curAddr]->callees[targetAddr] = this->cg.nodes[targetAddr];
                        this->cg.nodes[targetAddr]->callers[curAddr] = this->cg.nodes[curAddr];
                    }
                }
            }
        }
    }

    this->cgInfo.endTime  = std::chrono::steady_clock::now();
    auto duration         = std::chrono::duration_cast<std::chrono::nanoseconds>(this->cgInfo.endTime - this->cgInfo.startTime);
    this->cgInfo.timeCost = static_cast<double>(duration.count()) / 1000;
}