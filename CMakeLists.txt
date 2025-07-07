cmake_minimum_required(VERSION 3.10)
project(ServiceDiscovery)

# 使用 C++17 标准，禁用 GNU 扩展
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WITH_GLOG "Enable glog logging" OFF)

add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.)

if(WITH_GLOG)
    add_definitions(-DBRPC_WITH_GLOG=1)
endif()

# 默认构建类型为 Release（如果用户未指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Debug 构建定义
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug Mode")
    add_definitions(-DDEBUG_)
endif()

# 根据编译器设置不同的编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "/Od /Zi")
endif()

# 检查环境变量
foreach(var BRPC_HOME BRAFT_HOME)
    if(NOT DEFINED ENV{${var}})
        message(FATAL_ERROR "$ENV{${var}} is not set!")
    else()
        message(STATUS "${var} = $ENV{${var}}")
    endif()
endforeach()

# 复制脚本和配置文件
configure_file(${CMAKE_SOURCE_DIR}/scripts/clear_cach.sh ${CMAKE_BINARY_DIR}/clear_cach.sh COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/config.toml ${CMAKE_BINARY_DIR}/config.toml COPYONLY)

# 查找库路径
find_library(BRPC_LIB NAMES brpc HINTS $ENV{BRPC_HOME}/lib64 $ENV{BRPC_HOME}/lib REQUIRED)
find_library(BRAFT_LIB NAMES braft HINTS $ENV{BRAFT_HOME}/lib64 $ENV{BRAFT_HOME}/lib REQUIRED)
# find_library(GAMMA_LIB NAMES gamma HINTS /usr/home/guoyande/guoyande/program/SimSearch/build/gamma  REQUIRED)
message(STATUS "Found brpc_lib = ${BRPC_LIB}")
message(STATUS "Found braft_lib = ${BRAFT_LIB}")

# 头文件路径
include_directories(
    $ENV{BRPC_HOME}/include
    $ENV{BRAFT_HOME}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/idl/gen_idl
    ${CMAKE_SOURCE_DIR}/gamma
    ${CMAKE_SOURCE_DIR}/gamma/third_party
)

# 源文件
file(GLOB_RECURSE IDL_SRCS
    ${CMAKE_SOURCE_DIR}/idl/gen_idl/*/*.pb.cc
)

file(GLOB SRC_FILES
    ${CMAKE_SOURCE_DIR}/clients/*.cc
    ${CMAKE_SOURCE_DIR}/raft_store/*.cc
    ${CMAKE_SOURCE_DIR}/service/ps/*.cc
    ${CMAKE_SOURCE_DIR}/service/router/*.cc
    ${CMAKE_SOURCE_DIR}/config/*.cc
    ${CMAKE_SOURCE_DIR}/utils/*.cc
    ${CMAKE_SOURCE_DIR}/engine/*.cc
    ${CMAKE_SOURCE_DIR}/common/*.cc
    ${CMAKE_SOURCE_DIR}/third_party/easyloggingpp/*.cc
)


add_subdirectory(gamma)


# 查找 protobuf
find_package(Protobuf REQUIRED)

# 公共链接库
set(COMMON_LIBS
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${BRAFT_LIB}
    ${BRPC_LIB}
    protobuf::libprotobuf
    dl
    leveldb
    ssl
    crypto
    # glog
    gflags
    pthread
    gamma
)

# 添加可执行文件并链接
add_executable(ps_main service/ps_main.cc ${IDL_SRCS} ${SRC_FILES})
target_link_libraries(ps_main PRIVATE ${COMMON_LIBS})
add_executable(router_main service/router_main.cc ${IDL_SRCS} ${SRC_FILES})
target_link_libraries(router_main PRIVATE ${COMMON_LIBS})


