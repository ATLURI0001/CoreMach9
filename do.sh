#!/bin/bash
set -euo pipefail

BINARY_NAME="cpp-server"
CMD="${1:-}"

# If no command is given, show usage
if [ -z "$CMD" ]; then
  echo "Usage: $0 [cmake | build | run]"
  exit 1
fi

case "$CMD" in
  cmake|-cmake)
    rm -rf build
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j"$(nproc 2>/dev/null || echo 4)"
    ;;
  build|-build)
    cd build
    make -j"$(nproc 2>/dev/null || echo 4)"
    cd ..
    ;;
  run|-run)
    cd build
    make -j"$(nproc 2>/dev/null || echo 4)"
    ./"$BINARY_NAME"
    cd ..
    ;;
  *)
    echo "Usage: $0 [cmake | build | run]"
    exit 1
    ;;
esac
