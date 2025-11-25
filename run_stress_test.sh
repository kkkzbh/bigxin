#!/bin/bash

# 聊天服务器压力测试快速启动脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}   聊天服务器压力测试工具${NC}"
echo -e "${BLUE}=====================================${NC}"
echo ""

# 检查编译目录
BUILD_DIR="cmake-build-release"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}未找到编译目录,开始编译...${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . --target stress_test -j$(nproc)
    cd ..
fi

# 检查压测程序
STRESS_TEST_BIN="$BUILD_DIR/src/benchmark/stress_test"
if [ ! -f "$STRESS_TEST_BIN" ]; then
    echo -e "${YELLOW}压测程序不存在,开始编译...${NC}"
    cd "$BUILD_DIR"
    cmake --build . --target stress_test -j$(nproc)
    cd ..
fi

# 检查服务器程序
SERVER_BIN="$BUILD_DIR/src/server"
if [ ! -f "$SERVER_BIN" ]; then
    echo -e "${RED}错误: 服务器程序不存在,请先编译服务器${NC}"
    exit 1
fi

# 检查服务器是否在运行
echo -e "${BLUE}检查服务器状态...${NC}"
if netstat -an 2>/dev/null | grep -q ":5555.*LISTEN"; then
    echo -e "${GREEN}✓ 服务器已在运行 (端口 5555)${NC}"
else
    echo -e "${YELLOW}⚠ 服务器未运行${NC}"
    echo -e "${YELLOW}请在另一个终端运行: ${NC}"
    echo -e "${GREEN}  cd $(pwd)/$BUILD_DIR/src && ./server 5555${NC}"
    echo ""
    read -p "按 Enter 继续压测 (确保服务器已启动)..."
fi

echo ""
echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}   开始压力测试${NC}"
echo -e "${BLUE}=====================================${NC}"
echo ""

# 提高文件描述符限制
echo -e "${YELLOW}设置系统限制...${NC}"
ulimit -n 65535 2>/dev/null || echo -e "${YELLOW}警告: 无法提高文件描述符限制${NC}"

# 运行压测
"$STRESS_TEST_BIN"

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}   压力测试完成!${NC}"
echo -e "${GREEN}=====================================${NC}"
