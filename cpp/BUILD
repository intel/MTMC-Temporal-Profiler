cc_library(
    name = "mtmc_profiler",
    hdrs = ["env.h", "guard_sampler.h", "mtmc_profiler.h", "perfmon_collector.h", "perfmon_config.h", "util.h", "mtmc_temp_profiler.h"],
    srcs = ["mtmc_profiler.cpp", "perfmon_collector.cpp", "perfmon_config.cpp", "util.cpp", "guard_sampler.cpp"],
    copts = ["-O3",],
    linkopts = ["-lnuma"],
    visibility = ["//visibility:public"],
)