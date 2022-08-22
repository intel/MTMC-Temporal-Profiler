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
extra_build_opt=$1


echo "Project root: $projroot"
echo "Extra build opts: $extra_build_opt"

cd $projroot
mkdir build

#cd build && rm -rf * && cmake .. -DDPRINT_ENABLE=ON -DD_DPRINT_ENABLE=ON && make -j`nproc` && make install
cd build && rm -rf * && cmake .. -DDPRINT_ENABLE=ON $extra_build_opt && make -j`nproc` && make install

#echo $(pwd)