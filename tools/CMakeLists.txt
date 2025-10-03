file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")

foreach(SUBDIR ${SUBDIRS})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        message(STATUS "Found Tool    - ${SUBDIR}")
        add_subdirectory(${SUBDIR})
    endif()
endforeach()