#!/usr/bin/env node
// Task 59 — what does a LIVE UE 5.7.4 editor actually expose, and does it match
// what the records promise?
//
// Task 52's offline profile matrix can compute what SHOULD be reachable on a
// 5.7.4 profile from the contract alone. It cannot answer the question this task
// exists to ask, because the two ways the answer can be wrong are invisible to
// a matrix computed from the same records it is checking:
//
//   a capability the records declare and the live engine WITHHOLDS is a support
//   claim the product cannot honour on its own development target;
//
//   a capability the live engine OFFERS that the records gate away is WORSE. It
//   is the claim-without-implementation shape in reverse — the surface answers
//   for something the contract says is unavailable here, so a client that trusts
//   the contract is wrong about what it just called.
//
// WHERE THE GATE ACTUALLY LIVES, and why this probe is shaped around that.
// `src/server/gateway/gateway-availability.ts` decides `status` from deprecation
// and the dynamic tool manager ONLY, and reports the declared environment
// requirements (engine range, plugins, editor states) as DATA. That is a
// deliberate refusal, stated in its header: the discovery surface has no live
// editor to probe and guessing would produce a confident wrong answer. So a
// describe-only census measures CONTRACT FIDELITY and nothing else, and reading
// it as an availability verdict would report a perfect match on every engine —
// exactly the reading that proves nothing.
//
// The environment gate is therefore probed where it is really enforced: at
// execute, by the plugin, on the game thread. Every probe below targets
// something that DOES NOT EXIST (an actor named for this run) so the call
// reaches the handler and is refused on its merits without mutating the project.
// A refusal that names a missing target proves the handler is compiled in and
// reachable on this engine; a refusal that names an unknown action or an absent
// plugin proves it is gated out. Those are different answers and the whole point
// is to tell them apart.
//
// It NEVER launches an editor. It is pointed at a port another run owns and it
// opens exactly one session, which it deletes. If the port does not answer it
// says so and stops — inventing a census from the registry when the engine did
// not reply would produce a perfect match every time.
//
// Run: node scripts/qa/task59-capability-probe.mjs --native-port N [--out FILE]
//                                                  [--project-dir DIR]

import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';

import { NativeDriver } from '../../tests/unit/task-49/live-driver-native.mjs';
import { atLeast, censusTool, classify } from '../../tests/unit/task-59/capability-verdicts.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const PORT = Number(argOf('--native-port', '0'));
const OUT = argOf('--out', '.omo/evidence/task-59/capability-probe.json');
const PROJECT_DIR = argOf('--project-dir', null);
const log = (line) => { process.stderr.write(`${line}\n`); };

/** The engine this probe is written against, proven by the caller, asserted here. */
const ENGINE = Object.freeze({ major: 5, minor: 7, patch: 4 });

/** A name nothing in a freshly generated project can own. */
const ABSENT = `Task59Absent_${Date.now().toString(36)}`;

/**
 * Find one file by name under `root`, bounded so a probe can never walk a tree.
 * `mustContain` keeps the answer inside the bridge module's own build output —
 * an engine tree holds many files called `Definitions.h` and the wrong one would
 * report a macro set that belongs to somebody else's module.
 */
function findUnder(root, name, mustContain, budget = 20_000) {
  const queue = [root];
  let visited = 0;
  while (queue.length > 0 && visited < budget) {
    const dir = /** @type {string} */ (queue.shift());
    visited += 1;
    /** @type {import('node:fs').Dirent[]} */
    let entries = [];
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) queue.push(path);
      else if (entry.name === name && path.includes(mustContain)) return { path, visited };
    }
  }
  return { path: null, visited };
}

/**
 * The compiled feature macros THIS build actually chose.
 *
 * Every MCP_HAS_* is decided by Build.cs probing the ENGINE tree at compile time
 * — not the project's enabled-plugin list — so re-deriving them here would just
 * be the offline matrix wearing a runtime costume: it would agree with itself on
 * every engine, and it would be wrong in the same direction. UBT writes the
 * resolved set into the module's own `Definitions.h`, force-included into every
 * translation unit, which makes that header the only artifact that says what the
 * .so loaded in this editor was actually compiled with.
 *
 * When it cannot be found the field is null and names the reason. An inferred
 * macro set would be worse than no macro set.
 */
