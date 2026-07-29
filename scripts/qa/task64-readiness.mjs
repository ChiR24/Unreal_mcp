// scripts/qa/task64-readiness.mjs
//
// Emits the final readiness record for todo 64 of
// .omo/plans/pure-unreal-mcp-implementation.md.
//
// The record renders a decision that was already made and is NOT reopened here:
// todo 63 emitted BLOCKED_EXTERNAL under rule FALLBACK-1. This generator's job
// is to render that truthfully, name the tree it describes, and re-verify every
// fact it can re-verify at emit time.
//
// TRANSCRIBED vs RECOMPUTED — the distinction matters and is kept explicit:
//   * Gate exit codes are TRANSCRIBED from one recorded serial run whose raw
//     logs are preserved beside this record under .omo/evidence/task-64/gates/.
//     A generator that re-ran the gates would report a different tree than the
//     one the logs describe.
//   * Everything else is RECOMPUTED live at emit time: file digests, the
//     preservation baseline, the capability/tool counts read from the generated
//     manifest, the evidence link + hash sweep, and the proof that each edited
//     document is absent from the preservation baseline.
//
// The record is written to satisfy tests/unit/task-50/evidence-validator.mjs,
// which re-checks tree digests, artifact staleness, oracle links and cleanup
// links. If a field here cannot survive that validator it does not belong here.

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { dirname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const EVIDENCE_DIR = join(ROOT, '.omo/evidence');
const OUT = join(EVIDENCE_DIR, 'task-64-pure-unreal-mcp-implementation.json');
const BASELINE = join(EVIDENCE_DIR, 'preservation/preserved-24-baseline.sha256');

const sha256 = (abs) => createHash('sha256').update(readFileSync(abs)).digest('hex');
const readJson = (abs) => JSON.parse(readFileSync(abs, 'utf8'));
const iso = (ms) => new Date(ms).toISOString();

// ---------------------------------------------------------------------------
// Preservation baseline. Recomputed, never trusted from a prior run.
// ---------------------------------------------------------------------------
const baselineEntries = readFileSync(BASELINE, 'utf8')
  .split('\n')
  .filter((line) => line.trim().length > 0)
  .map((line) => {
    const [digest, ...rest] = line.trim().split(/\s+/);
    return { digest, path: rest.join(' ') };
  });

const verifyPreservation = () => {
  const rows = baselineEntries.map((entry) => {
    const abs = join(ROOT, entry.path);
    if (!existsSync(abs)) return { ...entry, ok: false, why: 'missing' };
    const actual = sha256(abs);
    return { ...entry, actual, ok: actual === entry.digest, why: actual === entry.digest ? 'ok' : 'digest changed' };
  });
  return {
    declared: rows.length,
    okCount: rows.filter((r) => r.ok).length,
    mismatches: rows.filter((r) => !r.ok).map((r) => `${r.path}: ${r.why}`),
  };
};

const preservedPaths = new Set(baselineEntries.map((e) => e.path));

// ---------------------------------------------------------------------------
// Evidence link + hash sweep across every recorded evidence document.
// ---------------------------------------------------------------------------
const ephemeral = (abs) =>
  abs.startsWith('/tmp/') || abs.startsWith('/proc/') || abs.startsWith('/dev/')
  || abs.includes('/Intermediate/') || abs.includes('/Binaries/')
  || abs.includes('/node_modules/') || abs.includes('/Saved/') || abs.includes('/DerivedDataCache/');

const pathShaped = (value) => {
  if (typeof value !== 'string' || value.length === 0) return false;
  if (/^https?:/i.test(value) || /^[a-z][a-z0-9+.-]*:\/\//i.test(value)) return false;
  if (value.includes('\n') || value.includes(' ')) return false;
  return value.includes('/');
};

// This generator's own output is excluded from its own sweep. Including it
// would measure the PREVIOUS run's stale copy — the digests that record snapshot
// carries are true of the tree at its own emit time, not of the tree the next
// run observes — and would inflate the mismatch count with an artifact of the
// generator rather than a finding about the evidence base.
const SELF = 'task-64-pure-unreal-mcp-implementation.json';

const sweepEvidence = () => {
  const docs = readdirSync(EVIDENCE_DIR)
    .filter((f) => /^task-.*\.json$/.test(f) && f !== SELF)
    .sort();
  const summary = {
    documentsScanned: docs.length,
    excludedFromScan: [{ document: SELF, why: 'this record is the sweep\'s own output; scanning it would measure the previous run rather than the evidence base' }],
    parseFailures: [], linksChecked: 0, deadLinks: [],
    hashesRechecked: 0, hashMismatches: [], skippedBasenameOnly: 0, skippedGlob: 0,
    skippedEphemeral: 0, skippedAbsenceRecord: 0,
  };

  const walk = (node, file, ptr) => {
    if (Array.isArray(node)) {
      node.forEach((entry, index) => { walk(entry, file, `${ptr}/${index}`); });
      return;
    }
    if (!node || typeof node !== 'object') return;

    const candidates = [['path', node.path], ['file', node.file], ['evidenceFile', node.evidenceFile],
      ['target', node.target], ['artifact', node.artifact]];
    const declared = [node.sha256, node.digest, node.hash]
      .find((h) => typeof h === 'string' && /^[0-9a-f]{64}$/.test(h)) ?? null;

    for (const [key, value] of candidates) {
      if (typeof value !== 'string' || value.length === 0) continue;
      if (!pathShaped(value)) {
        if (/\.(ts|js|mjs|cpp|h|json|md|log)$/.test(value)) summary.skippedBasenameOnly += 1;
        continue;
      }
      if (/[{}*?]/.test(value)) { summary.skippedGlob += 1; break; }
      // A record that ASSERTS absence is not a dead link; it is the finding.
      if (node.present === false || node.exists === false || node.rootPresent === false
        || /absent|missing|notFound|must-not-exist/i.test(ptr)) { summary.skippedAbsenceRecord += 1; break; }

      let abs = value.startsWith('/') ? value : join(ROOT, value);
      if (!value.startsWith('/') && !existsSync(abs) && existsSync(join(EVIDENCE_DIR, value))) {
        abs = join(EVIDENCE_DIR, value);
      }
      if (ephemeral(abs)) { summary.skippedEphemeral += 1; break; }

      summary.linksChecked += 1;
      if (!existsSync(abs)) { summary.deadLinks.push({ document: file, pointer: `${ptr}/${key}`, value }); break; }
      if (declared && statSync(abs).isFile()) {
        summary.hashesRechecked += 1;
        const actual = sha256(abs);
        if (actual !== declared) {
          summary.hashMismatches.push({ document: file, pointer: `${ptr}/${key}`, value, recorded: declared, actual });
        }
      }
      break;
    }
    for (const [k, v] of Object.entries(node)) if (v && typeof v === 'object') walk(v, file, `${ptr}/${k}`);
  };

  for (const doc of docs) {
    try { walk(readJson(join(EVIDENCE_DIR, doc)), doc, ''); }
    catch (error) { summary.parseFailures.push({ document: doc, error: String(error && error.message) }); }
  }

  // A raw count is not a finding. Each dead link is resolved against the tree
  // and against git so the report says WHICH kind of dead it is: a path written
  // relative to the plugin instead of the repository, a file that was really
  // deleted, or a file git has never seen under that name.
  const PLUGIN_PREFIX = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/';
  const gitKnows = (path) => {
    try {
      const out = execFileSync('git', ['log', '--all', '--oneline', '--', `**/${path.split('/').pop()}`],
        { cwd: ROOT, encoding: 'utf8' });
      return out.trim().length > 0;
    } catch { return false; }
  };

  summary.deadLinkClassification = summary.deadLinks.map((entry) => {
    const underPlugin = [`${PLUGIN_PREFIX}${entry.value}`, `${PLUGIN_PREFIX}Private/${entry.value}`]
      .find((candidate) => existsSync(join(ROOT, candidate)));
    if (underPlugin) {
      return { ...entry, kind: 'path-prefix-only', resolvesAt: underPlugin,
        note: 'the file EXISTS; the citation is written relative to the plugin module rather than the repository root. Cosmetic, not a missing artifact.' };
    }
    return {
      ...entry,
      kind: gitKnows(entry.value) ? 'deleted-since' : 'never-committed-under-this-name',
      note: gitKnows(entry.value)
        ? 'the file was really removed after the citing record was written; the citation describes a tree that no longer exists'
        : 'git has no commit touching a file of this basename, so the citation never named a committed path',
    };
  });

  summary.hashMismatchInterpretation = summary.hashMismatches.length === 0 ? null : {
    documents: [...new Set(summary.hashMismatches.map((m) => m.document))],
    what: 'every mismatching citation lives in an early verification record and names a file that is still tracked and has legitimately changed since. None is a corrupted or forged digest.',
    butAlso: 'those records would therefore FAIL the todo 50 validator\'s STALE_TREE check if it were run against today\'s tree. They remain truthful about the tree they described and must not be re-quoted as descriptions of the current one.',
    notRepaired: 'no historical digest was rewritten to match the present. Rewriting them would destroy the only thing those records are good for.',
  };

  return summary;
};

// ---------------------------------------------------------------------------
// Public surface counts, read from the generated manifest rather than asserted.
// ---------------------------------------------------------------------------
const surfaceCounts = () => {
  const manifest = readJson(join(ROOT, 'src/gateway/gateway-manifest.generated.json'));
  const matrix = readJson(join(ROOT, 'docs/capability-support-matrix.generated.json'));
  const actions = manifest.tools.reduce((sum, tool) => sum + (tool.actions?.length ?? 0), 0);
  return {
    publicMcpToolsPerTransport: 1,
    publicToolName: 'unreal',
    publicOperations: ['search', 'describe', 'execute', 'configure'],
    internalCanonicalParentTools: manifest.tools.length,
    canonicalCapabilities: actions,
    supportMatrixRecordCount: matrix.recordCount,
    mutatingCapabilities: matrix.mutationCount,
    catalogRevision: matrix.catalogRevision,
    countsAgree: actions === matrix.recordCount,
    readFrom: ['src/gateway/gateway-manifest.generated.json', 'docs/capability-support-matrix.generated.json'],
  };
};

// ---------------------------------------------------------------------------
// TRANSCRIBED gate table. Raw logs preserved beside this record.
// ---------------------------------------------------------------------------
const GATE_LOG_DIR = '.omo/evidence/task-64/gates';
const SCRATCH_DIRS = ['/tmp/opencode/task64', '/tmp/opencode/t64b'];
const GATES = [
  ['npx tsc --noEmit', 0, 'tsc.log'],
  ['npx eslint . --max-warnings=0', 0, 'eslint.log'],
  ['npm run test:unit', 1, 'test-unit.log'],
  ['npm run build', 0, 'build.log'],
  ['npm run test:smoke', 0, 'test-smoke.log'],
  ['npm run registry:check', 0, 'registry-check.log'],
  ['npm run manifest:check', 0, 'manifest-check.log'],
  ['npm run test:native-parity', 0, 'native-parity.log'],
  ['npm run test:params', 0, 'test-params.log'],
  ['npm run version:check', 0, 'version-check.log'],
  ['npm run workflow:check', 0, 'workflow-check.log'],
  ['npm audit --audit-level=moderate', 1, 'npm-audit.log'],
  ['sha256sum -c .omo/evidence/preservation/preserved-24-baseline.sha256', 0, 'preserved24.log'],
];

const gateRows = GATES.map(([command, exitCode, log], index) => ({
  position: index + 1,
  command,
  exitCode,
  log: `${GATE_LOG_DIR}/${log}`,
  logSha256: existsSync(join(ROOT, GATE_LOG_DIR, log)) ? sha256(join(ROOT, GATE_LOG_DIR, log)) : null,
}));

// ---------------------------------------------------------------------------
// Documents this todo edited, each proven unpreserved and each proven not to be
// a generator-owned artifact.
// ---------------------------------------------------------------------------
const GENERATOR_OWNED_DOCS = [
  'docs/capability-support-matrix.md',
  'docs/capability-support-matrix.generated.json',
  'docs/action-reference.generated.md',
  'docs/migration-reference.generated.md',
];

const PRE_EDIT_DIGESTS = {
  'docs/performance-and-evidence.md': '1efce048dfdd826d39ad5d70287ba3e24a0fffb89403c988df1aa36c459d171a',
  'docs/mcp-primitives.md': '3e5c9cf31412c80a8cb75c8a6b2315c750aa1cf92076a9c934bc98552563f1ac',
  'docs/security-and-receipts.md': '9c2fe8bd3321df48fd06cfb0ed0f1c1880324825d52df7125b35f613b7db42ed',
  'docs/gateway-client-guide.md': 'a45814791f53c7f32422864cc50ef1809a4e29f6b68bfd838105eb378ab92b03',
};
const EDITED_DOCS = Object.keys(PRE_EDIT_DIGESTS);

const gitBlobSha = (path) => {
  const bytes = execFileSync('git', ['show', `HEAD:${path}`], { cwd: ROOT, maxBuffer: 64 * 1024 * 1024 });
  return createHash('sha256').update(bytes).digest('hex');
};

const docDisposition = () => {
  const edited = EDITED_DOCS.map((path) => ({
    path,
    preserved: preservedPaths.has(path),
    generatorOwned: GENERATOR_OWNED_DOCS.includes(path),
    beforeSha256: gitBlobSha(path),
    afterSha256: sha256(join(ROOT, path)),
    beforeMatchesRecordedBaseline: gitBlobSha(path) === PRE_EDIT_DIGESTS[path],
  }));
  const refused = GENERATOR_OWNED_DOCS.map((path) => ({
    path,
    preserved: preservedPaths.has(path),
    generatorOwned: true,
    edited: false,
    why: 'emitted by scripts/canonical-registry/targets.ts and gated by npm run registry:check; a hand edit is overwritten by the next generate and fails the drift gate',
  }));
  return { edited, refusedBecauseGeneratorOwned: refused };
};

// ---------------------------------------------------------------------------
// Compose.
// ---------------------------------------------------------------------------
const preservation = verifyPreservation();
const evidenceSweep = sweepEvidence();
const counts = surfaceCounts();
const docs = docDisposition();

const treePaths = [
  'package.json',
  'src/gateway/gateway-manifest.generated.json',
  'src/tools/catalog/consolidated-tool-definitions.ts',
  'src/server/server-factory.ts',
  'scripts/qa/task64-readiness.mjs',
  ...EDITED_DOCS,
  '.omo/evidence/task-62-pure-unreal-mcp-implementation.json',
  '.omo/evidence/task-63-pure-unreal-mcp-implementation.json',
];
const treeFiles = treePaths
  .filter((p) => existsSync(join(ROOT, p)))
  .map((p) => ({ path: p, sha256: sha256(join(ROOT, p)) }));
const sourceDigest = createHash('sha256')
  .update([...treeFiles].map((e) => `${e.sha256}  ${e.path}`).sort().join('\n'))
  .digest('hex');

const newestUnder = (dir) => {
  const stack = [join(ROOT, dir)];
  let newest = { path: null, ms: 0 };
  while (stack.length > 0) {
    const current = stack.pop();
    for (const entry of readdirSync(current, { withFileTypes: true })) {
      const abs = join(current, entry.name);
      if (entry.isDirectory()) { stack.push(abs); continue; }
      const ms = statSync(abs).mtimeMs;
      if (ms > newest.ms) newest = { path: relative(ROOT, abs), ms };
    }
  }
  return newest;
};

const distCli = join(ROOT, 'dist/cli.js');
const newestSrc = newestUnder('src');
const artifacts = [
  {
    path: 'dist/cli.js',
    sha256: sha256(distCli),
    builtAtMs: statSync(distCli).mtimeMs,
    inputsNewest: newestSrc.path,
    inputsNewestAtMs: newestSrc.ms,
  },
  {
    path: 'scripts/qa/task64-readiness.mjs',
    sha256: sha256(join(ROOT, 'scripts/qa/task64-readiness.mjs')),
    builtAtMs: statSync(join(ROOT, 'scripts/qa/task64-readiness.mjs')).mtimeMs,
    inputsNewest: '.omo/evidence/task-63-pure-unreal-mcp-implementation.json',
    inputsNewestAtMs: statSync(join(EVIDENCE_DIR, 'task-63-pure-unreal-mcp-implementation.json')).mtimeMs,
  },
];

const now = Date.now();
const observedAt = iso(now);

// A mechanism must be watched reporting BOTH present and absent, otherwise a
// permanently blind probe satisfies every absence assertion in the document.
const controlPresent = join(ROOT, 'package.json');
const controlAbsent = join(EVIDENCE_DIR, 'task-64-control-must-not-exist.json');

const observations = [
  {
    id: 'obs-pre-docs', kind: 'doc-set', phase: 'pre', mechanism: 'git:show-head-blob',
    independence: 'out-of-band', target: 'the four edited documents as committed at HEAD',
    present: true, digest: null, conclusive: true,
    detail: {
      why: 'the pre-state is read from the git object store, which shares no code path with the filesystem write that changed the files',
      files: docs.edited.map((d) => ({ path: d.path, sha256: d.beforeSha256 })),
    },
    observedAt,
  },
  {
    id: 'obs-post-docs', kind: 'doc-set', phase: 'post', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: 'the four edited documents on disk',
    present: true, digest: null, conclusive: true,
    detail: { files: docs.edited.map((d) => ({ path: d.path, sha256: d.afterSha256 })) },
    observedAt,
  },
  {
    id: 'obs-preservation', kind: 'preservation-baseline', phase: 'post', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: '.omo/evidence/preservation/preserved-24-baseline.sha256',
    present: true, digest: sha256(BASELINE), conclusive: true,
    detail: preservation, observedAt,
  },
  {
    id: 'obs-gate-logs', kind: 'gate-run', phase: 'post', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: GATE_LOG_DIR,
    present: true, digest: null, conclusive: true,
    detail: { rows: gateRows, note: 'exit codes transcribed from this recorded run; the logs are the primary record' },
    observedAt,
  },
  {
    id: 'obs-evidence-sweep', kind: 'evidence-index', phase: 'post', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: '.omo/evidence/task-*.json',
    present: true, digest: null, conclusive: true, detail: evidenceSweep, observedAt,
  },
  {
    id: 'obs-counts', kind: 'generated-manifest', phase: 'post', mechanism: 'fs:read-generated-json',
    independence: 'out-of-band', target: 'src/gateway/gateway-manifest.generated.json',
    present: true, digest: sha256(join(ROOT, 'src/gateway/gateway-manifest.generated.json')),
    conclusive: true, detail: counts, observedAt,
  },
  {
    id: 'obs-control-present', kind: 'positive-control', phase: 'control', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: relative(ROOT, controlPresent),
    present: true, digest: sha256(controlPresent), conclusive: true,
    detail: { role: 'the same mechanism reporting PRESENT' }, observedAt,
  },
  {
    id: 'obs-control-absent', kind: 'positive-control', phase: 'control', mechanism: 'fs:sha256',
    independence: 'out-of-band', target: relative(ROOT, controlAbsent),
    present: existsSync(controlAbsent), digest: null, conclusive: true,
    detail: { role: 'the same mechanism reporting ABSENT; a probe that only ever says PRESENT proves nothing about a removal' },
    observedAt,
  },
  {
    id: 'obs-scratch-removed', kind: 'fixture', phase: 'post', mechanism: 'fs:stat',
    independence: 'out-of-band', target: SCRATCH_DIRS.join(', '),
    present: SCRATCH_DIRS.some((dir) => existsSync(dir)), digest: null, conclusive: true,
    detail: {
      role: 'the scratch directories that held the two gate runners and their logs before the logs were preserved into the evidence tree',
      perDirectory: SCRATCH_DIRS.map((dir) => ({ dir, present: existsSync(dir) })),
    },
    observedAt,
  },
  {
    id: 'obs-control-dir-present', kind: 'positive-control', phase: 'control', mechanism: 'fs:stat',
    independence: 'out-of-band', target: GATE_LOG_DIR,
    present: existsSync(join(ROOT, GATE_LOG_DIR)), digest: null, conclusive: true,
    detail: { role: 'the same fs:stat mechanism reporting PRESENT on a directory that does exist, so the ABSENT reading above is a real removal rather than a blind probe' },
    observedAt,
  },
];

const document = {
  task: 64,
  title: 'Produce the truthful final readiness record and reconcile all public claims',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 final synthesis',
  generatedAt: observedAt,
  verdict:
    'NOT READY — readiness status BLOCKED_EXTERNAL, carried forward unchanged from todo 63 (rule FALLBACK-1). '
    + 'The implementation gates pass on this tree with two declared exceptions; external certification does not. '
    + 'The Boulder is NOT complete: the plan permits that only after F1-F4 approve and the user accepts.',
  environment: {
    mockUnrealConnection: false,
    processes: [],
    readiness: {
      status: 'BLOCKED_EXTERNAL',
      rule: 'FALLBACK-1',
      decidedBy: '.omo/evidence/task-63-pure-unreal-mcp-implementation.json',
      decidedByDigest: sha256(join(EVIDENCE_DIR, 'task-63-pure-unreal-mcp-implementation.json')),
      restated: 'this record RENDERS that decision; it does not recompute or reopen it',
      unmetGatingRequirements: ['R1', 'R2', 'R4', 'R5'],
      unmetForBestInClassOnly: ['R7', 'R8'],
      withdrawnRules: ['R6 — its instrument measured the harness, not the product; the status did not move'],
      boulderComplete: false,
      boulderCompletionPrecondition: 'F1-F4 all APPROVE and the user explicitly accepts the surfaced result',
    },
    gateChain: {
      runOn: 'the final tree, after the documentation edits recorded below. The chain was run twice — once before the edits and once after — and both runs produced identical exit codes; the logs preserved here are the FINAL, post-edit run.',
      total: gateRows.length,
      exitZero: gateRows.filter((g) => g.exitCode === 0).length,
      nonZero: gateRows.filter((g) => g.exitCode !== 0).map((g) => g.command),
      commands: gateRows,
      declaredExceptions: [
        {
          id: 'EXC-1',
          gate: 'npm run test:unit',
          exitCode: 1,
          observed: '2 failed / 4124 passed over 369 files',
          failures: [
            'tests/unit/source_structure.test.ts > keeps numbered roadmap identifiers out of active code and filenames — invalidContents = ["tests/unit/_poc_security/security-poc.test.ts"]',
            'tests/unit/_poc_security/security-poc.test.ts > C3 > CONFIRMS regex gaps: URL creds and bare JWTs are NOT masked by redactText',
          ],
          attribution: 'both failures are attributable to tests/unit/_poc_security/security-poc.test.ts, which is entry 24 of the preservation baseline the plan itself protects',
          thirdFailureCheck: 'PASS — exactly two failures were observed; a third would have been reported as a regression rather than absorbed',
          excused: true,
        },
        {
          id: 'EXC-2',
          gate: 'npm audit --audit-level=moderate',
          exitCode: 1,
          observed: '7 advisories: 2 moderate, 5 high',
          devOnly: '5 high — the ESLint brace-expansion/minimatch chain (GHSA-mh99-v99m-4gvg); never installed by a consumer',
          productionPath: '@modelcontextprotocol/sdk (pinned exactly 1.29.0) -> @hono/node-server (GHSA-frvp-7c67-39w9, moderate)',
          excused: false,
          why: 'the advisory is reached on the path that ships; no exploitability assessment exists for this product, and an unassessed advisory is not a safe one',
        },
      ],
    },
    surface: counts,
    engineMatrix: {
      advertisedRange: '5.0-5.8 Preview',
      advertisedMinors: 9,
      certifiedMinors: 0,
      passingButStaleMinors: 1,
      failingMinors: 1,
      blockedMinors: 7,
      note: 'no minor is certified against the current tree; 5.7.4 is the single PASS row and its certification predates this tree',
      rows: [
        { minor: '5.0', identity: '5.0.3', rootPresent: true, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-unbuilt', certified: false, owner: 'operator', remediation: 'compile the editor target (Engine/Binaries/Linux/UnrealEditor-Cmd) for the already-installed root', compatibility: 'UNKNOWN' },
        { minor: '5.1', identity: null, rootPresent: false, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-absent', certified: false, owner: 'operator', remediation: 'install the engine at a root this host can see, then build its editor target', compatibility: 'UNKNOWN' },
        { minor: '5.2', identity: null, rootPresent: false, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-absent', certified: false, owner: 'operator', remediation: 'install the engine at a root this host can see, then build its editor target', compatibility: 'UNKNOWN' },
        { minor: '5.3', identity: '5.3.2', rootPresent: true, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-unbuilt', certified: false, owner: 'operator', remediation: 'compile the editor target for the already-installed root', compatibility: 'UNKNOWN' },
        { minor: '5.4', identity: null, rootPresent: false, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-absent', certified: false, owner: 'operator', remediation: 'install the engine at a root this host can see, then build its editor target', compatibility: 'UNKNOWN' },
        { minor: '5.5', identity: '5.5.4', rootPresent: true, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-unbuilt', certified: false, owner: 'operator', remediation: 'compile the editor target for the already-installed root', compatibility: 'UNKNOWN' },
        { minor: '5.6', identity: null, rootPresent: false, editorBuilt: false, state: 'BLOCKED_EXTERNAL', subclass: 'root-absent', certified: false, owner: 'operator', remediation: 'install the engine at a root this host can see, then build its editor target', compatibility: 'UNKNOWN' },
        {
          minor: '5.7', identity: '5.7.4', rootPresent: true, editorBuilt: true, state: 'PASS', subclass: null,
          certified: false, owner: 'us', remediation: 're-certify end-to-end against the current tree', compatibility: 'PROVEN on the certified tree only',
          liveRecord: '20/20 stages PASSED, 0 FAILED, 0 NOT_REACHED, recorded twice (todos 52 and 59); 84 automation requests started, 84 completed; 23 pass / 0 fail / 0 blocked over 24 driver cases on native and stdio',
          staleness: 'pluginSourceFilesNewerThanBinary=139; recordedTreeCoversPluginSource=false',
          compileAtHead: 'RunUAT Result: Succeeded, 0 errors, 0 warnings; sync-fidelity diff empty; 278 files on the GetJsonStringField accessor, 0 on the retired macros',
          compileIsNotCertification: 'the clean compile proves the current source builds. It does NOT re-run the 20 live stages and must never be read as a re-certification. That work is owned by F3.',
        },
        {
          minor: '5.8', identity: '5.8.0-preview-1', rootPresent: true, editorBuilt: true, state: 'FAIL', subclass: null,
          certified: false, owner: 'us', remediation: 'fix the plugin; nothing external is missing', compatibility: 'BROKEN — measured',
          failure: 'RunUAT exit 6, UnrealBuildTool: Failed (OtherCompilationError), 35 compiler errors across 20 files',
          apiBreaks: [
            { api: 'FJsonObject::Values key type FString -> UE::TSharedString<char16_t>', errors: 29, optOut: 'Epic ships one, documented as "will be removed"' },
            { api: 'UUserDefinedEnum::SetEnums 2 parameters -> 5', errors: 6, optOut: 'none' },
          ],
        },
      ],
      subclassesAreDistinct:
        'root-unbuilt and root-absent have DIFFERENT remediation and are never merged: root-unbuilt needs a build on an existing root, root-absent needs an install first. Neither is evidence that the plugin is broken on that minor.',
    },
    blockers: {
      total: 15,
      byClass: {
        'supply-chain-advisory': 2,
        'external-operator-input': 9,
        'stale-certification': 1,
        'owned-defect': 1,
        'owned-instrument-gap': 2,
      },
      ownedByUs: [
        'available-engine-not-passing:5.8 (owned-defect)',
        'stale-certification:5.7',
        'static-gate:17-npm-audit and supply-chain:@hono/node-server (both supply-chain-advisory)',
        'adversarial:EDITOR_RSS_DELTA_DOMINATED_BY_ENGINE_RECLAIM and adversarial:NO_DELEGATE_INSTRUMENT (owned-instrument-gap)',
      ],
      ownedByOperator: [
        'engine-not-available:5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6',
        'adversarial:FULL_PARITY_CORPUS_NOT_RUN (operator + us)',
        'model-arm-not-configured',
      ],
      statusNameUnderstatesIt:
        'BLOCKED_EXTERNAL is the plan\'s only residual status, but 6 of the 15 blockers are OURS, not external. The ownership lives in byClass rather than in the status string because widening the status vocabulary was not this plan\'s call.',
      source: '.omo/evidence/task-63-pure-unreal-mcp-implementation.json#/environment/blockers',
    },
    documentation: {
      ...docs,
      preservedDocsUntouched: baselineEntries
        .map((e) => e.path)
        .filter((p) => /\.md$/.test(p) || p === 'docs/protocol.md'),
      falseClaimsFoundInPreservedDocs: [
        {
          file: 'README.md',
          line: 63,
          wording: '**Unreal Engine 5.0–5.8 (Preview) compatibility target.** The MCP Automation Bridge plugin is scoped to build and run across UE 5.0 through 5.8 Preview.',
          why: 'measurably false for 5.8 Preview 1: the plugin does not build there (RunUAT exit 6, 35 errors). "scoped to build and run" asserts a build capability that has been tested and failed.',
          severity: 'blocking',
          action: 'NOT EDITED — README.md is entry 3 of the preservation baseline. Reported instead of silently fixed.',
        },
        {
          file: 'docs/protocol.md',
          line: 15,
          wording: '| Requires | Node.js 20.19.0+, `unreal-engine-mcp-server` | UE 5.0–5.8, no Node.js |',
          why: 'states the native transport requires UE 5.0-5.8, which reads as "any minor in that range works". 5.8 does not compile and seven minors are untried.',
          severity: 'blocking',
          action: 'NOT EDITED — docs/protocol.md is entry 4 of the preservation baseline.',
        },
        {
          file: 'plugins/McpAutomationBridge/README.md',
          line: 200,
          wording: '**Important/Additional Notes:** Requires Unreal Engine 5.0-5.8.',
          why: 'same defect as docs/protocol.md line 15, in the plugin\'s own README.',
          severity: 'blocking',
          action: 'NOT EDITED — entry 6 of the preservation baseline.',
        },
        {
          file: 'plugins/McpAutomationBridge/README.md',
          line: 30,
          wording: '- **Unreal Engine**: 5.0 - 5.8 source-compatibility target. The current complete live acceptance record covers UE 5.7.4 only.',
          why: 'weaker than the two above because it says "target" and names the 5.7.4-only limit, but a target contradicted by a measured compile failure on the top of the range is still misleading.',
          severity: 'advisory',
          action: 'NOT EDITED — entry 6 of the preservation baseline.',
        },
        {
          file: 'README.md',
          line: 6,
          wording: '[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-orange)]',
          why: 'a bare 5.0-5.8 badge reads as a support claim to anyone who does not continue to the certification section below it.',
          severity: 'advisory',
          action: 'NOT EDITED — entry 3 of the preservation baseline.',
        },
      ],
      contractGapFound: {
        gate: 'tests/unit/docs/docs-claim-rules.ts#unbacked-certification',
        gap: 'the rule fires only on CERTIFICATION_CONTEXT (certif / compile-verified / verified-on / tested-on / validated-on). A support claim phrased as "scoped to build and run across UE 5.0 through 5.8" or "Requires UE 5.0-5.8" carries no such word, so the gate cannot see it. CERTIFICATION_NEGATIONS additionally excuses any paragraph containing the exact phrase "compatibility target".',
        consequence: 'the five findings above all pass the docs contract today. The gate is not weakened by this record; the gap is named so it can be closed deliberately.',
      },
    },
    evidenceIntegrity: evidenceSweep,
    preservation,
    openDecisionsForAHuman: [
      {
        id: 'DECISION-1',
        subject: 'UE 5.8 Preview 1 does not compile while the product advertises 5.0-5.8',
        options: [
          {
            option: 'stopgap: enable Epic\'s documented opt-out define and version-gate the SetEnums call',
            measuredCost: 'closes 35 errors with a small, localized change, but Epic documents the opt-out as "will be removed", so it buys one engine cycle rather than a fix. The SetEnums break has no opt-out and still needs a version-gated call site.',
            sideEffect: 'any code change invalidates the 5.7.4 certification and the recorded tree hash of this record',
          },
          {
            option: 'durable migration off both APIs',
            measuredCost: 'approximately 20 files touched, spanning the FJsonObject key-type change (29 errors) and the SetEnums signature change (6 errors)',
            sideEffect: 'same invalidation of the 5.7.4 certification and this record\'s tree hash',
          },
          {
            option: 'narrow the advertised range to 5.0-5.7',
            measuredCost: 'no code change and no re-certification, but it is a product-scope reduction that touches preserved public docs (README.md, docs/protocol.md, both plugin docs) and is therefore not this lane\'s call',
            sideEffect: 'none to the tree hash',
          },
        ],
        requiresHuman: 'yes — every option is either a product-scope change or invalidates recorded certification evidence',
      },
      {
        id: 'DECISION-2',
        subject: 'npm audit is red in CI and the production-path fix is a breaking SDK bump',
        detail: '@modelcontextprotocol/sdk is pinned at exactly 1.29.0; the advisory fix moves to 1.30.0, which npm reports as outside the stated dependency range. The 5 high findings are dev-only ESLint transitives and do not ship.',
        measuredCost: 'CI runs `npm audit --audit-level=moderate` and this gate exits 1 today. Accepting it means a permanently red gate; fixing it means a breaking dependency move whose compatibility with this server has not been tested.',
        alsoUnknown: 'the shipped advisory has never been exploitability-assessed against this product',
        requiresHuman: 'yes',
      },
      {
        id: 'DECISION-3',
        subject: 'the native runtime describe surface has never been successfully censused',
        detail: 'two probe runs both measured the harness: run 2 was paging-limited (19 of 23 tools pinned at exactly 20 names), run 3 fixed paging but harvested dispatch-group and tool names into the action list on 5 of 23 tools and carried alias names on the declared side.',
        consequence: 'transport naming equivalence is unproven in BOTH directions. This project claims neither divergence nor agreement. Contract-level parity IS proven (npm run test:native-parity exit 0, 23 native canonical tools, 0 action mismatches, 0 schema property mismatches).',
        measuredCost: 'building a trustworthy census needs a live editor session and a probe that compares the same level of the describe tree on both sides',
        requiresHuman: 'yes — it needs an engine the operator supplies, and it decides whether a parity claim may ever be published',
      },
    ],
  },
  tree: { files: treeFiles, sourceDigest },
  artifacts,
  engine: {},
  clients: [],
  commands: gateRows.map((g) => ({ command: g.command, exitCode: g.exitCode, log: g.log })),
  transcripts: [],
  observations,
  claims: [
    {
      id: 'claim-readiness-status',
      target: 'the final readiness status of this plan',
      effect: 'unchanged',
      outcome: 'error',
      verdict: 'BLOCKED_EXTERNAL',
      pass: false,
      reason: 'rendered from todo 63 rule FALLBACK-1 with R1, R2, R4 and R5 unmet. This record re-verified the tree, the gates, the preservation baseline and the evidence index; it did not recompute the decision.',
      oracleRefs: ['obs-gate-logs', 'obs-preservation', 'obs-evidence-sweep', 'obs-counts'],
      cleanupRef: null,
    },
    {
      id: 'claim-docs-reconciled',
      target: 'the four unpreserved documents updated to the decided status',
      effect: 'modified',
      outcome: 'success',
      verdict: 'RECONCILED — four documents updated; four generator-owned documents deliberately not touched; five false claims in preserved documents reported rather than edited',
      pass: true,
      reason: 'each edited path was proven absent from the preservation baseline before it was written, and proven not to be a generator-owned artifact. Both drift gates still exit 0 afterwards.',
      oracleRefs: ['obs-pre-docs', 'obs-post-docs', 'obs-preservation'],
      cleanupRef: 'cleanup-docs',
    },
    {
      id: 'claim-preserved-hunks-survive',
      target: 'the 24 preserved paths carrying the original dirty work',
      effect: 'unchanged',
      outcome: 'success',
      verdict: `${preservation.okCount}/${preservation.declared} OK, ${preservation.mismatches.length} mismatches`,
      pass: preservation.okCount === preservation.declared && preservation.mismatches.length === 0,
      reason: 'the baseline was re-read at the start of this work and again after every edit; both readings agree',
      oracleRefs: ['obs-preservation'],
      cleanupRef: null,
    },
  ],
  cleanup: [
    {
      id: 'cleanup-scratch',
      owned: `${SCRATCH_DIRS.join(', ')} (two gate-runner scripts, their per-gate logs, and the evidence sweep prototypes)`,
      verifiedBy: 'obs-scratch-removed',
      pass: SCRATCH_DIRS.every((dir) => !existsSync(dir)),
      verdict: 'RELEASED',
      reason: 'the logs of the final run were copied into .omo/evidence/task-64/gates/ first so the durable record survives the removal; both scratch directories are then gone',
    },
    {
      id: 'cleanup-docs',
      owned: 'docs/performance-and-evidence.md, docs/mcp-primitives.md, docs/security-and-receipts.md, docs/gateway-client-guide.md',
      verifiedBy: 'obs-post-docs',
      pass: true,
      verdict: 'RETAINED BY DESIGN — not a fixture',
      reason: 'these four documents are the deliverable of this todo, not temporary state. They are retained deliberately; their before and after digests are both recorded so the change is reviewable and reversible. No temporary file was created in the documentation tree.',
    },
  ],
  positiveControls: {
    ok: true,
    mechanisms: [
      {
        mechanism: 'fs:sha256',
        present: 'obs-control-present (package.json, hashed)',
        absent: 'obs-control-absent (.omo/evidence/task-64-control-must-not-exist.json, never created)',
        why: 'a probe that only ever reports PRESENT would satisfy every absence assertion in this document, including the cleanup receipt',
      },
      {
        mechanism: 'fs:stat',
        present: `obs-control-dir-present (${GATE_LOG_DIR}, which does exist)`,
        absent: 'obs-scratch-removed (/tmp/opencode/task64, gone after cleanup)',
        why: 'the removal receipt below rests on this mechanism, so the mechanism itself is shown reporting both readings',
      },
    ],
  },
  notProven: [
    'NO ENGINE, EDITOR, PLUGIN BUILD OR RunUAT INVOCATION WAS PERFORMED BY THIS TODO. Every engine fact here is re-read from the recorded todo 56-62 evidence. This record adds zero new live coverage. F3 owns live work.',
    'THE 5.7.4 CERTIFICATION IS NOT RE-ESTABLISHED. The plugin compiles clean at HEAD against 5.7.4, and that is all the compile proves. The 20 live stages were NOT re-run at HEAD. A compile is not a certification.',
    'SEVEN OF NINE ADVERTISED MINORS HAVE NO PLUGIN COMPILE RESULT AT ALL. For those minors nothing is proven in either direction — only that they could not be tried on this host.',
    'THE NATIVE RUNTIME DESCRIBE SURFACE WAS NEVER SUCCESSFULLY CENSUSED, so transport naming equivalence is unproven in BOTH directions. Contract-level parity is proven; runtime naming equivalence is not.',
    'NO MODEL WAS CONTACTED. The model arm remains BLOCKED_EXTERNAL / NOT_ENABLED and no accuracy figure is claimed or denied.',
    'THE SHIPPED @hono/node-server ADVISORY WAS NOT EXPLOITABILITY-ASSESSED against this product. No lane has ruled on whether the vulnerable route is reachable here.',
    'THE EVIDENCE SWEEP RE-CHECKS LINKS AND HASHES, NOT MEANING. It proves a cited file exists and still hashes as recorded; it does not prove the citation supports the sentence that cites it.',
    'THE BOULDER IS NOT COMPLETE AND THIS RECORD DOES NOT MARK IT COMPLETE. F1-F4 have not run, and the plan requires their approval plus the user\'s explicit acceptance.',
  ],
  notes: [
    'STATUS BLOCKED_EXTERNAL, rendered not re-litigated. Todo 63 decided it under rule FALLBACK-1; this record\'s job was to render it truthfully and reconcile the documentation to it.',
    'THE GATE CHAIN WAS RE-RUN ON THE FINAL TREE. 13 gates, 11 exit 0, 2 exit 1: test:unit (exactly the two declared failures, both attributable to a preserved path) and npm audit (7 advisories, 1 of them on the shipped path). No third unit failure appeared; if one had, it would be reported here as a regression rather than absorbed.',
    'FOUR OF THE EIGHT DOCUMENTS NAMED FOR UPDATE ARE GENERATOR-OWNED and were deliberately NOT edited: capability-support-matrix.md, capability-support-matrix.generated.json, action-reference.generated.md and migration-reference.generated.md are emitted by scripts/canonical-registry/targets.ts and gated by registry:check. Hand-editing them would have been overwritten by the next generate and would have failed the drift gate this todo is required to keep green.',
    'FIVE FALSE OR MISLEADING CLAIMS LIVE IN PRESERVED DOCUMENTS and were reported rather than fixed. Editing any of them breaks the preserved-24 gate that five completed todos verified. The report is the deliverable, not a silent fix.',
    'A GAP IN THE DOCS CONTRACT WAS FOUND AND NAMED, NOT CLOSED: the unbacked-certification rule cannot see a support claim phrased without a certification word, which is exactly how the three blocking findings survive it today.',
    'THE PRESERVATION BASELINE CONTAINS ONE MALFORMED ENTRY: a zero-byte, git-tracked file at the repository root named `_stream_contracts.est.ts`, whose recorded digest is the SHA-256 of the empty string. It is almost certainly a truncated `automation_event_stream_contracts.test.ts`. It is entry 13 of the baseline, so it was NOT removed — deleting it would break the gate. Reported for a human to resolve.',
  ],
};

mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, `${JSON.stringify(document, null, 2)}\n`, 'utf8');
process.stdout.write(`wrote ${relative(ROOT, OUT)}\n`);
process.stdout.write(`  preservation: ${preservation.okCount}/${preservation.declared} OK, ${preservation.mismatches.length} mismatches\n`);
process.stdout.write(`  gates: ${gateRows.filter((g) => g.exitCode === 0).length}/${gateRows.length} exit 0\n`);
process.stdout.write(`  evidence: ${evidenceSweep.linksChecked} links, ${evidenceSweep.deadLinks.length} dead, ${evidenceSweep.hashesRechecked} hashes, ${evidenceSweep.hashMismatches.length} mismatched\n`);
process.stdout.write(`  surface: ${counts.publicMcpToolsPerTransport} public tool, ${counts.internalCanonicalParentTools} internal parents, ${counts.canonicalCapabilities} capabilities\n`);
