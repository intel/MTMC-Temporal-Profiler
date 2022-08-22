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

cmake_minimum_required(VERSION 3.12)

project(mtmc)

set(CMAKE_CXX_STANDARD 11)

set(EXECUTABLE_OUTPUT_PATH ${mtmc_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${mtmc_BINARY_DIR}/lib)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

message("Project binary dir:" ${mtmc_BINARY_DIR})
message("Project source dir:" ${mtmc_SOURCE_DIR})

set(MIN_CXX_VER "8.4.0")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL MIN_CXX_VER)
    add_subdirectory(cpp)
else()
    message(FATAL_ERROR "CXX compiler version should greater than or equals to ${MIN_CXX_VER}")
endif()
