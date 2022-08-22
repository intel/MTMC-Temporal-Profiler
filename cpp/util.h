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

    /*int CheckPath(const std::string& path, CheckType type);

    int CheckLink(const std::string& path);*/

}
}


#endif //MTMC_UTIL_H
