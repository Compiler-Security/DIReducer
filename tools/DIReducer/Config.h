#ifndef DR_CONFIG_H
#define DR_CONFIG_H

#include <boost/program_options/variables_map.hpp> // for variables_map
#include <string>                                  // for string

namespace opts = boost::program_options;

struct Config {
    bool is_print_elf;
    bool is_print_info;
    bool is_print_abbrev;
    bool is_help;
    bool is_dwarf_verbose;
    bool is_analysis_verbose;
    bool is_run;
    bool is_single_thread;
    std::string src_path;
    std::string dst_path;

    Config(const opts::variables_map &vm);
    void reset();
};

#endif