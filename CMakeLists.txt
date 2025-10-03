message(STATUS "Found Project Root")

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(dr)

# OPTION
if(DEFINED DEBUG_MODE)
    if(DEBUG_MODE STREQUAL "on")
        message(STATUS "Enable debug mode")
    else()
        message(STATUS "Disable debug mode")
        add_compile_definitions(FIG_DEBUG_OFF_MODE)
    endif()
endif()

# if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -gdwarf-5")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -gdwarf-5") # '-stdlib=libc++' is not binary compatible with default Boost
# elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -gdwarf-5")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -gdwarf-5 ")
# else()
#     message(FATAL_ERROR "Supporting only GCC or Clang")
# endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
    AFTER
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/include
)

add_subdirectory(lib)
add_subdirectory(tools)