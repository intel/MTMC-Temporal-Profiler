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
set(MAIN_LIBRARY "")

OPTION(DPRINT_ENABLE "Enable general debug print" OFF)
OPTION(D_DPRINT_ENABLE "Enable detailed debug print" OFF)
OPTION(BUILD_TEST "Build test and example folder" OFF)
OPTION(BUILD_TF_TEST "Build tensorflow tests" OFF)
#OPTION(EBPF_CTX_SC "Build to support eBPF based context switch pmc probe. (require bcc library)" OFF)
OPTION(OTL_EXPORTER "Build to support export as opentelemetry standard to the Jaeger Backend" ON)

set(mtmc_sources util.cpp perfmon_config.cpp perfmon_collector.cpp mtmc_profiler.cpp guard_sampler.cpp)
set(mtmc_headers guard_sampler.h mtmc_temp_profiler.h mtmc_profiler.h perfmon_collector.h perfmon_config.h util.h env.h)
set(mtmc_link_library -lpthread)

find_package(nlohmann_json REQUIRED)

if (OTL_EXPORTER)
    set(mtmc_sources ${mtmc_sources} exporter.cpp)
    set(mtmc_headers ${mtmc_headers} exporter.h)
    set(mtmc_link_library ${mtmc_link_library} -lrt -lipc)
endif()

# Perftester SHARED
add_library(pfc SHARED ${mtmc_sources})
target_link_libraries(pfc PUBLIC ${mtmc_link_library})
set_target_properties(pfc PROPERTIES COMPILE_FLAGS "${MAIN_FLAG} -lpthread")

set(CMP_OPS "")

if(DPRINT_ENABLE)
    set(CMP_OPS "${CMP_OPS} DEBUG_PRINT")
endif()

if(D_DPRINT_ENABLE)
    set(CMP_OPS "${CMP_OPS} DETAILED_PRINT")
endif()

target_compile_definitions(pfc PUBLIC ${CMP_OPS})

# Install
set(install_headers ${mtmc_headers})
set(include_dir /usr/local/include/mtmc)
set(lib_dir /usr/local/lib64)
set(bin_dir /usr/local/bin)
install(FILES ${install_headers} DESTINATION ${include_dir})
install(TARGETS pfc LIBRARY DESTINATION ${lib_dir})

if (OTL_EXPORTER)
    add_subdirectory(OtlExportProxy)
endif()

# Unit tests for all modules and submodules for cpp version of mtmc per
if (BUILD_TEST)
    add_subdirectory(tests)
endif()