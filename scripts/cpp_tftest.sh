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

filepath=$(cd "$(dirname "$0")"; pwd)
projroot=$(cd $filepath; cd ..; pwd)

cd $filepath
#./rebuild.sh " -DBUILD_TEST=ON -DEBPF_CTX_SC=ON"
./rebuild.sh " -DBUILD_TEST=ON"
#./rebuild.sh "-DBUILD_TEST=ON -DEBPF_CTX_SC=ON -DCMAKE_BUILD_TYPE=Debug"

mkdir $projroot/cpp/tests/test_logs/thp_info/
rm -f $projroot/cpp/tests/test_logs/thp_info/*
export MTMC_THREAD_EXPORT=$projroot/cpp/tests/test_logs/thp_info/

logfolder=$projroot/cpp/tests/test_logs/mtmc_tf_tests_logs
pbfile=$projroot/cpp/tests/dlrm_keras.pbtxt
mkdir $logfolder

export MTMC_CONFIG=$projroot/cpp/tests/configs/configExampleCaches.txt

cd $projroot/build/bin && numactl -C 0-23,96-119 ./mtmc_tf_tests 100 10 8 8 $logfolder $pbfile

thpfolder=$projroot/cpp/tests/test_logs/thp_info/
configfile=$projroot/cpp/tests/configs/configExampleCaches.txt
ebpffile=$logfolder/ebpf.txt

cd $filepath && python $projroot/post_processing/post_processing.py -l $logfolder -t $thpfolder -c $configfile -e $ebpffile