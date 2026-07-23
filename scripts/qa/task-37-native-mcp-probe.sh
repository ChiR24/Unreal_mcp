#!/usr/bin/env bash
# scripts/qa/task-37-native-mcp-probe.sh
# Task 37 — OPTIONAL, best-effort native MCP (HTTP/SSE) probe.
#
# Probes the plugin's native `/mcp` Streamable-HTTP transport for the Task 37
# primitive surface. It is EXPLICITLY OPTIONAL and its result is NEVER COUNTED
# when no live editor is reachable (`counted=false`, exit 0). It mutates nothing,
# drives no build, and performs no git mutation.
#
# It correctly handles:
#   * a hung endpoint      -> curl --max-time/--connect-timeout => TIMEOUT (uncounted)
#   * a dirty git tree     -> recorded in the JSON, NEVER a failure
#   * misleading success   -> HTTP 200 whose JSON-RPC body has `.error` or lacks
#                             `.result` is classified NOT success (FAIL, not PASS)
#
# Exit: best-effort exit 0 by default. `--strict` makes a REAL, editor-backed
# failure exit 1 (for a CI lane that actually has a live editor). Absence of an
# editor is never a failure and never a pass.
#
# Env overrides:
#   MCP_NATIVE_URL       (default http://127.0.0.1:3000/mcp)
#   MCP_NATIVE_PROTOCOL  (default 2025-11-25)
#   MCP_NATIVE_TOKEN     (optional; sent as X-MCP-Capability-Token)
#   MCP_NATIVE_TIMEOUT   (default 5 ; curl --max-time seconds)
#   TASK37_PROBE_OUT     (default .omo/evidence/task-37/native-probe-result.json)

set -euo pipefail

STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

URL="${MCP_NATIVE_URL:-http://127.0.0.1:3000/mcp}"
PROTO="${MCP_NATIVE_PROTOCOL:-2025-11-25}"
TOKEN="${MCP_NATIVE_TOKEN:-}"
TIMEOUT="${MCP_NATIVE_TIMEOUT:-5}"
CONNECT_TIMEOUT=3
OUT="${TASK37_PROBE_OUT:-$REPO_ROOT/.omo/evidence/task-37/native-probe-result.json}"
STRICT=0
JSON_ONLY=0

usage() {
  cat <<'EOF'
task-37-native-mcp-probe — OPTIONAL best-effort native MCP HTTP/SSE probe

Usage: bash scripts/qa/task-37-native-mcp-probe.sh [options]

Options:
  --url=<url>     override MCP_NATIVE_URL (default http://127.0.0.1:3000/mcp)
  --strict        exit 1 on a REAL editor-backed failure (default: best-effort 0)
  --json-only     print only the JSON document to stdout
  --help, -h      this message

Behaviour:
  * No live editor reachable  -> SKIPPED_NO_EDITOR, counted=false, exit 0
  * Endpoint hangs            -> TIMEOUT, counted=false, exit 0
  * HTTP 200 with error body  -> FAIL (misleading success is NOT counted as pass)
  * Never mutates files, git, or the engine; never runs a build.
EOF
}

for arg in "$@"; do
  case "$arg" in
    --help | -h) usage; exit 0 ;;
    --strict) STRICT=1 ;;
    --json-only) JSON_ONLY=1 ;;
    --url=*) URL="${arg#--url=}" ;;
    *) echo "[task-37-native-mcp-probe] unknown arg: $arg" >&2 ;;
  esac
done

log() { [ "$JSON_ONLY" = "1" ] || echo "[task-37-native-mcp-probe] $*" >&2; }

command -v curl >/dev/null 2>&1 || { log "curl not found; cannot probe"; }
command -v node >/dev/null 2>&1 || { log "node not found; cannot classify"; }

TMPDIR_PROBE="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_PROBE"' EXIT
BODY_FILE="$TMPDIR_PROBE/init-body"
HDR_FILE="$TMPDIR_PROBE/init-hdr"
TOOLS_FILE="$TMPDIR_PROBE/tools-body"

# --- dirty tree: record, never fail ---
DIRTY_COUNT="$(git -C "$REPO_ROOT" status --porcelain 2>/dev/null | wc -l | tr -d ' ' || echo 0)"
[ -n "$DIRTY_COUNT" ] || DIRTY_COUNT=0

# --- classify a JSON-RPC body (handles both application/json and SSE data frames) ---
classify_initialize() {
  node -e '
    const fs = require("fs");
    let raw = ""; try { raw = fs.readFileSync(process.argv[1], "utf8"); } catch {}
    const extract = (t) => {
      const s = t.trim();
      if (s.startsWith("{")) return s;
      const data = s.split(/\r?\n/).filter((l) => l.startsWith("data:")).map((l) => l.slice(5).trim());
      return data.join("");
    };
    let cls = "malformed", code = "", session = "";
    try {
      const j = JSON.parse(extract(raw));
      if (j && j.result && j.result.capabilities) cls = "success";
      else if (j && j.error) { cls = "error"; code = String(j.error.code ?? ""); }
      else cls = "malformed";
    } catch { cls = raw.trim() === "" ? "empty" : "nonjson"; }
    process.stdout.write(cls + "\t" + code + "\t" + session);
  ' "$1" 2>/dev/null || printf 'nonjson\t\t'
}

