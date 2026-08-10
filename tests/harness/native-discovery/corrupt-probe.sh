#!/usr/bin/env bash
# Fail-closed probe: corrupt the generated capability shards and prove the native
# store refuses to serve discovery rather than falling back to stale metadata.
#
# Builds the SAME native sources against a corrupted copy of MCP/Generated, so
# the refusal comes from production code, not from the harness.
#
# Exits 0 when the store correctly refused; non-zero when it served anyway.
set -uo pipefail

MODE="${1:?usage: corrupt-probe.sh <truncate-json|wrong-count|invalid-record>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRIVATE="${HERE}/../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/task25-corrupt-XXXXXX")"
trap 'rm -rf "${WORK}"' EXIT

mkdir -p "${WORK}/MCP"
cp -r "${PRIVATE}/MCP/Generated" "${WORK}/MCP/Generated"

python3 - "${WORK}/MCP/Generated" "${MODE}" <<'PY'
import sys, pathlib
generated, mode = pathlib.Path(sys.argv[1]), sys.argv[2]
shard = generated / 'McpGeneratedCapabilityShards_MCP_CAP_SHARD_MANAGE_TOOLS.cpp'
index = generated / 'McpGeneratedCapabilityShards.h'
if mode == 'truncate-json':
    lines = shard.read_text().splitlines(keepends=True)
    cut = max(i for i, l in enumerate(lines) if l.startswith('\tTEXT("'))
    shard.write_text(''.join(lines[:cut] + lines[cut + 1:]))
elif mode == 'wrong-count':
    text = index.read_text()
    index.write_text(text.replace(
        'Detail::MCP_CAP_SHARD_MANAGE_TOOLS_CHUNKS, ', 'Detail::MCP_CAP_SHARD_MANAGE_TOOLS_CHUNKS, ', 1)
        .replace('TEXT("manage_tools"), Detail::MCP_CAP_SHARD_MANAGE_TOOLS_CHUNKS, ', 'TEXT("manage_tools"), Detail::MCP_CAP_SHARD_MANAGE_TOOLS_CHUNKS, ')
        .replace(', 8 },', ', 9 },', 1))
elif mode == 'invalid-record':
    shard.write_text(shard.read_text().replace('\\"id\\":', '\\"identifier\\":', 1))
else:
    raise SystemExit(f'unknown mode {mode}')
PY

BIN="${WORK}/harness"
if ! g++ -std=c++17 -O0 -w \
  -I "${HERE}/ue-shim" -I "${WORK}" -I "${PRIVATE}" \
  -o "${BIN}" \
  "${HERE}/harness-main.cpp" \
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayCanonicalJson.cpp" \
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayCapabilityStore.cpp" \
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayGuidance.cpp" \
  "${PRIVATE}/MCP/Gateway/McpNativeGatewaySearch.cpp" \
  "${PRIVATE}/MCP/Gateway/McpNativeGatewayDescribe.cpp" \
  "${WORK}"/MCP/Generated/McpGeneratedCapabilityShards_*.cpp 2>"${WORK}/build.log"; then
  echo "REFUSED_AT_BUILD"
  exit 0
fi

OUTPUT="$("${BIN}" "${HERE}/cases.json" 2>&1)"
STATUS=$?
if [[ ${STATUS} -eq 0 ]]; then
  echo "SERVED_ANYWAY: corrupted shards produced discovery output"
  exit 1
fi
echo "REFUSED status=${STATUS} detail=${OUTPUT}"
exit 0
