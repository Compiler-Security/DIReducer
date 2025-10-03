#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iostream>
#include <memory>
#include <string>
#include "Config.h"
#include "Context.h"
#include "Fig/error.hpp"

namespace opts = boost::program_options;

int main(int argc, char *argv[]) {
    opts::options_description optDesc("DIReducer options");
    optDesc.add_options()("input", opts::value<std::string>(), "input file");
    optDesc.add_options()("output", opts::value<std::string>(), "output file");
    optDesc.add_options()("single-thread", opts::bool_switch()->default_value(false), "enable single thread");
    optDesc.add_options()("help", opts::bool_switch()->default_value(false), "display this help and exit");
    optDesc.add_options()("version", opts::bool_switch()->default_value(false), "output version information and exit");
    optDesc.add_options()("print-elf", opts::bool_switch()->default_value(false), "display elf header information");
    optDesc.add_options()("print-info", opts::bool_switch()->default_value(false), "display debug-info");
    optDesc.add_options()("print-abbrev", opts::bool_switch()->default_value(false), "display debug-abbrev");
    optDesc.add_options()("dwarf-verbose", opts::bool_switch()->default_value(false), "enable detailed message output when parsing dwarf");
    optDesc.add_options()("analysis-verbose", opts::bool_switch()->default_value(false), "enable detailed message output when analysing");
    optDesc.add_options()("run", opts::bool_switch()->default_value(false), "enable Type Inference");

    opts::variables_map vm;
    try {
        opts::store(opts::parse_command_line(argc, argv, optDesc), vm);
        notify(vm);
    } catch(opts::error &e) {
        FIG_ERROR("Fail to parse arguments: ", e.what());
    } catch(...) {
        FIG_ERROR("Unknown exception caught!\n");
    }

    // pre-process
    if(vm["help"].as<bool>()) {
        std::cout << optDesc << std::endl;
        return 0;
    } else if(vm["version"].as<bool>()) {
        std::cout << "dr 0.1.0" << std::endl;
        return 0;
    } else if(vm.count("input") == 0) {
        FIG_ERROR("Missing input file");
    }

    Config drConfig(vm);
    std::make_unique<DIReducerContext>(drConfig)->run();

    return 0;
}
