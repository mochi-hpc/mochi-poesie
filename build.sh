#!/bin/bash
SCRIPT_DIR=$(dirname "$0")
cmake $SCRIPT_DIR -DENABLE_TESTS=ON -DENABLE_EXAMPLES=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DENABLE_PYTHON=ON -DENABLE_BEDROCK=ON -DENABLE_LUA=ON -DENABLE_RUBY=ON