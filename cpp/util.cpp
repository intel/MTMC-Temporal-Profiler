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

    // Code from https://github.com/iovisor/bcc/tree/master/src/cc/common.cc
    std::vector<int> read_cpu_range(const std::string& path) {
        std::ifstream cpus_range_stream { path };
        std::vector<int> cpus;
        std::string cpu_range;

        while (std::getline(cpus_range_stream, cpu_range, ',')) {
            std::size_t rangeop = cpu_range.find('-');
            if (rangeop == std::string::npos) {
                cpus.push_back(std::stoi(cpu_range));
            }
            else {
                int start = std::stoi(cpu_range.substr(0, rangeop));
                int end = std::stoi(cpu_range.substr(rangeop + 1));
                for (int i = start; i <= end; i++)
                    cpus.push_back(i);
            }
        }
        return cpus;
    }

    std::vector<int> GetCurrAvailableCPUList() {
        return read_cpu_range("/sys/devices/system/cpu/online");
    }

    int GetMaxNumOfCpus() {
        return read_cpu_range("/sys/devices/system/cpu/possible").size();
    }

    std::string PathJoin(const std::string& p1, const std::string& p2) {
        if (*(p1.end()-1) == '/') return p1 + p2;
        else return p1 + "/" + p2;
    }

    void hex_dump(void *pBuff, unsigned int nLen) {
        if (NULL == pBuff || 0 == nLen)
        {
            return;
        }
        const int nBytePerLine = 16;
        unsigned char* p = (unsigned char*)pBuff;
        char szHex[3*nBytePerLine+1] = {0};

        printf("-------------------hex_dump---------------------\n");
        for (unsigned int i=0; i<nLen; ++i)
        {
            int idx = 3 * (i % nBytePerLine);
            if (0 == idx)
            {
                memset(szHex, 0, sizeof(szHex));
            }
            snprintf(&szHex[idx], 4, "%02x ", p[i]);

            if (0 == ((i+1) % nBytePerLine))
            {
                printf("%s\n", szHex);
            }
        }

        if (0 != (nLen % nBytePerLine))
        {
            printf("%s\n", szHex);
        }

        printf("------------------hex_dump end-------------------\n");
    }

    size_t GenHashId() {
        static std::hash<std::string> hasher;
        thread_local std::string pid;
        if (pid.empty()) {
            pid = std::to_string(getpid());
        }
        return hasher(pid + std::to_string(Env::rdtsc()));
    }

    std::vector<std::string> StringSplit(const std::string& s, char sep) {
        std::vector<std::string> fields;
        std::string field;
        for (char i : s) {
            if (i == sep) {
                fields.push_back(field);
                field.clear();
            }
            else {
                field += i;
            }
        }
        fields.push_back(field);
        return fields;
    };

    std::vector<uint64_t> HexToVec(const std::string& hex_string) {

        std::vector<uint64_t> nums;
        std::stringstream ss(hex_string);
        std::string token;
        while (std::getline(ss, token, ',')) {
            uint64_t num;
            std::stringstream(token) >> std::hex >> num;
            nums.push_back(num);
        }

//    for (auto& num : nums) {
//        std::cout << std::hex << "0x" << num << std::endl;
//    }

        return nums;
    }

    int CheckConfigType(const std::string& path) {
        std::string file_suffix = path.substr(path.find_last_of(".") + 1);

        if (file_suffix == "json") {
            return CFG_FILE_TYPE::JSON;
        } else if (file_suffix == "txt") {
            return CFG_FILE_TYPE::TXT;
        } else {
            return CFG_FILE_TYPE::UNKNOWN;
        }
    }

    uint64_t ConvertTimeToNanoSeconds(const std::string& time_str) {
        uint64_t ns;
        uint64_t num;
        std::string unit;
        std::istringstream ss(time_str);

        ss >> num >> unit;
        if (unit == "ns") {
            ns = num;
        } else if (unit == "us") {
            ns = num * 1000;
        } else if (unit == "ms") {
            ns = num * 1000000;
        } else if (unit == "s") {
            ns = num * 1000000000;
        } else {
            throw std::invalid_argument("Invalid time unit");
        }

        return ns;
    }

    std::vector<uint8_t> GenerateUniqueTraceId(uint64_t first_hash, uint64_t second_hash) {
        if (first_hash == 0)
            first_hash = mtmc::util::GenHashId();
        if (second_hash == 0)
            second_hash = mtmc::util::GenHashId();
        std::vector<uint8_t> hash_arr(16);
        memcpy(hash_arr.data(), &first_hash, sizeof(first_hash));
        memcpy(hash_arr.data()+8, &second_hash, sizeof(second_hash));
        return hash_arr;
    };

    std::vector<uint8_t> GenerateUniqueSpanId(uint64_t hash_int) {
        if (hash_int == 0)
            hash_int = mtmc::util::GenHashId();
        std::vector<uint8_t> hash_arr(8);
        memcpy(hash_arr.data(), &hash_int, sizeof(hash_int));
        return hash_arr;
    };

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
