//Copyright 2022 Intel Corporation
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//        limitations under the License.

#include "util.h"

namespace mtmc {
namespace util {

    uint32_t base_freq = 0;

    void DprintfInternal(const char *file, const int line, const char *arg, ...) {
#ifdef DEBUG_MODE
        va_list ap;
        va_start(ap, arg);
        printf("[%s:%d] ", file, line);
        vprintf(arg, ap);
        va_end(ap);
#endif
    }

    void DprintfInternal(const std::string &str, const int line, const char *arg, ...) {
        const char* file = str.c_str();
#ifdef DEBUG_MODE
        va_list ap;
        va_start(ap, arg);
        printf("[%s:%d] ", file, line);
        vprintf(arg, ap);
        va_end(ap);
#endif
    }

    uint32_t GetBaseFreq() {
        if (!base_freq) {
            std::ifstream ss;
            ss.open(("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency"), std::ios::in);
            if (ss.fail()) {
                Dprintf(FRED("Failed to open filesystem to get cpu base frequency\n"));
                return 0;
            }
            std::string line;
            if (ss.eof()) {
                Dprintf(FRED("Failed to get cpu base frequency, the base_frequency file system is empty\n"));
                return 0;
            }
            std::getline(ss,line);
            base_freq = std::stoul(line);
        }
        return base_freq;
    }

    uint64_t GetNsFromTSC(uint64_t tsc) {
        return tsc * GetBaseFreq();
    }

    std::vector<int> GetCurrAvailableCPUList() {
        // TODO: Current implementation must use NUMA library (libnuma), later can be more flexable by reading /proc/self/status directly
        std::vector<int> cpu_num;
        auto ptr = (uint8_t *) (numa_all_cpus_ptr->maskp);
        for (int i = 0; i < numa_all_cpus_ptr->size; i += (sizeof(uint8_t)*8) ) {
            for (int j = 0; j < sizeof(uint8_t)*8; ++j) {
                auto flag = ((*ptr) >> j) & 0b1;
                if (flag) cpu_num.push_back(i+j);
            }
            ptr += 1;
        }
        return cpu_num;
    }

    int GetMaxNumOfCpus() {
        // TODO: Current implementation requires NUMA library (libnuma), later can be more flexable
        return numa_num_possible_cpus();
    }

    std::string PathJoin(const std::string& p1, const std::string& p2) {
        if (*(p1.end()-1) == '/') return p1 + p2;
        else return p1 + "/" + p2;
    }

    // Path to a file. Need to check if the path before the file exists
    // CAN NOT HANDLE RACE CONDITION
    /*int CheckPath(const std::string& path, CheckType type) {
//        Dprintf(FYEL("Check path:, %s\n"), path.c_str());

        if (path.empty()) return -1;

        // Check if the path before file exists
        size_t last_seg_idx = path.rfind('/');

        if (last_seg_idx == std::string::npos) return -1;

        std::string path_to_dir = path.substr();

        struct stat ret{};

        if (lstat(path.c_str(), &ret) == -1) {
            return -1;
        }
        else {
//            Dprintf(FYEL("Is link: %d, Is file %d, Is dir %d\n"), S_ISLNK(ret.st_mode), S_ISREG(ret.st_mode), S_ISDIR(ret.st_mode));
            // Link is always prohibited
            if (S_ISLNK(ret.st_mode)) return -1;

            // Regular file
            if (type == CheckType::FILE && !S_ISREG(ret.st_mode)) return -1;

            // Directory
            if (type == CheckType::DIR && !S_ISDIR(ret.st_mode)) return -1;
        }

        return 1;
    }

    int CheckLink(const std::string& path) {
        int fd = open(path.c_str(), O_RDWR|O_NOFOLLOW);
        if (fd == -1) {
            return -1;
        }
        close(fd);
        return 1;
    }*/

}
}
