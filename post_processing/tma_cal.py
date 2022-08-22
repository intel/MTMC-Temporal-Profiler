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

def MAX(a, b):
    return max(a, b)

def MIN(a, b):
    return min(a, b)

class TmaCal(object):
    def __init__(self):
        self.equation_dict = {}
        return

    def EquLookup(self, metrics, event_names):
        return []

    def CalTopDown(self, *args):
        return ["L1,L2 Metrics not available"],[0]
