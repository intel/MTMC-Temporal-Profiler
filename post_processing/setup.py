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

import os
from setuptools import setup, find_packages

__version__ = '1.0' # version
requirements = open('requirements.txt').readlines() # dependency

setup(
    name = 'MTMC-temporal-profiler-post-processing',
    version = __version__,
    author = 'Zhou, Huan',
    author_email = 'huan.h.zhou@intel.com',
    url = '',
    description = 'MTMC-temporal-profiler-post-processing: post processing tool for MTMC-temporal-profiler',
    packages = find_packages(exclude=["tests"]),
    python_requires = '>=3.6.0',
    install_requires = requirements
        )
