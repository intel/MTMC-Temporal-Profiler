import os, time
import subprocess as sp
import json
import argparse

def read_config(config_addr):
    with open(config_addr, 'r') as f:
        cfg_json = json.load(f)

    if not 'Configs' in cfg_json.keys():
        raise ValueError(f"No configs were found in {config_addr}")

    return cfg_json

def single_run(sig_cfg, command, store_path):
    popen = sp.Popen(command, shell=True, env={"MTMC_CONFIG": sig_cfg,
                                               "MTMC_LOG_EXPORT_PATH": store_path})
    popen.wait()

def get_trace_hash(export_mode):
    if export_mode == 3:
        # All iterations share the same hash id
        return hash(str(time.time_ns())+str(os.getpid()))
    else:
        # when hash is 0, the exporter will generate unique hash_id for each individual iteration
        return 0

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", help="Configuration json file path.", type=str, required=True)
    parser.add_argument("-p", help="Export directory.", type=str, required=True)
    parser.add_argument("-r", help="Command to run the workload.", type=str, required=True)
    parser.add_argument("--mode", help="Export mode.\n0: Disable automatic export.\n"
                                       "1: Export raw results to disk.\n"
                                       "2: Export to Jaeger. If Event requires mux, each iterations will be a unique trace.\n"
                                       "3: Export to Jaeger. If Event requires mux, all iterations will be fused to a single trace.\n",
                        type=int,
                        default=0)
    parser.add_argument("--mux", help="Turn on events multiplexing. Default is true", default=False, action="store_true")
    args = parser.parse_args()

    # Variables from arguments
    abs_script_dir = os.path.dirname(os.path.abspath(__file__))
    cfg_path = args.c
    log_path = args.p
    command = args.r
    export_mode = int(args.mode)
    event_mux = args.mux

    # Some sanity checks
    if not os.path.isfile(cfg_path):
        raise ValueError("-c should be a config file end with .json")
    if os.path.isfile(log_path):
        raise ValueError("-p should be a directory, but got a file instead")

    cfg_json = read_config(cfg_path)

    # Export Mode selection
    if export_mode == 0:
        print("Export mode 0. Will not do automatic export. Please insert export code by yourself.")
        pass
    elif export_mode == 1:
        if os.getenv("MTMC_LOG_EXPORT_PATH") is not None:
            log_path = os.environ["MTMC_LOG_EXPORT_PATH"]
        print("Export mode 1. Will export raw logs to:\n%s" % (log_path,))
    else:
        print("Export mode %d." % (export_mode,))

    trace_hash = get_trace_hash(export_mode)

    # Mux settings
    if event_mux:
        configs = cfg_json['Configs']
        num_cfgs = len(configs)
        print("Will do event multiplexing outside over %d configs." % (num_cfgs,))

        modified_configs = []
        for idx, cfg in enumerate(configs):
            full_cfg = {
                'Configs': [cfg],
                'ExportMode': export_mode,
                'TraceHash': trace_hash,
                'ConfigsId': idx,
                # ConfigsId is used only for event mux. Put -1 or leave it alone if not using multiplexing
                'SwitchIntvl': "0ms"
            }

            itr_cfg = os.path.join(abs_script_dir, 'temp_cfg.json')
            with open(itr_cfg, 'w') as f:
                json.dump(full_cfg, f, indent=4)
                print("Dump config %d to %s" % (idx, itr_cfg))
            single_run(itr_cfg, command, os.path.join(log_path, f"mtmc_temp_{idx}.txt"))

            # mtmc_profiler cpp will modify and rearrange configs if necessary. So store a new configs
            new_itr_cfg = read_config(itr_cfg)
            modified_configs += new_itr_cfg['Configs']
    else:
        print("Will not do event multiplexing outside.")
        single_run(cfg_path, command, os.path.join(log_path, f"mtmc_temp_0.txt"))


if __name__ == "__main__":
    main()