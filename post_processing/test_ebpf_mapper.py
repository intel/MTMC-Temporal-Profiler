import post_processing as pp

if __name__ == "__main__":
    addr = "/home/intel/work/mtmc/cpp/tests/test_logs/mtmc_tf_tests_logs/ebpf.txt"
    ebpf_mapper = pp.EbpfMapper()
    ebpf_mapper.CoreThreadDataToTimeline(addr, "/home/intel/work/mtmc/cpp/tests/test_logs/mtmc_tf_tests_logs/ebpf_log.json")
    print("Done")