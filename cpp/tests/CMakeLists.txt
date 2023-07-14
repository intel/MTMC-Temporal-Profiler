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

include_directories(../)

set(TEST_FLAG "${MAIN_FLAG} -lpthread -fopenmp")

#add_executable(perfmon_tests perfmon_tests.cpp)
#target_link_libraries(perfmon_tests PUBLIC pfc -lpthread -lnuma)
#set_target_properties(perfmon_tests PROPERTIES COMPILE_FLAGS ${TEST_FLAG})

#######################################################################################
# Tensorflow Tests
if(BUILD_TF_TEST)
    set(CMD "${mtmc_SOURCE_DIR}/scripts/build_config.sh")
    message("${CMD}")
    execute_process(COMMAND ${CMD}
            OUTPUT_VARIABLE tf_include_dir)
    message("${tf_include_dir}")

    set(TF_FLAG " -D_GLIBCXX_USE_CXX11_ABI=0 -std=c++11 -march=native -Wtarget-lifetime -fopenmp")
    # TODO: For open source, the TF include directories and lib dir should let user define
    include_directories(/home/intel/yuqiu/env/lib/python3.6/site-packages/tensorflow_core/include)
    #include_directories(${tf_include_dir})

    link_directories(${mtmc_SOURCE_DIR}/cpp/tests/tf/lib)
    link_directories(/usr/lib64)
    add_executable(mtmc_tf_tests mtmc_tf_tests.cpp)
    set_target_properties(mtmc_tf_tests PROPERTIES COMPILE_FLAGS ${TF_FLAG})
    target_link_libraries(mtmc_tf_tests PUBLIC tensorflow_framework tensorflow_cc -lpthread -fopenmp)
endif()
#target_link_libraries(mtmc_tf_tests PUBLIC ${MAIN_LIBRARY})
#set_target_properties(mtmc_tf_tests PROPERTIES COMPILE_FLAGS -L/usr/lib64)

#######################################################################################
# Unittests

add_executable(mtmc_profiler_tests mtmc_profiler_tests.cpp)
#target_link_libraries(mtmc_profiler_tests PUBLIC pfc -lpthread -lnuma -fsanitize=address -fsanitize=leak)
target_link_libraries(mtmc_profiler_tests PUBLIC pfc -lpthread -lnuma)
target_link_libraries(mtmc_profiler_tests PUBLIC ${MAIN_LIBRARY})
set_target_properties(mtmc_profiler_tests PROPERTIES COMPILE_FLAGS ${TEST_FLAG})

#######################################################################################
# Ebpf context switch
#if (EBPF_CTX_SC)
#    target_link_libraries(mtmc_profiler_tests PUBLIC pfcebpf)
#    target_link_libraries(mtmc_tf_tests PUBLIC pfcebpf)
#    target_compile_definitions(mtmc_profiler_tests PUBLIC EBPF_CTX_SC)
#    target_compile_definitions(mtmc_tf_tests PUBLIC EBPF_CTX_SC)
#endif()

#######################################################################################
# Performance Benchmarks
#add_executable(mtmc_benchmark mtmc_benchmark.cpp)
#target_link_libraries(mtmc_benchmark PUBLIC ${MAIN_LIBRARY})
#target_link_libraries(mtmc_benchmark PUBLIC pfc -lpthread)
#set_target_properties(mtmc_benchmark PROPERTIES COMPILE_FLAGS ${TEST_FLAG})