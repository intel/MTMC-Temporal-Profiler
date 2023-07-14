list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cpp/OtlExportProxy")

find_path(
        Thrift_INCLUDE_DIR
        NAMES thrift/Thrift.h
        HINTS ${Thrift_HOME} ENV Thrift_HOME /usr/local /opt/local
        PATH_SUFFIXES include)

# prefer the Thrift version supplied in Thrift_HOME
find_library(
        Thrift_LIBRARIES
        NAMES thrift libthrift
        HINTS ${Thrift_HOME} ENV Thrift_HOME /usr/local /opt/local
        PATH_SUFFIXES lib lib64)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Thrift DEFAULT_MSG Thrift_LIBRARIES
        Thrift_INCLUDE_DIR)
mark_as_advanced(Thrift_LIBRARIES Thrift_INCLUDE_DIR)


if(Thrift_FOUND AND NOT (TARGET thrift::thrift))
    add_library(thrift::thrift UNKNOWN IMPORTED)
    set_target_properties(
            thrift::thrift
            PROPERTIES IMPORTED_LOCATION ${Thrift_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${Thrift_INCLUDE_DIR})
endif()

find_package(CURL REQUIRED)
find_package(opentelemetry-cpp CONFIG REQUIRED)
#find_package(Thrift REQUIRED)
#find_package(Protobuf REQUIRED)
find_package(nlohmann_json REQUIRED)

link_directories(${OPENTELEMETRY_CPP_INCLUDE_DIRS})
link_directories(/usr/local/lib/)
link_directories(/usr/local/lib64/)
include_directories(/usr/local/include)
include_directories(/usr/include)

add_executable(opentele_exporter ../util.h ../util.cpp opentele_exporter.cpp opentele_exporter.h)
set_target_properties(opentele_exporter PROPERTIES COMPILE_FLAGS " -march=native -O3")
target_link_libraries(opentele_exporter PUBLIC -lpthread ${OPENTELEMETRY_CPP_LIBRARIES} -lrt -lipc)
target_include_directories(opentele_exporter PUBLIC ${OPENTELEMETRY_CPP_INCLUDE_DIRS})

install(FILES ${CMAKE_BINARY_DIR}/bin/opentele_exporter PERMISSIONS OWNER_EXECUTE OWNER_WRITE DESTINATION ${bin_dir})