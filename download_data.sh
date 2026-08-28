#!/bin/bash
echo "========================================"
echo "  飞花令 & 成语接龙 - 数据下载脚本 (Linux/macOS)"
echo "========================================"
echo

mkdir -p third_party

echo "[1/2] 下载 chinese-poetry (诗词库)..."
if [ -d "third_party/chinese-poetry/.git" ]; then
    echo "  已存在，跳过"
else
    git clone --depth 1 https://github.com/chinese-poetry/chinese-poetry.git third_party/chinese-poetry
fi

echo
echo "[2/2] 下载 chinese-xinhua (成语库)..."
if [ -d "third_party/chinese-xinhua/.git" ]; then
    echo "  已存在，跳过"
else
    git clone --depth 1 https://github.com/pwxcoo/chinese-xinhua.git third_party/chinese-xinhua
fi

echo
echo "========================================"
echo "  数据下载完成！"
echo "  构建: cmake -B build && cmake --build build -j"
echo "========================================"
