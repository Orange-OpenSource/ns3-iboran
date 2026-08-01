# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(yaml-cpp REQUIRED)

set(name iboran)

include_directories(
    ${PROJECT_SOURCE_DIR}/contrib/iboran
    ${PROJECT_SOURCE_DIR}/contrib/ai/model
    ${PROJECT_SOURCE_DIR}/build/include
    ${PROJECT_SOURCE_DIR}/src/buildings/model
    ${PROJECT_SOURCE_DIR}/src/buildings/helper
)

set(IBORAN_COMMON_SOURCES
    model/iboran_application/iboran-application.cc
    model/iboran_application/iboran-application.h
    model/iboran_application/iboran-side-info-tracker.cc
    model/iboran_application/iboran-side-info-tracker.h
    model/iboran_interfaces/iboran-shm-structure.h
    model/iboran_interfaces/iboran-shm-structure.cc
    model/iboran_ran_environment/iboran-ran-params.h
    model/iboran_ran_environment/iboran-ran-params.cc
    model/iboran_ran_environment/iboran-ran-common.h
    model/iboran_ran_environment/iboran-ran-common.cc
)

set(IBORAN_COMMON_LIBS
    ${libcore}
    ${libpoint-to-point}
    ${libnetwork}
    ${libapplications}
    ${libmobility}
    ${libcsma}
    ${libinternet}
    ${libflow-monitor}
    ${libbuildings}
    ${libnetanim}
    yaml-cpp::yaml-cpp
)

if(UNIX AND NOT APPLE)
    list(APPEND IBORAN_COMMON_LIBS rt pthread)
elseif(APPLE)
    list(APPEND IBORAN_COMMON_LIBS pthread)
endif()

build_lib(
    LIBNAME ${name}
    SOURCE_FILES 
        ${IBORAN_COMMON_SOURCES}
        model/iboran_mac_scheduler/iboran-lte-mac-scheduler.h
        model/iboran_mac_scheduler/iboran-lte-mac-scheduler.cc
        model/iboran_mac_scheduler/iboran-nr-mac-scheduler.h
        model/iboran_mac_scheduler/iboran-nr-mac-scheduler.cc
    LIBRARIES_TO_LINK 
        ${IBORAN_COMMON_LIBS}
        ${liblte}
        ${libnr}
)

# == PYTHON BINDINGS ================================================
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(BUILD_PYTHON_BINDINGS)
    execute_process(
        COMMAND python3 -c "import sysconfig; print(sysconfig.get_path('include'))"
        OUTPUT_VARIABLE PYTHON3_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(PYTHON3_INCLUDE_DIR)
        message(STATUS "Found Python include: ${PYTHON3_INCLUDE_DIR}")
        
        find_package(pybind11 QUIET)
        
        if(pybind11_FOUND)
            set(BINDING_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/model/iboran_interfaces/iboran_shm_structure_py_binding.cc")
            
            if(EXISTS ${BINDING_SOURCE})
                message(STATUS "Building Python bindings")
                
                add_library(iboran_shm_structure_py_binding MODULE
                    model/iboran_interfaces/iboran_shm_structure_py_binding.cc
                )
                
                target_include_directories(iboran_shm_structure_py_binding
                    PRIVATE
                        ${PYTHON3_INCLUDE_DIR}
                        ${pybind11_INCLUDE_DIRS}
                )
                
                target_link_libraries(iboran_shm_structure_py_binding
                    PRIVATE
                        pybind11::module
                        ${libiboran}
                        ${libcore}
                        ${liblte}
                        ${libnr}
                        ${libbuildings}
                )
                
                set_target_properties(iboran_shm_structure_py_binding PROPERTIES
                    PREFIX ""
                    SUFFIX ".so"
                    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/model//iboran_interfaces
                    CXX_VISIBILITY_PRESET "hidden"
                )
                
                if(APPLE)
                    set_target_properties(iboran_shm_structure_py_binding PROPERTIES
                        BUILD_RPATH "${PROJECT_SOURCE_DIR}/build/lib"
                        INSTALL_RPATH "${PROJECT_SOURCE_DIR}/build/lib;@loader_path;@loader_path/../../build/lib"
                        BUILD_WITH_INSTALL_RPATH FALSE
                        INSTALL_RPATH_USE_LINK_PATH TRUE
                    )
                endif()
                
                add_dependencies(iboran_shm_structure_py_binding ${libiboran})
                
                message(STATUS "Python binding configured")
            endif()
        endif()
    endif()
endif()

message(STATUS "ib-ORAN configured successfully")