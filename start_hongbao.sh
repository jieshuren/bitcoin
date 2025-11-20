#!/bin/bash
# 红包币独立网络启动脚本（最终版）

echo "启动红包币独立网络..."
echo "网络模式：regtest (本地测试网络，完全独立)"
echo "特点：5秒出块时间，固定1 HBC奖励"
echo ""

# 终止已有的进程
pkill -f "bitcoin" 2>/dev/null || true

# 等待结束
sleep 2

# 清理锁文件
rm -f /tmp/hongbao_data/regtest/.lock 2>/dev/null || true

# 创建数据目录
mkdir -p /tmp/hongbao_data

# 启动GUI (内置节点功能)
echo "启动红包币GUI客户端 (内置节点)..."
cd /Users/comfan/Documents/GitHub/bitcoin
./build/bin/bitcoin-qt -datadir=/tmp/hongbao_data -regtest -listen=0 -dnsseed=0 -discover=0 &

echo "红包币独立网络已启动"
echo "注意：此网络完全独立，不会连接比特币主网"