function readCompiledMacros(projectDir) {
  if (projectDir === null || !existsSync(projectDir)) {
    return {
      source: projectDir, read: false, macros: null,
      reason: projectDir === null ? 'NO_PROJECT_DIR_SUPPLIED' : 'PROJECT_DIR_ABSENT',
    };
  }
  const found = findUnder(projectDir, 'Definitions.h', '/McpAutomationBridge/');
  if (found.path === null) {
    return {
      source: projectDir, read: false, macros: null, dirsVisited: found.visited,
      reason: 'DEFINITIONS_HEADER_NOT_FOUND',
    };
  }
  const text = readFileSync(found.path, 'utf8');
  /** @type {Record<string, number>} */
  const macros = {};
  for (const match of text.matchAll(/^#define\s+(MCP_HAS_[A-Z0-9_]+)\s+([01])\s*$/gmu)) {
    macros[match[1]] = Number(match[2]);
  }
  return {
    source: found.path,
    read: true,
    reason: Object.keys(macros).length > 0 ? 'READ_FROM_UBT_DEFINITIONS_HEADER' : 'DEFINITIONS_HEADER_NAMED_NO_MCP_MACROS',
    macros: Object.keys(macros).length > 0 ? macros : null,
  };
}

async function main() {
  if (!Number.isInteger(PORT) || PORT <= 0) {
    log('REFUSING: --native-port must name the port of an editor this run may read.');
    process.exitCode = 2;
    return;
  }

  const registry = JSON.parse(readFileSync(
    `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
  ));
  /** @type {any[]} */
  const records = registry.records;
  const byId = new Map(records.map((record) => [String(record.id), record]));

  // The contract's own view, grouped the way the gateway addresses them.
  /** @type {Map<string, Set<string>>} */
  const declaredByTool = new Map();
  for (const record of records) {
    const tool = record.parent?.parent ?? null;
    if (tool === null) continue;
    if (!declaredByTool.has(tool)) declaredByTool.set(tool, new Set());
    /** @type {Set<string>} */ (declaredByTool.get(tool)).add(String(record.id).split('.').slice(1).join('.'));
  }

  const driver = /** @type {any} */ (new NativeDriver({ port: PORT, clientName: 'task59-capability-probe' }));
  const opened = await driver.initialize();
  if (opened.ok !== true) {
    log(`the native /mcp surface on 127.0.0.1:${PORT} did not initialize (status ${String(opened.status)}).`);
    writeOut({
      probed: false,
      reason: 'NATIVE_MCP_DID_NOT_INITIALIZE',
      detail: `127.0.0.1:${PORT} answered status ${String(opened.status)}; no census was taken and none is inferred`,
      port: PORT,
    });
    process.exitCode = 1;
    return;
  }
  log(`initialized on 127.0.0.1:${PORT}, protocol ${String(opened.negotiatedVersion)}`);

  /** @type {Record<string, unknown>} */
  const census = {
    probed: true,
    port: PORT,
    engine: ENGINE,
    negotiatedVersion: opened.negotiatedVersion ?? null,
    absentTargetName: ABSENT,
    startedAt: new Date().toISOString(),
  };

  // ── 1. The public surface is exactly one gateway tool ──────────────────────
  const listed = await driver.request('tools/list', {});
  const tools = (listed.response?.result?.tools ?? []).map((entry) => entry?.name);
  census.publicTools = tools;
  census.exactlyOneGatewayTool = tools.length === 1 && tools[0] === 'unreal';
  log(`tools/list -> ${JSON.stringify(tools)}`);

  // ── 2. Declared vs runtime action census, per canonical tool ───────────────
  /** @type {Array<Record<string, unknown>>} */
  const perTool = [];
  for (const tool of [...declaredByTool.keys()].sort()) {
    const census = await censusTool(driver, tool);
    const runtime = census.names;
    const declared = [...(/** @type {Set<string>} */ (declaredByTool.get(tool)))].sort();
    // Records address some actions with a family prefix (`cinematic.add_fade_track`)
    // while the surface names the bare action. Comparing both spellings keeps a
    // documented addressing convention from reading as a missing capability.
    const bare = (name) => name.slice(name.lastIndexOf('.') + 1);
    const runtimeSet = new Set([...runtime, ...runtime.map(bare)]);
    const declaredSet = new Set([...declared, ...declared.map(bare)]);
    const missingAtRuntime = declared.filter((action) => !runtimeSet.has(action) && !runtimeSet.has(bare(action)));
    const extraAtRuntime = runtime.filter((action) => !declaredSet.has(action) && !declaredSet.has(bare(action)));
    perTool.push({
      tool,
      declaredCount: declared.length,
      runtimeCount: runtime.length,
      reportedCount: census.reportedCount ?? null,
      censusMethod: census.method,
      pages: census.pages,
      isError: census.isError === true,
      missingAtRuntime,
      extraAtRuntime,
    });
    log(`describe ${tool}: declared ${declared.length}, runtime ${runtime.length} (${census.pages.length} page(s))`
      + `${missingAtRuntime.length > 0 ? `, MISSING ${missingAtRuntime.length}` : ''}`
      + `${extraAtRuntime.length > 0 ? `, EXTRA ${extraAtRuntime.length}` : ''}`);
  }
  census.perTool = perTool;
  census.totals = {
    declared: records.length,
    runtimeNamed: perTool.reduce((sum, row) => sum + Number(row.runtimeCount), 0),
    toolsWithMissing: perTool.filter((row) => /** @type {any[]} */ (row.missingAtRuntime).length > 0).length,
    toolsWithExtra: perTool.filter((row) => /** @type {any[]} */ (row.extraAtRuntime).length > 0).length,
  };

  // ── 3. The engine-range gate, computed from the records for THIS engine ────
  // Exactly one capability in the catalogue declares a minimum above 5.0, and it
  // declares 5.7.0. That makes 5.7.4 the first certified engine on which it must
  // be present — and 5.0.3/5.3.2/5.5.4 the engines on which its absence is
  // correct. A run that cannot tell those apart cannot certify either.
  const engineGated = records.filter((record) => {
    const min = record.availability?.unreal?.min;
    return min !== undefined && (min.minor > 0 || min.major > 5);
  });
  const aboveMax = records.filter((record) => {
    const max = record.availability?.unreal?.max;
    return max !== undefined && !atLeast(max, ENGINE);
  });
  census.engineRangeModel = {
    totalRecords: records.length,
    declaringMinAboveBaseline: engineGated.map((record) => ({
      id: record.id, min: record.availability.unreal.min, plugins: record.availability.requiredPlugins,
    })),
    withheldByMaxOnThisEngine: aboveMax.map((record) => record.id),
    expectation: 'on 5.7.4 every declared minimum is satisfied and no declared maximum is exceeded, '
      + 'so the engine-range gate withholds NOTHING here; anything withheld is withheld for a plugin, '
      + 'editor-state or compile reason instead, and the probes below say which',
  };

  // ── 4. The gate as the live engine enforces it ─────────────────────────────
  // Each row targets something that does not exist, so the call is refused on its
  // merits and the project is unchanged either way.
  // Each row states what would be a DEFECT rather than one blessed verdict,
  // because several answers are equally correct here and pinning an expectation
  // to one of them would manufacture a failure out of a legitimate difference.
  // `manage_geometry` needs the GeometryScripting plugin, which a generated
  // project does not enable; being refused for THAT is fine and proves nothing
  // about 5.7. Being refused for the ENGINE is the thing 5.7.4 must never do.
  const probeTable = [
    {
      id: 'manage_geometry.convert_to_nanite',
      params: { targetActor: ABSENT, actorName: ABSENT },
      allow: ['REACHED', 'GATED_PLUGIN'],
      forbid: ['GATED_ENGINE', 'SUCCEEDED'],
      why: 'the ONLY capability in the catalogue declaring UE >= 5.7.0, so 5.7.4 is the first certified '
        + 'engine that must not withhold it for an engine reason. A plugin refusal is acceptable and '
        + 'expected (GeometryScripting is not enabled in a generated project); an ENGINE refusal here '
        + 'would contradict the record that gates it, and a success against an actor that does not '
        + 'exist would mean the handler never checked',
    },
    {
      id: 'manage_geometry.get_mesh_info',
      params: { targetActor: ABSENT, actorName: ABSENT },
      allow: ['REACHED', 'GATED_PLUGIN'],
      forbid: ['GATED_ENGINE', 'SUCCEEDED'],
      why: 'the same plugin family at the 5.0.0 baseline. It is the control for the row above: the two '
        + 'differ only in their declared minimum, so if both answer the same way the 5.7 gate is not '
        + 'what decided either, and if they differ the difference is the gate',
    },
    {
      id: 'manage_networking.check_has_authority',
      params: { actorName: ABSENT },
      allow: ['GATED_STATE', 'REACHED'],
      forbid: ['SUCCEEDED'],
      why: 'declared pie/simulate only, and this editor is in edit. A state refusal is the contract-honest '
        + 'answer; a refusal for the missing actor is acceptable because the actor genuinely is missing. '
        + 'A SUCCESS would be the worse defect — the surface answering for what the records withhold',
    },
    {
      id: 'manage_networking.check_is_locally_controlled',
      params: { actorName: ABSENT },
      allow: ['GATED_STATE', 'REACHED'],
      forbid: ['SUCCEEDED'],
      why: 'the second read-effect member of the 12 pie/simulate-only records, so the editor-state verdict '
        + 'rests on two readings rather than one',
    },
  ];

  /** @type {Array<Record<string, unknown>>} */
  const gateProbes = [];
  for (const row of probeTable) {
    const record = byId.get(row.id);
    if (record === undefined) {
      // A renamed capability must break this probe loudly rather than quietly
      // reduce its coverage — a table that silently probes nothing still reports.
      gateProbes.push({
        ...row, ran: false, verdict: 'RECORD_ABSENT', outcome: 'NOT_RUN',
        detail: `${row.id} is not in the registry, so this gate was never probed and no result may be inferred for it`,
      });
      continue;
    }
    const tool = record.parent.parent;
    const action = String(record.id).split('.').slice(1).join('.');
    // `action` is supplied at the GATEWAY level and must not be repeated inside
    // params: the gateway refuses that outright with INVALID_PARAMS, which never
    // reaches the editor and would score every row as unreadable.
    const call = await driver.callTool({
      operation: 'execute', tool, action, params: { ...row.params },
    }, { timeoutMs: 120_000 });
    const seen = classify(call.response);
    const violated = row.forbid.includes(seen.verdict);
    gateProbes.push({
      id: row.id,
      tool,
      action,
      allow: row.allow,
      forbid: row.forbid,
      why: row.why,
      ran: true,
      declaredMin: record.availability.unreal.min,
      declaredPlugins: record.availability.requiredPlugins,
      declaredStates: record.availability.editorStates,
      verdict: seen.verdict,
      errorCode: seen.errorCode,
      reachedEditor: seen.reachedEditor === true,
      // Three outcomes, not two. UNCLEAR is neither agreement nor a defect: it
      // means this probe could not read the answer, and saying so is the only
      // honest report. Scoring it either way would invent a result.
      outcome: violated ? 'DEFECT' : row.allow.includes(seen.verdict) ? 'AGREES' : 'UNCLEAR',
      evidence: seen.evidence,
      ms: call.ms ?? null,
    });
    log(`execute ${row.id}: allow ${row.allow.join('|')}, observed ${seen.verdict}`);
  }
  census.gateProbes = gateProbes;

  // A capability that ANSWERS where the records withhold it, and one WITHHELD for
  // an engine reason where the records say this engine qualifies, are the two
  // defects this section exists to catch. They are counted separately from
  // ordinary unreadability rather than folded into one number.
  census.gateSummary = {
    ran: gateProbes.filter((row) => row.ran === true).length,
    agreed: gateProbes.filter((row) => row.outcome === 'AGREES').length,
    defects: gateProbes.filter((row) => row.outcome === 'DEFECT').length,
    unclear: gateProbes.filter((row) => row.outcome === 'UNCLEAR').length,
    answeredWhereWithheld: gateProbes.filter((row) => row.verdict === 'SUCCEEDED' && row.forbid.includes('SUCCEEDED')).length,
    withheldByEngineWhereQualified: gateProbes.filter((row) => row.verdict === 'GATED_ENGINE' && row.forbid.includes('GATED_ENGINE')).length,
  };

  // ── 5. The compiled feature set, from this build's own log ─────────────────
  census.compiledMacros = readCompiledMacros(PROJECT_DIR);

  const closed = await driver.close();
  const released = await driver.verifySessionReleased();
  census.session = { closed: closed.deleted === true, released: released.released === true, observed: released.observed };
  census.finishedAt = new Date().toISOString();
  writeOut(census);
  if (census.gateSummary.defects > 0 || census.totals.toolsWithExtra > 0) process.exitCode = 1;
}

function writeOut(document) {
  mkdirSync(dirname(`${REPO}/${OUT}`), { recursive: true });
  writeFileSync(`${REPO}/${OUT}`, `${JSON.stringify(document, null, 2)}\n`);
  log(`wrote ${OUT}`);
}

main().catch((error) => {
  log(String(error?.stack ?? error));
  process.exitCode = 1;
});
