#Copyright 2022 Intel Corporation
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

set(MAIN_FLAG " -std=c++11 -march=native -O3 -fPIC")

OPTION(DPRINT_ENABLE "Enable general debug print" OFF)
OPTION(D_DPRINT_ENABLE "Enable detailed debug print" OFF)
OPTION(BUILD_TEST "Build test and example folder" OFF)

# Perftester SHARED
add_library(pfc SHARED util.cpp perfmon_config.cpp perfmon_collector.cpp mtmc_profiler.cpp guard_sampler.cpp)
target_link_libraries(pfc PUBLIC -lpthread)
set_target_properties(pfc PROPERTIES COMPILE_FLAGS "${MAIN_FLAG} -lpthread")

if(DPRINT_ENABLE)
    target_compile_definitions(pfc PUBLIC DEBUG_PRINT)
endif()

if(D_DPRINT_ENABLE)
    target_compile_definitions(pfc PUBLIC DETAILED_PRINT)
endif()

# Install
set(install_headers guard_sampler.h mtmc_temp_profiler.h)
set(include_dir /usr/local/include/mtmc)
set(lib_dir /usr/local/lib64)
install(FILES ${install_headers} DESTINATION ${include_dir})
install(TARGETS pfc LIBRARY DESTINATION ${lib_dir})

# Unit tests for all modules and submodules for cpp version of mtmc per
if (BUILD_TEST)
    add_subdirectory(tests)
endif()