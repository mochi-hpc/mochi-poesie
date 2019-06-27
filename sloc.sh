#!/bin/bash
CLIENT=(include/poesie-client.h include/poesie-common.h src/poesie-client.c)
SERVER=(include/poesie-server.h src/poesie-rpc-types.h src/poesie-server.c src/poesie-vm* src/lang/*)
echo "************** CLIENT ***************"
sloccount "${CLIENT[@]}"

echo "************** SERVER ***************"
sloccount "${SERVER[@]}"
