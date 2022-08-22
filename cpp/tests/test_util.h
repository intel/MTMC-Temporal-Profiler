//
// Created by yuqiuzha on 6/10/2022.
//

#ifndef MTMC_TEST_UTIL_H
#define MTMC_TEST_UTIL_H

#include <cassert>
#include <x86intrin.h>
#include <immintrin.h>
#include <chrono>
#include <atomic>
#include <dirent.h>

#define GET_METRIC(m, i) (((m) >> (i*8)) & 0xff)

/// L1 Topdown metric events
#define TOPDOWN_RETIRING(val)	((double)GET_METRIC(val, 0) / 0xff)
#define TOPDOWN_BAD_SPEC(val)	((double)GET_METRIC(val, 1) / 0xff)
#define TOPDOWN_FE_BOUND(val)	((double)GET_METRIC(val, 2) / 0xff)
#define TOPDOWN_BE_BOUND(val)	((double)GET_METRIC(val, 3) / 0xff)

/// L2 Topdown metric events.
/// Available on Sapphire Rapids and later platforms
#define TOPDOWN_HEAVY_OPS(val)		((double)GET_METRIC(val, 4) / 0xff)
#define TOPDOWN_BR_MISPREDICT(val)	((double)GET_METRIC(val, 5) / 0xff)
#define TOPDOWN_FETCH_LAT(val)		((double)GET_METRIC(val, 6) / 0xff)
#define TOPDOWN_MEM_BOUND(val)		((double)GET_METRIC(val, 7) / 0xff)

namespace testutl {

    void Assert(bool val, const std::string& test_name) {
        if (val) {
            printf(UNDL(FGRN("%s : Pass\n")), test_name.c_str());
        }
        else {
            printf(FRED("%s : Failed\n"), test_name.c_str());
        }
    }

    typedef std::chrono::time_point<std::chrono::system_clock> TP;
    typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::microseconds> TPUS;
    typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::nanoseconds> TPNS;
    inline TP GetTimeStamp() {
        return std::chrono::system_clock::now();
    }

    template<typename TPType>
    inline std::time_t GetUsFromTP(const TPType& tp) {
        TPUS tpus = std::chrono::time_point_cast<std::chrono::microseconds>(tp);
        return tpus.time_since_epoch().count();
    }

    inline std::time_t GetTimeStampUs() {
        return GetUsFromTP(GetTimeStamp());
    }

    /**
     *  Helper class that creates all kinds of job to test the correctness of perfmon collector
     */
    class Jobs {
        int arrSize_;
        int* array0_;
        int* array1_;
        int16_t* array_bf160_;
        int16_t* array_bf161_;
        std::vector<std::string> job_names_;
        std::atomic<int> job_done;

    public:
        Jobs() {
            arrSize_ = 1024 * 1024;
            array0_ = (int*) malloc(sizeof(int)*arrSize_);
            array1_ = (int*) malloc(sizeof(int)*arrSize_);
            for (int i = 0; i < sizeof(array0_); ++i) array0_[i] = 2;
            for (int i = 0; i < sizeof(array1_); ++i) array1_[i] = 3;
            job_names_ = {
                    "MemoryCopy", "Multiply"
            };
            job_done = 0;
        }

        ~Jobs() {
            free(array0_);
            free(array1_);
        }

        enum JOB_TYPE {
            MEMCPY = 0,
            MULTIPLY = 1
        };

        void DoJob(JOB_TYPE job, int times) {
            if (job == MEMCPY) {
                for (int i = 0; i < times; ++i) MemoryCopy();
            }
            else if (job == MULTIPLY) {
                for (int i = 0; i < times; ++i) Multiply();
            }
        }

        void DoJobForPeriod(JOB_TYPE job, time_t us) {
            auto startus = GetTimeStampUs();
            while(GetTimeStampUs() - startus < us) {
                DoJob(job, 1);
            }
            job_done.fetch_add(1);
        }

        void BlockUntilNJobsDone(int n) {
            while(job_done.load() < n) {
            }
        }

    private:

        __attribute__((optimize("O0"))) void MemoryCopy() {
            auto array_ret = (int*) malloc(sizeof(int)*arrSize_);
            memcpy(array_ret, array0_, sizeof(int)*arrSize_);
            memcpy(array_ret, array1_, sizeof(int)*arrSize_);
            free(array_ret);
        }

        __attribute__((optimize("O0"))) int Multiply() {
            int ret;
            for (int i = 0; i < arrSize_; ++i) {
                ret += array0_[i] * array1_[i];
            }
            return ret;
        }

    };

    struct TopDownRet {
        uint64_t  slots;
        uint64_t  metric;
    };

    struct PerfMetricsResult {
        double retiringRatio;
        double badSpecRatio;
        double feBoundRatio;
        double beBoundRatio;
        double heavuOpsRatio;
        double lightOpsRatio;
        double brMispredictRatio;
        double machineClearsRatio;
        double fetchLatRatio;
        double fetchBwRatio;
        double memBoundRatio;
        double coreBoundRatio;
    };

