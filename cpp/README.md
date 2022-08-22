## C++ Api for MTMC profiler

MTMC profiler provides  C++ apis to collect logs for your application. The data collection is counting-base. 
You can insert collection start and stop function to cover the region that you are interested in. 

For Intel CPUs, you can specify the counters you would like to read and equations for data post processing.

## Example Useage

### 1. Write a config file
Configs for events
````
Group,X
[Event name],[event],[umask],[inv],[cmask]
...
...
End
Group,1
[Event name],[event],[umask],[inv],[cmask]
...
End
EndConfig
````
Configs equations for post-processing.
[Equ NameX] is the TMA calculation equation from post_processing/tma_cal.py
````
Equations
[Equ Name1]
[Equ Name2]
[Equ Name3]
End
````
Example
````
Group,0
Cycles,xx,xx,xx,xx
Instructions,xx,xx,xx,xx
End
Equations
CPI
Freq(GHz)
End
````
### 2. Insert your api to the code

Work flow is like this:
````
mtmc::MTMCTemprolProfiler profiler([path to your config file]);
profiler.Init();

profiler.LogStart();
...
profiler.LogEnd();

profiler.Finish([path to your export dir]);
````
If you are using thread-pool:
````
Threadpool thp();

mtmc::MTMCTemprolProfiler profiler([path to your config file]);
profiler.Init();

profiler.ExportThreadPoolInfo([Thread pool thread ids]);
ParentInfo parent_info = profiler.GetParentInfo();

auto worker = [parent_info] {
    profiler.LogStart(parent_info);
    ...
    profiler.LogEnd();
}

thp.schedule(worker);

profiler.Finish([path to your export dir]);
````
### 3. Set up guard sampler

````
mtmc::GuardSampler sampler();

sampler.Read([path to your log foler]);

...
All code that under profile
...

sampler.stop();
````

### 4. Set up environment variable before run

````
MTMC_CONFIG: The address that stores the profiler's configuration
MTMC_THREAD_EXPORT: Address of a folder that will store exported threadpool information
MTMC_PROF_DISABLE: Globally disable the profiler
````

### 5. Run your application and analysis logs

post-processing.py will generate processed log similar to tensorflow timeline. The final default output log is processed_log.json. It can be visualized by https://ui.perfetto.dev/ or chrome://tracing/,
just like tensorflow timeline