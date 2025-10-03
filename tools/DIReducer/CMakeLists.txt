find_package(Boost REQUIRED COMPONENTS program_options thread CONFIG)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost library not found. Please install Boost or specify Boost location.")
endif()

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    link_directories(
        BEFORE
        ${CMAKE_SOURCE_DIR}/external/lib/aarch64
    )
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    link_directories(
        BEFORE
        ${CMAKE_SOURCE_DIR}/external/lib/amd64
    )
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_executable(
    DIReducer
    main.cpp
    Config.cpp
    Context.cpp
    LibLoader.cpp
)

target_link_libraries(
    DIReducer PRIVATE
    Core
    CFG
    DWARF
    DWARFLinker
    Utils
    capstone
    Boost::program_options
    Boost::thread
)