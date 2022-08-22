file_path="$(cd "$(dirname "$0")"; pwd)"
patch_path="${file_path}/mtmc_tf115_build.patch"
eigen_patch="${file_path}/eigen.patch"
tf_dir=$1
eigen_path="${tf_dir}/bazel-tensorflow/external/eigen_archive/unsupported/Eigen/CXX11/src/ThreadPool/"
echo Tensorflow 1.15.0 source code dir: $tf_dir
echo Patch dir: $patch_path
echo Eigen path: $eigen_path

echo Apply patch..

cd $tf_dir && git apply ${patch_path}

echo Create link

cd $file_path/../../..
cpp_path=$(pwd)/cpp
cd $tf_dir
ln -s ${cpp_path} mtmc_profiler

echo apply mtmc..

#cp -f $eigen_patch $eigen_path
cd $eigen_path && patch -l < $eigen_patch

echo Done!