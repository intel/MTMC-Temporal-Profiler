# MTMC Temporal profier raw log

The logs that being exported by C++ Finsh() api is defined as the raw log of the profiler.
The raw log contains user space information that can be collected by the profiler. 

## Raw log composition

The detail composition of the raw log is illustrated in this section. First, here is the structure of the raw log:


The raw log is a ',' seperated list of section. Each section can have '_' sepreated subsections. Here [A] indicates section A and [A.B] indicate section A subsection B.
```
[0],[1],[2],[3],[4],[5],[6],[7.0]_[7.1]_[7.2]_[7.3]_[7.4]_[7.5],[8.0]_[8.1]_[8.2],[9.0]_..._[9.N],[10.0]_[10.1]_[10.2],[11.0]_..._[11.N],
```

For example:
```
1107138,1602152192,1662113994732217088,1662113994733739014,1107181,1241265920,1662113994732206443,3786_3411785758808611119_140737493906307_18412794_13836746939049903115_140737493909085,11_12_0,1731201101_3690717719_1463852088_10519267_10356736_3476234_356044729_6979630,11_12_0,1734269057_3691050936_1466446788_13052429_12885932_5936761_358428561_6987354,
```

Here is the **detailed explentation** of each section:

* [0]: Kernel thread id of the task.
* [1]: 32bit unsigned pthread id of the task.
* [2]: Starting time of the task(Ns, Clock realtime).
* [3]: Ending time of the task(Ns, Clock realtime).
* [4]: Kernel thread id of the task's parent task(The task that scheduled this task).
* [5]: 32bit unsigned pthread id of the task's parent task.
* [6]: Time when the task being scheduled(Ns, Clock realtime).
* [7]: Topdown telemetry information. If you are using Intel 12th Gen Core, Xeon Ice Lake server or later generation, this part is the value from Perf_Metrics MSR. You can get more details about the MSR and Topdown metric calculations from Intel Software developer's manual Vol. 3b Chapter 19 Performance Monitoring 19.3.10.1.1 Perf Metrics Extensions.
  * [7.0] Topdown Slots event reading at the **beginning** of the task.
  * [7.1] Topdown metric event reading at the **beginning** of the task.
  * [7.2] Reserved
  * [7.3] Topdown Slots event reading at the **ending** of the task.
  * [7.4] Topdown metric event reading at the **ending** of the task.
  * [7.5] Reserved
* [8]: General information for collection at the **beginning** of the task
  * [8.0] Number of events being collected at the **beginning**.
  * [8.1] The core id when the event is being collected.
  * [8.2] Prefix information for this task (Not implemented in V0.1. Later will support it as a user defined unique information of the task) 
* [9]: Event counter reading at the **beginning** of the task
  * [9.0] Event counter No.0 reading.
  * ...
  * [9.N] Event counter No.N reading.
* [10]: General information for collection at the **ending** of the task
    * [10.0] Number of events being collected at the **ending**.
    * [10.1] The core id when the event is being collected.
    * [10.2] Prefix information for this task (Not implemented in V0.1. Later will support it as a user defined unique information of the task)
* [11]: Event counter reading at the **ending** of the task
    * [11.0] Event counter No.0 reading.
    * ...
    * [11.N] Event counter No.N reading.
  