    PerfMetricsResult calDelta(TopDownRet a, TopDownRet b) {
        auto slots_a = a.slots;
        auto slots_b = b.slots;
        auto metric_a = a.metric;
        auto metric_b = b.metric;
        assert(slots_b >= slots_a);

        /// compute delta scaled metrics between b and a
        printf("%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n", GET_METRIC(metric_a, 0),GET_METRIC(metric_a, 1),
               GET_METRIC(metric_a, 2), GET_METRIC(metric_a, 3), GET_METRIC(metric_a, 4),GET_METRIC(metric_a, 5),
               GET_METRIC(metric_a, 6), GET_METRIC(metric_a, 7));
        printf("%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n", GET_METRIC(metric_b, 0),GET_METRIC(metric_b, 1),
               GET_METRIC(metric_b, 2), GET_METRIC(metric_b, 3), GET_METRIC(metric_b, 4),GET_METRIC(metric_b, 5),
               GET_METRIC(metric_b, 6), GET_METRIC(metric_b, 7));

        printf("Slots: %lu, %lu\n", slots_a, slots_b);
        printf("Retiring: %f, %f\n", TOPDOWN_RETIRING(metric_a), TOPDOWN_RETIRING(metric_b));
        printf("Bed Spec: %f, %f\n", TOPDOWN_BAD_SPEC(metric_a), TOPDOWN_BAD_SPEC(metric_b));
        printf("FE: %f, %f\n", TOPDOWN_FE_BOUND(metric_a), TOPDOWN_FE_BOUND(metric_b));
        printf("BE: %f, %f\n", TOPDOWN_BE_BOUND(metric_a), TOPDOWN_BE_BOUND(metric_b));


        auto retiring_slots = TOPDOWN_RETIRING(metric_b) * slots_b - TOPDOWN_RETIRING(metric_a) * slots_a;
        auto bad_spec_slots = TOPDOWN_BAD_SPEC(metric_b) * slots_b - TOPDOWN_BAD_SPEC(metric_a) * slots_a;
        auto fe_bound_slots = TOPDOWN_FE_BOUND(metric_b) * slots_b - TOPDOWN_FE_BOUND(metric_a) * slots_a;
        auto be_bound_slots = TOPDOWN_BE_BOUND(metric_b) * slots_b - TOPDOWN_BE_BOUND(metric_a) * slots_a;
        printf("%f, %f, %f, %f\n", retiring_slots, bad_spec_slots, fe_bound_slots, be_bound_slots);

        /// Compute ratio
        auto slots_delta = slots_b - slots_a;
        auto retiring_ratio = ((double)retiring_slots / slots_delta);
        auto bad_spec_ratio = ((double)bad_spec_slots / slots_delta);
        auto fe_bound_ratio = ((double)fe_bound_slots / slots_delta);
        auto be_bound_ratio = ((double)be_bound_slots / slots_delta);

        printf("Retiring %.2f%%\nBad Speculation %.2f%%\nFE Bound %.2f%%\nBE Bound %.2f%%\n",
               retiring_ratio * 100.,
               bad_spec_ratio * 100.,
               fe_bound_ratio * 100.,
               be_bound_ratio * 100.);

        /// Compute L2 metrics
        auto heavy_ops_slots = TOPDOWN_HEAVY_OPS(metric_b) * slots_b - TOPDOWN_HEAVY_OPS(metric_a) * slots_a;
        auto br_mispredict_slots = TOPDOWN_BR_MISPREDICT(metric_b) * slots_b - TOPDOWN_BR_MISPREDICT(metric_a) * slots_a;
        auto fetch_lat_slots = TOPDOWN_FETCH_LAT(metric_b) * slots_b - TOPDOWN_FETCH_LAT(metric_a) * slots_a;
        auto mem_bound_slots = TOPDOWN_MEM_BOUND(metric_b) * slots_b - TOPDOWN_MEM_BOUND(metric_a) * slots_a;

        auto heavy_ops_ratio = (double)heavy_ops_slots / slots_delta;
        auto light_ops_ratio = retiring_ratio - heavy_ops_ratio;

        auto br_mispredict_ratio = (double)br_mispredict_slots / slots_delta;
        auto machine_clears_ratio = bad_spec_ratio - br_mispredict_ratio;

        auto fetch_lat_ratio = (double)fetch_lat_slots / slots_delta;
        auto fetch_bw_ratio = fe_bound_ratio - fetch_lat_ratio;

        auto mem_bound_ratio = (double)mem_bound_slots / slots_delta;
        auto core_bound_ratio = be_bound_ratio - mem_bound_ratio;

        printf("Heavy Operations %.2f%%\nLight Operations %.2f%%\n"
               "Branch Mispredict %.2f%%\nMachine Clears %.2f%%\n"
               "Fetch Latency %.2f%%\nFetch Bandwidth %.2f%%\n"
               "Mem Bound %.2f%%\nCore Bound %.2f%%\n",
               heavy_ops_ratio * 100.,
               light_ops_ratio * 100.,
               br_mispredict_ratio * 100.,
               machine_clears_ratio * 100.,
               fetch_lat_ratio * 100.,
               fetch_bw_ratio * 100.,
               mem_bound_ratio * 100.,
               core_bound_ratio * 100.);
        return PerfMetricsResult{
                retiring_ratio * 100.,
                bad_spec_ratio * 100.,
                fe_bound_ratio * 100.,
                be_bound_ratio * 100.,
                heavy_ops_ratio * 100.,
                light_ops_ratio * 100.,
                br_mispredict_ratio * 100.,
                machine_clears_ratio * 100.,
                fetch_lat_ratio * 100.,
                fetch_bw_ratio * 100.,
                mem_bound_ratio * 100.,
                core_bound_ratio * 100.
        };
    }


}

#endif //MTMC_TEST_UTIL_H
