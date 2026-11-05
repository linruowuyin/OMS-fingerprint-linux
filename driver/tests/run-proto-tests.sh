#!/bin/bash
# 编译并运行 omsmoc 帧编解码的单元测试
#
# 不需要硬件、不需要 libfprint 源码树，只要 glib 开发包。
# 在 libfprint 树里也可以用 meson test omsmoc-proto 跑同一份用例。
#
# 用法: driver/tests/run-proto-tests.sh [--verbose]
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
DRIVER_DIR=$(dirname "$HERE")
BUILD_DIR="${TMPDIR:-/tmp}/omsmoc-proto-test.$$"

mkdir -p "$BUILD_DIR"
trap 'rm -rf "$BUILD_DIR"' EXIT

CFLAGS=$(pkg-config --cflags glib-2.0)
LIBS=$(pkg-config --libs glib-2.0)

echo "==> 编译"
cc -std=gnu11 -O2 -Wall -Wextra -Werror \
   -DOMS_PROTO_STANDALONE \
   -I "$DRIVER_DIR/omsmoc" \
   $CFLAGS \
   "$HERE/test-omsmoc-proto.c" "$DRIVER_DIR/omsmoc/omsmoc_proto.c" \
   $LIBS -o "$BUILD_DIR/omsmoc-proto-test"

echo "==> 运行"
"$BUILD_DIR/omsmoc-proto-test" "$@"