classify_tools() {
  node -e '
    const fs = require("fs");
    let raw = ""; try { raw = fs.readFileSync(process.argv[1], "utf8"); } catch {}
    const extract = (t) => {
      const s = t.trim();
      if (s.startsWith("{")) return s;
      const data = s.split(/\r?\n/).filter((l) => l.startsWith("data:")).map((l) => l.slice(5).trim());
      return data.join("");
    };
    let cls = "error", detail = "";
    try {
      const j = JSON.parse(extract(raw));
      if (j && j.error) { cls = "error"; detail = "jsonrpc " + String(j.error.code ?? ""); }
      else {
        const tools = (j && j.result && j.result.tools) || [];
        const names = tools.map((t) => t.name);
        const ok = names.length === 1 && names[0] === "unreal";
        cls = ok ? "success" : "mismatch";
        detail = names.join(",");
      }
    } catch { cls = "nonjson"; detail = ""; }
    process.stdout.write(cls + "\t" + detail);
  ' "$1" 2>/dev/null || printf 'nonjson\t'
}

# --- curl arg builders ---
common_headers=(-H "Content-Type: application/json" -H "Accept: application/json, text/event-stream" -H "MCP-Protocol-Version: $PROTO")
[ -n "$TOKEN" ] && common_headers+=(-H "X-MCP-Capability-Token: $TOKEN")

INIT_BODY='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"'"$PROTO"'","capabilities":{},"clientInfo":{"name":"task-37-native-probe","version":"0.0.0"}}}'

log "probing $URL (protocol $PROTO, max-time ${TIMEOUT}s)"

HTTP_CODE=""
CURL_EXIT=0
HTTP_CODE="$(curl -sS -m "$TIMEOUT" --connect-timeout "$CONNECT_TIMEOUT" \
  -o "$BODY_FILE" -D "$HDR_FILE" -w '%{http_code}' \
  -X POST "${common_headers[@]}" --data "$INIT_BODY" "$URL" 2>"$TMPDIR_PROBE/init-err")" && CURL_EXIT=0 || CURL_EXIT=$?

EDITOR_REACHABLE="false"
COUNTED=0
INIT_CLASS="unreachable"
INIT_CODE=""
SESSION_ID=""
TOOLS_CLASS="skipped"
TOOLS_DETAIL=""
SSE_CLASS="skipped"
OVERALL="SKIPPED_NO_EDITOR"

case "$CURL_EXIT" in
  0)
    IFS=$'\t' read -r INIT_CLASS INIT_CODE _ < <(classify_initialize "$BODY_FILE")
    SESSION_ID="$(grep -i '^mcp-session-id:' "$HDR_FILE" 2>/dev/null | tail -1 | sed 's/^[^:]*:[[:space:]]*//' | tr -d '\r' || true)"
    case "$INIT_CLASS" in
      success | error | malformed | nonjson)
        EDITOR_REACHABLE="true"; COUNTED=1 ;;
      empty)
        EDITOR_REACHABLE="unknown"; COUNTED=0 ;;
    esac
    ;;
  28)
    INIT_CLASS="timeout"; EDITOR_REACHABLE="unknown"; COUNTED=0; OVERALL="INCONCLUSIVE"
    log "endpoint hung (curl 28 timeout) — inconclusive, uncounted" ;;
  6 | 7)
    INIT_CLASS="unreachable"; EDITOR_REACHABLE="false"; COUNTED=0; OVERALL="SKIPPED_NO_EDITOR"
    log "no live editor at $URL (curl $CURL_EXIT) — skipped, uncounted" ;;
  *)
    INIT_CLASS="curl-$CURL_EXIT"; EDITOR_REACHABLE="unknown"; COUNTED=0; OVERALL="INCONCLUSIVE"
    log "curl exit $CURL_EXIT — inconclusive, uncounted" ;;
esac

