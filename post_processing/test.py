from post_processing import PerfmonParser, SingleLog
from tma_cal import SprTmaCal
import os

if __name__ == "__main__":
    parser = PerfmonParser(SprTmaCal(), "config")

    with open(os.path.join("test"), 'r') as f:
        logs = f.readlines()
        for line in logs:
            sig_prof = line.split(',')
            sig_log = SingleLog(sig_prof)

            tmam_names, output_tmam = parser.CalTopDown(sig_log.topdown_metrics[0],
                                                                sig_log.topdown_metrics[1],
                                                                sig_log.topdown_metrics[2],
                                                                sig_log.topdown_metrics[3],
                                                                sig_log.topdown_metrics[4],
                                                                sig_log.topdown_metrics[5])

            output_dict = {}

            output_dict["Dur"] = sig_log.time_end - sig_log.time_begin
            output_dict["Core"] = f"{sig_log.b_core}_{sig_log.e_core}"
            output_dict["Raw_tma"] = sig_log.topdown_metrics
            output_dict["Raw pmc_b"] = sig_log.b_events
            output_dict["Raw pmc_e"] = sig_log.e_events

            assert (len(tmam_names) == len(output_tmam))
            for i in range(len(tmam_names)):
                output_dict[tmam_names[i]] = output_tmam[i]

            parser.CalPmu(pmu_begin=sig_log.b_events, pmu_end=sig_log.e_events,
                          dict_to_append=output_dict)

            print("-"*20)
            for key, value in output_dict.items():
                print(f"{key}: {value}")



    print("Done")