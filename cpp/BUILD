cc_library(
    name = "mtmc_profiler",
    hdrs = ["env.h", "guard_sampler.h", "mtmc_profiler.h", "perfmon_collector.h",
            "perfmon_config.h", "util.h", "mtmc_temp_profiler.h",
            "exporter.h"],
    srcs = ["mtmc_profiler.cpp", "perfmon_collector.cpp", "perfmon_config.cpp", "util.cpp", "guard_sampler.cpp",
            "exporter.cpp"],
    copts = ["-O3", "-DDEBUG_PRINT"],
    linkopts = ["-lnuma",
                "-lrt",
                "-l:/usr/local/lib/libipc.a"],
    visibility = ["//visibility:public"],
)