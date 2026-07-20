#!/usr/bin/env bash
# Build the Task-25 native discovery harness from the REAL plugin sources.
#
# Rebuilds only when an input is newer than the binary, so repeated unit-test
# runs pay the compile cost once.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRIVATE="${HERE}/../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private"
OUT="${1:-${HERE}/build/native-discovery-harness}"
mkdir -p "$(dirname "${OUT}")"

SOURCES=(
  "${HERE}/harness-main.cpp"
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayCanonicalJson.cpp"
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayCapabilityStore.cpp"
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayGuidance.cpp"
  "${PRIVATE}/MCP/Gateway/McpNativeGatewaySearch.cpp"
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayDescribe.cpp"
)
while IFS= read -r shard; do SOURCES+=("${shard}"); done \
  < <(find "${PRIVATE}/MCP/Generated" -name 'McpGeneratedCapabilityShards_*.cpp' | sort)

if [[ -x "${OUT}" ]]; then
  stale=0
  while IFS= read -r input; do
    [[ "${input}" -nt "${OUT}" ]] && { stale=1; break; }
  done < <(printf '%s\n' "${SOURCES[@]}"; find "${HERE}/ue-shim" "${PRIVATE}/MCP/Gateway" "${PRIVATE}/MCP/Generated" -name '*.h')
  if [[ "${stale}" -eq 0 ]]; then
    echo "up to date ${OUT}"
    exit 0
  fi
fi

g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter \
  -I "${HERE}/ue-shim" -I "${PRIVATE}" \
  -o "${OUT}" "${SOURCES[@]}"

echo "built ${OUT}"
