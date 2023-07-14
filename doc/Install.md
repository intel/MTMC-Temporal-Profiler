# Install MTMC-Temp-Profiler

This document will guide you to install the MTMC-Temp-Profiler. We created this step-by-step guide from a Centos Stream OS.

## Requirements

The profiler is compatible with Xeon later than ICX generation.

Please make sure you have GCC >= 8.5 and CMake >= 3.12

## Install Dependency

Install thrift v0.17.0:
````
wget https://github.com/apache/thrift/archive/refs/tags/v0.17.0.tar.gz
tar -zxf v0.17.0.tar.gz
cd thrift-0.17.0/
dnf install libtool-devel gcc-c++ boost-devel
./bootstrap.sh
make -j
make install
thrift --version # should show 0.17.0
````
Install nlohmann_json:
````
wget https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
tar -zxf v3.11.2.tar.gz
cd json-3.11.2/
mkdir build
cd build
cmake ..
make -j
make install
````
Install cpp-ipc:
````
git clone https://github.com/mutouyun/cpp-ipc
cd cpp-ipc
git checkout 768e58f60502efce0b9f1f8d2c31482ea99a01a9
mkdir build
cd build && cmake .. && make -j
make install
````
Install Opentelemetry-cpp. Currently, the profiler is built on a specific version. If you
already have Opentelemetr-cpp installed, it may be not completable with MTMC.
````
git clone --recurse-submodules https://github.com/open-telemetry/opentelemetry-cpp
cd opentelemetry-cpp
git checkout 1d18614edba76d24880de7cb954abf5e7968d8e3
dnf install libcurl-devel
mkdir build
cd build
cmake .. -DBUILD_TESTING=OFF -DWITH_JAEGER=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=11
make -j
make install
````
Deploy Jaeger all-in-one from https://www.jaegertracing.io/docs/1.6/getting-started/
 and make sure the Jaeger docker is up. In our testing, we run the Jaeger with following command:
````
docker run -d --name jaeger \
    -e COLLECTOR_ZIPKIN_HTTP_PORT=9411 \
    -p 5775:5775/udp \
    -p 6831:6831/udp \
    -p 6832:6832/udp \
    -p 5778:5778 \
    -p 16686:16686 \
    -p 14268:14268 \
    -p 9411:9411 \
    jaegertracing/all-in-one:1.39
````

## Install MTMC-Temp-Profiler
````
mkdir build && cd build
cmake .. -DDPRINT_ENABLE=ON # Set if you want to show message for debugging
make -j
make install
````
