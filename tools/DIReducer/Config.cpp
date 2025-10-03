#include "Config.h"
#include <boost/program_options/variables_map.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <filesystem>
#include <string>
#include "Fig/assert.hpp"

namespace fs   = std::filesystem;
namespace opts = boost::program_options;

Config::Config(const opts::variables_map &vm) {
    this->reset();
    if(vm.count("input"))
        this->src_path = vm["input"].as<std::string>();
    if(vm.count("output"))
        this->dst_path = vm["output"].as<std::string>();

    this->is_print_elf        = vm["print-elf"].as<bool>();
    this->is_print_info       = vm["print-info"].as<bool>();
    this->is_print_abbrev     = vm["print-abbrev"].as<bool>();
    this->is_dwarf_verbose    = vm["dwarf-verbose"].as<bool>();
    this->is_analysis_verbose = vm["analysis-verbose"].as<bool>();
    this->is_run              = vm["run"].as<bool>();
    this->is_single_thread    = vm["single-thread"].as<bool>();

    FIG_ASSERT(!this->src_path.empty(), "input file is empty!");
    if(this->dst_path.empty())
        this->dst_path.assign(this->src_path + "-dump");

    this->src_path = fs::absolute(fs::path(this->src_path)).string();
    this->dst_path = fs::absolute(fs::path(this->dst_path)).string();
}

void Config::reset() {
    this->is_print_elf     = false;
    this->is_print_info    = false;
    this->is_print_abbrev  = false;
    this->is_dwarf_verbose = false;
    this->is_help          = false;
    this->src_path.assign("");
    this->dst_path.assign("");
}
