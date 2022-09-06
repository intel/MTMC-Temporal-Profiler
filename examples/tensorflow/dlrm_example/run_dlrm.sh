filepath=$(cd "$(dirname "$0")"; pwd)
projroot=$(cd $filepath; cd ../../..; pwd)

echo $projroot

cd $projroot/scripts/
bash rebuild.sh -DBUILD_TEST=ON

logfolder=$filepath/logs
pbfile=$projroot/cpp/tests/dlrm_keras.pbtxt
thpfolder=$filepath/thp_info
configfile=$filepath/config_examples/$5

#mkdir logfolder
#mkdir thpfolder
#cd logfolder && rm -rf *
#cd thpfolder && rm -rf *
cd $projroot

rm -rf $thpfolder
mkdir $thpfolder
export MTMC_THREAD_EXPORT=$thpfolder

rm -rf $logfolder
mkdir $logfolder

export MTMC_CONFIG=$configfile

cd $projroot/build/bin && numactl -N 0 ./mtmc_tf_tests $1 $2 $3 $4 $logfolder $pbfile

# Post processing
cd $filepath && python $projroot/post_processing/post_processing.py -l $logfolder -t $thpfolder -c $configfile