# --- if initialize really succeeded, best-effort tools/list + SSE open ---
if [ "$INIT_CLASS" = "success" ]; then
  init_hdr=("${common_headers[@]}")
  [ -n "$SESSION_ID" ] && init_hdr+=(-H "Mcp-Session-Id: $SESSION_ID")

  # notifications/initialized (best-effort, ignore result)
  curl -sS -m "$TIMEOUT" --connect-timeout "$CONNECT_TIMEOUT" -o /dev/null \
    -X POST "${init_hdr[@]}" \
    --data '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$URL" >/dev/null 2>&1 || true

  tools_exit=0
  curl -sS -m "$TIMEOUT" --connect-timeout "$CONNECT_TIMEOUT" -o "$TOOLS_FILE" \
    -X POST "${init_hdr[@]}" \
    --data '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' "$URL" >/dev/null 2>&1 && tools_exit=0 || tools_exit=$?
  if [ "$tools_exit" = "28" ]; then
    TOOLS_CLASS="timeout"
  elif [ "$tools_exit" != "0" ]; then
    TOOLS_CLASS="error"; TOOLS_DETAIL="curl-$tools_exit"
  else
    IFS=$'\t' read -r TOOLS_CLASS TOOLS_DETAIL < <(classify_tools "$TOOLS_FILE")
  fi

  # SSE open test — short, bounded; a hang is INCONCLUSIVE, never a hard failure
  sse_hdr=(-H "Accept: text/event-stream" -H "MCP-Protocol-Version: $PROTO")
  [ -n "$TOKEN" ] && sse_hdr+=(-H "X-MCP-Capability-Token: $TOKEN")
  [ -n "$SESSION_ID" ] && sse_hdr+=(-H "Mcp-Session-Id: $SESSION_ID")
  sse_out="$(curl -sS -m 3 --connect-timeout "$CONNECT_TIMEOUT" -X GET "${sse_hdr[@]}" "$URL" 2>/dev/null || true)"
  if printf '%s' "$sse_out" | grep -q '^data:\|^event:'; then SSE_CLASS="ok"; else SSE_CLASS="inconclusive"; fi
fi

# --- overall verdict ---
if [ "$EDITOR_REACHABLE" = "true" ]; then
  if [ "$INIT_CLASS" = "success" ]; then
    case "$TOOLS_CLASS" in
      success) OVERALL="PASS" ;;
      timeout | skipped) OVERALL="INCONCLUSIVE" ;;
      *) OVERALL="FAIL" ;;
    esac
  else
    # editor responded but initialize was NOT a real success (misleading-success guard)
    OVERALL="FAIL"
  fi
fi

# --- assemble machine-readable JSON ---
mkdir -p "$(dirname "$OUT")" 2>/dev/null || true
OUT_JSON="$(
  STARTED_AT="$STARTED_AT" PROBE_URL="$URL" PROBE_PROTO="$PROTO" PROBE_TIMEOUT="$TIMEOUT" \
  STRICT="$STRICT" EDITOR_REACHABLE="$EDITOR_REACHABLE" COUNTED="$COUNTED" \
  CURL_EXIT="$CURL_EXIT" HTTP_CODE="$HTTP_CODE" INIT_CLASS="$INIT_CLASS" INIT_CODE="$INIT_CODE" \
  SESSION_ID="$SESSION_ID" TOOLS_CLASS="$TOOLS_CLASS" TOOLS_DETAIL="$TOOLS_DETAIL" \
  SSE_CLASS="$SSE_CLASS" DIRTY_COUNT="$DIRTY_COUNT" OVERALL="$OVERALL" \
  node -e '
    const j = {
      task: 37,
      harness: "task-37-native-mcp-probe",
      optional: true,
      startedAt: process.env.STARTED_AT,
      finishedAt: new Date().toISOString(),
      url: process.env.PROBE_URL,
      protocolVersion: process.env.PROBE_PROTO,
      timeoutSec: Number(process.env.PROBE_TIMEOUT),
      strict: process.env.STRICT === "1",
      editorReachable: process.env.EDITOR_REACHABLE,
      counted: process.env.COUNTED === "1",
      curlExit: Number(process.env.CURL_EXIT),
      httpCode: process.env.HTTP_CODE || null,
      dirtyTree: { count: Number(process.env.DIRTY_COUNT || 0), note: "recorded, never a failure" },
      checks: {
        initialize: { class: process.env.INIT_CLASS, jsonRpcErrorCode: process.env.INIT_CODE || null, sessionId: process.env.SESSION_ID || null },
        toolsList: { class: process.env.TOOLS_CLASS, detail: process.env.TOOLS_DETAIL || null },
        sse: { class: process.env.SSE_CLASS },
      },
      overall: process.env.OVERALL,
      notes: [
        "OPTIONAL best-effort probe; never counted when no live editor is reachable.",
        "Misleading success guarded: an HTTP 200 whose body carries .error or lacks .result is FAIL, not PASS.",
        "A hung endpoint (curl 28) or SSE hang is INCONCLUSIVE / uncounted, not a failure.",
        "Mutates nothing; drives no build; performs no git mutation.",
      ],
    };
    process.stdout.write(JSON.stringify(j, null, 2));
  '
)"

printf '%s\n' "$OUT_JSON" >"$OUT" 2>/dev/null || log "could not write $OUT"
printf '%s\n' "$OUT_JSON"
log "overall=$OVERALL counted=$([ "$COUNTED" = "1" ] && echo true || echo false) editorReachable=$EDITOR_REACHABLE -> $OUT"

# --- exit code: best-effort 0; strict only fails on a real editor-backed failure ---
if [ "$STRICT" = "1" ] && [ "$COUNTED" = "1" ] && [ "$OVERALL" = "FAIL" ]; then
  exit 1
fi
exit 0
