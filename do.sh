#!/bin/bash
set -euo pipefail

BINARY_NAME="cpp-server"
CMD="${1:-}"

# If no command is given, show usage
if [ -z "$CMD" ]; then
  echo "Usage: $0 [cmake | build | run | test]"
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
  test|-test)
    if [ -d "build" ]; then
      cd build
      ctest --output-on-failure
      cd ..
    else
      echo "Build directory not found. Run './do.sh cmake' first."
      exit 1
    fi
    ;;
  *)
    echo "Usage: $0 [cmake | build | run | test]"
    exit 1
    ;;
esac
