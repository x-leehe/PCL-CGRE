#!/bin/bash
# PCL-CGRE 编译脚本
set -euo pipefail

BUILD_TYPE="${1:-Release}"
JOBS="${2:-$(nproc)}"

if [ "${BUILD_TYPE,,}" = "debug" ]; then
    BUILD_DIR="build-debug"
    SRC_NAME="pcl-cgre_debug"
    DST_NAME="pcl-cgre_debug"
else
    BUILD_DIR="build"
    SRC_NAME="pcl-cgre_release"
    DST_NAME="pcl-cgre"
fi

echo "==> 配置 (${BUILD_TYPE})..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" .

echo ""
echo "==> 编译 (${JOBS} 并行)..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo ""
mkdir -p build
cp "${BUILD_DIR}/${SRC_NAME}" "build/${DST_NAME}"

if [ "${BUILD_DIR}" != "build" ]; then
    cp "${BUILD_DIR}/pcl-cgre.gresource" build/pcl-cgre.gresource
    rm -rf build/resources
    cp -r "${BUILD_DIR}/resources" build/resources
    rm -rf build-debug
fi

rm -rf build/pcl-cgre_release

echo "==> 完成: ./build/${DST_NAME}"
