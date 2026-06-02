#!/usr/bin/env node
/**
 * Characterization test for `manage_ai.get_ai_info(blackboardPath)`.
 *
 * Pins the current Blackboard key serialization shape so that future refactors
 * of the BB serialization path in `McpAutomationBridge_AIHandlers.cpp` cannot
 * silently regress the response contract that downstream callers depend on.
 *
 * Run:
 *   npm run build:core
 *   node tests/integration/get_ai_info_characterization.mjs
 *
 * Requires UE editor running with the McpAutomationBridge plugin loaded.
 *
 * Pinned facts (observed against UE 5.7 + current dev):
 *   - Response data lives at `response.structuredContent.result.aiInfo`.
 *   - UE auto-inserts a `SelfActor` key inherited from BlackboardData base, so
 *     `keyCount` is (added keys + 1). This inheritance must be preserved.
 *   - Type strings carry the `BlackboardKeyType_*` prefix (the C++ class name),
 *     not the bare authoring enum (`Bool`/`Object`/`Int`).
 */

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..', '..');

const TEST_FOLDER = '/Game/IntegrationTest';
// Each run lives inside its own temp folder under TEST_FOLDER. Sweeping that
// folder in `finally` removes every artifact this run created, even ones an
// individual asset-level delete might miss (and keeps prior runs that crashed
// before cleanup isolated to their own tag rather than scattering siblings).
const RUN_TAG = `${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
const RUN_FOLDER = `${TEST_FOLDER}/PR0a_run_${RUN_TAG}`;
const BB_NAME = 'BB_GetAiInfoCharacterization';

const transport = new StdioClientTransport({
  command: 'node',
  args: [path.join(repoRoot, 'dist', 'cli.js')],
  cwd: repoRoot,
});

const client = new Client({ name: 'pr0a-characterization', version: '0.0.1' }, { capabilities: {} });

/**
 * Invoke an MCP tool and return its `structuredContent` payload.
 *
 * Reads from `structuredContent` (the canonical JSON envelope this MCP server
 * emits for automation responses); the sibling `content[0].text` is a flat
 * human-readable summary, not JSON, and is intentionally ignored except for
 * surfacing detail on transport-level `isError`. Throws on any of:
 *   - transport `isError: true`
 *   - missing `structuredContent`
 *   - `structuredContent.success === false`
 *
 * @param {string} toolName  Tool identifier (e.g. `manage_ai`, `manage_asset`).
 * @param {object} args      Tool arguments forwarded as `arguments`.
 * @returns {Promise<object>} The parsed `structuredContent` object.
 */
async function call(toolName, args) {
  const res = await client.callTool({ name: toolName, arguments: args });
  if (res?.isError) {
    const detail = (res?.content?.[0]?.text ?? '').slice(0, 500);
    throw new Error(`Tool ${toolName} returned isError; detail: ${detail}`);
  }
  const sc = res?.structuredContent;
  if (!sc) throw new Error(`No structuredContent from ${toolName}`);
  // Reject anything that is not an explicit success — strict `!== true` so a
  // response that omits the field (which can happen on some MCP servers
  // depending on result-class) doesn't slip through and let downstream code
  // operate on an unverified payload.
  if (sc.success !== true) {
    throw new Error(`Tool ${toolName} did not report success:true; payload: ${JSON.stringify(sc).slice(0, 500)}`);
  }
  return sc;
}

/**
 * Call `add_blackboard_key` with a short retry. The first add after
 * `create_blackboard_asset` can race the UE asset registry's visibility
 * propagation; subsequent adds run after the BB is already registered, so
 * only the first invocation needs this guard.
 *
 * @param {object} args  Arguments forwarded to `add_blackboard_key`.
 * @param {number} attempts  Total tries including the first.
 * @param {number} delayMs   Backoff between tries (linear, kept small).
 * @returns {Promise<object>} The `call()` result of the successful attempt.
 */
async function addKeyWithRetry(args, attempts = 3, delayMs = 100) {
  let lastErr;
  for (let i = 0; i < attempts; i++) {
    try {
      return await call('manage_ai', { action: 'add_blackboard_key', ...args });
    } catch (e) {
      lastErr = e;
      if (i < attempts - 1) await new Promise((r) => setTimeout(r, delayMs));
    }
  }
  throw lastErr;
}

let exitCode = 0;
// Hoisted so `finally` can target the created BB explicitly. Without this, the
// folder-delete code path enumerates assets via McpSafeDeleteFolder which, for
// some asset classes (BlackboardData observed in UE 5.7), triggers an
// interactive "Delete Assets" modal that wedges the editor main thread and
// times out the request at 30s. Deleting the asset directly first uses a
// quieter UEditorAssetLibrary::DeleteAsset path; the subsequent folder delete
// then takes the fast empty-folder branch.
let bbPath = '';
try {
  // Connect inside try so a failed connect (transport spawn / MCP handshake) still hits the finally cleanup
  await client.connect(transport);

  // Safety belt: if a prior process somehow used the same RUN_TAG (essentially
  // impossible with timestamp + random suffix), best-effort sweep its folder.
  try { await call('manage_asset', { action: 'delete', path: RUN_FOLDER, force: true }); } catch {}

  // Setup — create_folder is best-effort for both the shared parent (may already
  // exist from tests/integration.mjs) and the per-run subfolder.
  try { await call('manage_asset', { action: 'create_folder', path: TEST_FOLDER }); } catch {}
  try { await call('manage_asset', { action: 'create_folder', path: RUN_FOLDER }); } catch {}

  // Use the bbPath returned by create_blackboard_asset (the canonical UE object
  // path, e.g. "/Game/.../BB_Name.BB_Name") for every follow-up call rather than
  // a client-side string concat. Insulates the test against any future server-
  // side path normalization changes.
  const createResp = await call('manage_ai', { action: 'create_blackboard_asset', path: RUN_FOLDER, name: BB_NAME });
  bbPath = createResp.result?.blackboardPath ?? '';
  assert.ok(bbPath, 'create_blackboard_asset must return result.blackboardPath');

  // First add retries the asset-registry visibility race; later adds don't need it.
  await addKeyWithRetry({ blackboardPath: bbPath, keyName: 'Spotted', keyType: 'Bool' });
  await call('manage_ai', { action: 'add_blackboard_key', blackboardPath: bbPath, keyName: 'TargetActor',   keyType: 'Object', baseObjectClass: 'Actor' });
  await call('manage_ai', { action: 'add_blackboard_key', blackboardPath: bbPath, keyName: 'WaypointIndex', keyType: 'Int' });
  await call('manage_ai', { action: 'set_key_instance_synced', blackboardPath: bbPath, keyName: 'WaypointIndex', isInstanceSynced: true });

  // Action under test
  const resp = await call('manage_ai', { action: 'get_ai_info', blackboardPath: bbPath });

  // Envelope
  assert.equal(resp.success, true, 'structuredContent.success must be true');

  // Access path: structuredContent.result.aiInfo
  const aiInfo = resp.result?.aiInfo;
  assert.ok(aiInfo, 'response must include result.aiInfo for blackboardPath input');

  // Existing top-level fields (must remain bit-identical across PR1a refactor)
  assert.equal(aiInfo.assignedBlackboard, BB_NAME,
    'assignedBlackboard must equal the BB asset name');

  // 3 keys we added + 1 SelfActor inherited from BlackboardData base = 4
  assert.equal(aiInfo.keyCount, 4,
    'keyCount = added keys + 1 SelfActor (inherited from BlackboardData)');

  assert.ok(Array.isArray(aiInfo.blackboardKeys),
    'blackboardKeys must be an array');
  assert.equal(aiInfo.blackboardKeys.length, 4,
    'blackboardKeys array length matches keyCount');

  const byName = (n) => aiInfo.blackboardKeys.find((k) => k.name === n);

  // Per-key shape: name, type ("BlackboardKeyType_*" — the UE C++ class name),
  // instanceSynced. PR1a may add fields (baseClass, entryCategory,
  // parentBlackboard, ...); per-key extra fields are tolerated, the existing
  // three fields must remain unchanged.

  const selfActor = byName('SelfActor');
  assert.ok(selfActor, 'SelfActor key must be present (auto-inserted by UE BlackboardData base)');
  assert.equal(selfActor.name, 'SelfActor');
  assert.equal(selfActor.type, 'BlackboardKeyType_Object');
  assert.equal(selfActor.instanceSynced, false);

  const spotted = byName('Spotted');
  assert.ok(spotted, 'Spotted key must be present');
  assert.equal(spotted.name, 'Spotted');
  assert.equal(spotted.type, 'BlackboardKeyType_Bool');
  assert.equal(spotted.instanceSynced, false);

  const targetActor = byName('TargetActor');
  assert.ok(targetActor, 'TargetActor key must be present');
  assert.equal(targetActor.name, 'TargetActor');
  assert.equal(targetActor.type, 'BlackboardKeyType_Object');
  assert.equal(targetActor.instanceSynced, false);

  const waypointIndex = byName('WaypointIndex');
  assert.ok(waypointIndex, 'WaypointIndex key must be present');
  assert.equal(waypointIndex.name, 'WaypointIndex');
  assert.equal(waypointIndex.type, 'BlackboardKeyType_Int');
  assert.equal(waypointIndex.instanceSynced, true,
    'set_key_instance_synced must have flipped this to true');

  console.log('PASS: get_ai_info BB shape characterization');
} catch (err) {
  console.error('FAIL:', err?.message ?? err);
  if (err?.stack) console.error(err.stack);
  exitCode = 1;
} finally {
  // Two-step teardown, best-effort. Step 1 deletes the BB asset by its exact
  // path so the plugin takes the single-asset code path (silent
  // UEditorAssetLibrary::DeleteAsset). Step 2 then sweeps the now-empty
  // RUN_FOLDER; the empty-folder fast path skips asset enumeration entirely.
  // This avoids the interactive "Delete Assets" modal that the folder-delete
  // code path can raise for some asset classes in UE 5.7.
  if (bbPath) {
    try { await call('manage_asset', { action: 'delete', path: bbPath, force: true }); } catch {}
  }
  try { await call('manage_asset', { action: 'delete', path: RUN_FOLDER, force: true }); } catch {}
  try { await client.close(); } catch {}
}

process.exit(exitCode);
