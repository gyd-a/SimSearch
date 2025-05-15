#!/bin/bash

# 1. 设置 proto 源文件根目录
ETCD_PROTO_DIR="etcd_idl"
RPC_SERVICE_PROTO_DIR="rpc_service_idl"


# 2. 设置输出目录
OUT_DIR="gen_idl"
# 3. 确保输出目录存在
rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"
# 4. 编译所有 .proto 文件为 C++ 文件
echo "Compiling .proto files to C++..."

protoc \
  -I=. \
  -I="${ETCD_PROTO_DIR}" \
  -I="${ETCD_PROTO_DIR}" \
  -I="${ETCD_PROTO_DIR}/google" \
  -I="${ETCD_PROTO_DIR}/gogoproto" \
  --cpp_out="${OUT_DIR}" \
  $(find "${ETCD_PROTO_DIR}" -name "*.proto")

# 5. 打印完成信息
if [ $? -eq 0 ]; then
  echo "✅ '${ETCD_PROTO_DIR}' Proto files successfully compiled to ${OUT_DIR}"
else
  echo "'${ETCD_PROTO_DIR}' ❌ Failed to compile proto files"
  exit 1
fi


protoc \
  -I=. \
  -I="${RPC_SERVICE_PROTO_DIR}" \
  --cpp_out="${OUT_DIR}" \
  $(find "${RPC_SERVICE_PROTO_DIR}" -name "*.proto")
# 5. 打印完成信息
if [ $? -eq 0 ]; then
  echo "✅ '${RPC_SERVICE_PROTO_DIR}' Proto files successfully compiled to ${OUT_DIR}"
else
  echo "'${RPC_SERVICE_PROTO_DIR}' ❌ Failed to compile proto files"
  exit 1
fi

