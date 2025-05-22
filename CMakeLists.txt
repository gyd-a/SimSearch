cmake_minimum_required(VERSION 3.10)
project(ServiceDiscovery)
# 指定 C++17 标准，并禁用 GNU 扩展（使用 -std=c++17 而不是 gnu++17）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# 日志打印绝对路径
option(WITH_GLOG "With glog" ON)
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.)
if(WITH_GLOG)
    set(WITH_GLOG_VAL "1")
    set(BRPC_WITH_GLOG 1)
endif()

# 设置默认构建类型为 Release（仅当用户未设置时）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_BUILD)
endif()

# 为不同构建类型设置编译选项（Release/Debug）
# 支持 GCC / Clang / MSVC
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # GCC 或 Clang
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g")
elseif(MSVC)
    # MSVC 编译器
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "/Od /Zi")
endif()

if(NOT DEFINED ENV{BRPC_HOME})
    message(FATAL_ERROR "BRPC_HOME isn't set! ")
    exit()
endif()
message("BRPC_HOME = $ENV{BRPC_HOME}")

if(NOT DEFINED ENV{BRAFT_HOME})
    message(FATAL_ERROR "BRAFT_HOME isn't set! ")
    exit()
endif()
message("BRAFT_HOME = $ENV{BRAFT_HOME}")

configure_file(${CMAKE_SOURCE_DIR}/scripts/clear_cach.sh
               ${CMAKE_BINARY_DIR}/clear_cach.sh
               COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/config.toml
               ${CMAKE_BINARY_DIR}/config.toml
               COPYONLY)

# 如果 brpc 或 protobuf 安装路径不在标准位置，可以通过以下方式指定路径
link_directories(
    $ENV{BRPC_HOME}/lib64
    $ENV{BRPC_HOME}/lib
    $ENV{BRAFT_HOME}/lib
)

include_directories(
    $ENV{BRPC_HOME}/include
    $ENV{BRAFT_HOME}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/idl/gen_idl
    ${CMAKE_SOURCE_DIR}/third_party/toml11/include
)
file(GLOB_RECURSE IDL_SRCS
    ${CMAKE_SOURCE_DIR}/idl/gen_idl/etcd_idl/*.pb.cc
    ${CMAKE_SOURCE_DIR}/idl/gen_idl/node_idl/*.pb.cc
    ${CMAKE_SOURCE_DIR}/idl/gen_idl/raft_store_idl/*.pb.cc
    ${CMAKE_SOURCE_DIR}/idl/gen_idl/rpc_service_idl/*.pb.cc
)
file(GLOB SRC_FILES
    ${CMAKE_SOURCE_DIR}/clients/*.cc
    ${CMAKE_SOURCE_DIR}/raft_store/*.cc
    # ${CMAKE_SOURCE_DIR}/service/*.cc
    ${CMAKE_SOURCE_DIR}/service/ps/*.cc
    ${CMAKE_SOURCE_DIR}/service/router/*.cc
    ${CMAKE_SOURCE_DIR}/config/*.cc
    ${CMAKE_SOURCE_DIR}/utils/*.cc
    ${CMAKE_SOURCE_DIR}/engine/*.cc
    ${CMAKE_SOURCE_DIR}/common/*.cc
)

# # 查找 protobuf
find_package(Protobuf REQUIRED)


# 添加可执行文件
add_executable(ps_main service/ps_main.cc ${IDL_SRCS} ${SRC_FILES})
add_executable(router_main service/router_main.cc ${IDL_SRCS} ${SRC_FILES})


# 链接库
target_link_libraries(
    ps_main
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    braft
    brpc
    protobuf::libprotoc
    protobuf::libprotobuf
    dl
    leveldb
    ssl
    crypto
    glog
    gflags
    pthread
)
target_link_libraries(
    router_main
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    braft
    brpc
    protobuf::libprotoc
    protobuf::libprotobuf
    dl
    leveldb
    ssl
    crypto
    glog
    gflags
    pthread
)


# # 添加可执行文件
# add_executable(service_register example/etcdctl.cc ${IDL_SRCS} ${SRC_FILES})

# # 链接库
# target_link_libraries(
#     service_register
#     pthread
#     brpc
#     protobuf::libprotoc
#     protobuf::libprotobuf
#     dl
#     gflags
#     leveldb
#     ssl
#     crypto
# )

