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

#ifndef MTMC_UTIL_H
#define MTMC_UTIL_H

#include <exception>
#include <string>
#include <iostream>
#include <cstdarg>
#include <numa.h>
#include <fstream>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <atomic>
#include <sstream>

#include "env.h"

// Print color
#define RST  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define FRED(x) KRED x RST
#define FGRN(x) KGRN x RST
#define FYEL(x) KYEL x RST
#define FBLU(x) KBLU x RST
#define FMAG(x) KMAG x RST
#define FCYN(x) KCYN x RST
#define FWHT(x) KWHT x RST

#define BOLD(x) "\x1B[1m" x RST
#define UNDL(x) "\x1B[4m" x RST

//#define DEBUG_PRINT
#ifdef DEBUG_PRINT
#define Dprintf(a, args...) mtmc::util::DprintfInternal(__FILE__, __LINE__, a, ## args);syslog(LOG_INFO|LOG_USER, a, ## args)
//#define Dprintf(a, args...) syslog(LOG_INFO|LOG_USER, a, ## args)
#define Nprintf(a, args...) printf(a, ## args)
#else
#define Dprintf(a, args...) ;
#define Nprintf(a, args...) ;
#endif


#ifdef DETAILED_PRINT
#define DDprintf(a, args...) mtmc::util::DprintfInternal(__FILE__, __LINE__, a, ## args)
#else
#define DDprintf(a, args...) ;
#endif

namespace mtmc {
namespace util {

    extern uint32_t base_freq;

    void DprintfInternal(const char* file, const int line, const char* arg, ...);

    void DprintfInternal(const std::string& str, const int line, const char* arg, ...);

    uint32_t GetBaseFreq();

    uint64_t GetNsFromTSC(uint64_t tsc);

    std::vector<int> GetCurrAvailableCPUList();
    int GetMaxNumOfCpus();

    std::string PathJoin(const std::string& p1, const std::string& p2);

    enum CheckType {
        FILE,
        DIR,
    };

    /*
     * Dump content int pBuff in a hex manner
     */
    void hex_dump(void* pBuff, unsigned int nLen);

    size_t GenHashId();

    std::vector<std::string> StringSplit(const std::string& s, char sep);

    /*int CheckPath(const std::string& path, CheckType type);

    int CheckLink(const std::string& path);*/

    // Vector with index tracer.
    template <typename T>
    class IndexVector {
    public:
        IndexVector() {
            curr_idx_ = 0;
            init_reserve_size_ = 0;
            reset_flag_.store(false, std::memory_order_release);
            release_memory_flag_.store(false, std::memory_order_release);
        }

        explicit IndexVector(size_t init_reserve_size) {
            curr_idx_ = 0;
            data_.reserve(init_reserve_size);
            init_reserve_size_ = init_reserve_size;
            reset_flag_.store(false, std::memory_order_release);
            release_memory_flag_.store(false, std::memory_order_release);
        }

        void PushBack(const T& value) {
            ActualClear();
            if (data_.size() == curr_idx_) {
                data_.push_back(value);
                ++curr_idx_;
            }
            else if (data_.size() > curr_idx_) {
                data_[curr_idx_] = value;
                ++curr_idx_;
            }
            else {
                printf("Error\n");
            }
        }

        void PushBack(T&& value) {
            ActualClear();
            if (data_.size() == curr_idx_) {
                data_.push_back(value);
                ++curr_idx_;
            }
            else if (data_.size() > curr_idx_) {
                data_[curr_idx_] = std::move(value);
                ++curr_idx_;
            }
            else {
                printf("Error\n");
            }
        }

        void Clear() {
            curr_idx_ = 0;
        }

        void ClearAndReleaseMemory() {
            Clear();
            data_.clear();
        }

        bool Empty() {
            ActualClear();
            return curr_idx_ == 0;
        }

        void AsyncClear() {
            reset_flag_.store(true, std::memory_order_release);
        }

        void AsyncClearAndReleaseMemory() {
            AsyncClear();
            release_memory_flag_.store(true);
        }

        T& Back() {
            ActualClear();
            if (curr_idx_ == 0) {
                throw std::runtime_error("Calling back() to an empty IndexVector");
            }
            else {
                return data_[curr_idx_-1];
            }
        }

        size_t Size() {
            ActualClear();
            return curr_idx_;
        }

        T& operator[](size_t n) {
            ActualClear();
            return data_[n];
        }

    private:
        inline void ActualClear() {
            if (reset_flag_.load(std::memory_order_relaxed)) {
                if (release_memory_flag_.load(std::memory_order_relaxed)) {
                    ClearAndReleaseMemory();
                    release_memory_flag_.store(false, std::memory_order_release);
                }
                else {
                    Clear();
                }
                reset_flag_.store(false, std::memory_order_release);
            }
        }

    private:
        int curr_idx_;
        std::vector<T> data_;
        size_t init_reserve_size_;

        std::atomic<bool> reset_flag_{};
        std::atomic<bool> release_memory_flag_{};

    };

    std::vector<uint64_t> HexToVec(const std::string& hex_string);

    enum CFG_FILE_TYPE {
        JSON,
        TXT,
        UNKNOWN
    };

    int CheckConfigType(const std::string& path);

    uint64_t ConvertTimeToNanoSeconds(const std::string& time_str);

    template<typename T>
    int GetNextIdxOfVec(const std::vector<T>& vec, int curr_idx) {
        return curr_idx >= vec.size() - 1 ? 0 : curr_idx + 1;
    }

    std::vector<uint8_t> GenerateUniqueTraceId(uint64_t first_hash = 0, uint64_t second_hash = 0);

    std::vector<uint8_t> GenerateUniqueSpanId(uint64_t hash_int = 0);

}
}


#endif //MTMC_UTIL_H
