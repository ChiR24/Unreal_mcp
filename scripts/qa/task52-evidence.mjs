#!/usr/bin/env node
// Task 52 — compose the task-level evidence document.
//
// Two halves of this task produce evidence in different ways and the final
// document has to hold both without blurring them:
//
//   the OFFLINE simulation is deterministic and re-derivable right now, so it is
//   recomputed here rather than copied from a log. If the registry or the
//   plugin's `#if` lines moved since the certification ran, the numbers in this
//   document move with them and the difference is visible.
//
//   the LIVE certification happened once, at a moment, against bytes that a
//   disposable run then deleted. Its observations, claims and cleanup receipts
//   are carried over VERBATIM with their ids intact, because re-deriving them is
//   exactly what nobody can do — that is what made them evidence.
//
// The document is then handed to Task 50's validator, which re-checks every
// hash, pid, oracle link and cleanup link it can and refuses the ones it cannot.
//
// A SUPERSEDED RUN IS NOT DELETED. `--superseded FILE` carries an earlier
// certification's stage table and the reason it did not stand into
// `environment.certificationHistory`. The first 5.7 run scored 15/16 because its
// stdio driver REFUSED to measure a `dist/` that a concurrent lane had made stale
// mid-run — a correct refusal, and the only record that the guard has ever fired
// against a real event. Dropping it would leave a clean 20/20 that silently
// implies the guard is decorative, which is how two of Task 46's four
// "divergences" came to read as live HIGH defects.
//
// Run: node scripts/qa/task52-evidence.mjs [--certification FILE]
//                                          [--superseded FILE]... [--out FILE]

import { existsSync, readFileSync } from 'node:fs';

import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/task-52/engine-inventory.mjs';
import {
  buildProfileMatrix, collectNativeGates, defineProfile, evaluateNativeFeatures,
} from '../../tests/unit/task-52/profile-matrix.mjs';
import { surveyOwnedParent, findOrphanedProcesses } from '../../tests/unit/task-52/disposable-project.mjs';
import { EvidenceAggregator } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};
/** Every `--superseded FILE`, in the order given. */
const argsAll = (name) => process.argv.reduce((found, token, index) => (
  token === name && process.argv[index + 1] !== undefined ? [...found, process.argv[index + 1]] : found
), /** @type {string[]} */ ([]));

const REPO = process.cwd();
const CERTIFICATION = argOf('--certification', '.omo/evidence/task-52/certify-5.7.json');
const SUPERSEDED = argsAll('--superseded');
const OUT = argOf('--out', '.omo/evidence/task-52-pure-unreal-mcp-implementation.json');
const PLUGIN_ROOT = `${REPO}/plugins/McpAutomationBridge`;
const log = (line) => { process.stderr.write(`${line}\n`); };

const TREE = [
  'tests/unit/task-52/engine-identity.mjs',
  'tests/unit/task-52/engine-inventory.mjs',
  'tests/unit/task-52/preprocessor-conditions.mjs',
  'tests/unit/task-52/profile-matrix.mjs',
  'tests/unit/task-52/disposable-project.mjs',
  'tests/unit/task-52/project-scaffold.mjs',
  'tests/unit/task-52/certification-stages.mjs',
  'tests/unit/task-52/certification-drivers.mjs',
  'scripts/qa/task52-certify-engine.mjs',
  'scripts/qa/task52-reclaim.mjs',
  'scripts/qa/task52-evidence.mjs',
];

/** The four profile dimensions, spread so the matrix shows each one moving. */
function profiles(allPlugins) {
  return [
    defineProfile({ id: 'ue5.0-all-plugins-edit-native', engine: { major: 5, minor: 0, patch: 3 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.3-all-plugins-edit-native', engine: { major: 5, minor: 3, patch: 2 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.5-all-plugins-edit-native', engine: { major: 5, minor: 5, patch: 4 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.7-all-plugins-edit-native', engine: { major: 5, minor: 7, patch: 4 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.8-all-plugins-edit-native', engine: { major: 5, minor: 8, patch: 0 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.7-no-optional-plugins', engine: { major: 5, minor: 7, patch: 4 }, plugins: [] }),
    defineProfile({ id: 'ue5.7-pie', engine: { major: 5, minor: 7, patch: 4 }, plugins: allPlugins, editorState: 'pie' }),
    defineProfile({
      id: 'ue5.7-stdio-legacy-protocol', engine: { major: 5, minor: 7, patch: 4 }, plugins: allPlugins,
      client: { transport: 'stdio', protocolVersion: '2024-11-05' },
    }),
    defineProfile({
      id: 'ue5.7-native-legacy-protocol', engine: { major: 5, minor: 7, patch: 4 }, plugins: allPlugins,
      client: { transport: 'native', protocolVersion: '2024-11-05' },
    }),
  ];
}

const records = JSON.parse(readFileSync(
  `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
)).records;
const allPlugins = [...new Set(records.flatMap((entry) => entry.availability?.requiredPlugins ?? []))].sort();

const aggregator = new EvidenceAggregator({
  task: 52,
  title: 'Build capability simulation and disposable UE certification orchestration',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-6 version/certification lane',
});
aggregator.recordTree(TREE);

// ── the offline half, recomputed now ────────────────────────────────────────
const inventory = buildEngineInventory({ searchDirs: ['/data'] });
aggregator.document.environment.engineInventory = {
  table: formatInventoryTable(inventory),
  available: inventory.available.map(({ identity, ...rest }) => ({
    ...rest,
    buildVersionFile: identity.sources.buildVersion.file,
    buildVersionSha256: identity.sources.buildVersion.sha256,
    versionHeaderAgrees: identity.sources.versionHeader.agrees,
    gitDescribe: identity.sources.gitDescribe.raw,
    branch: identity.branch,
    compatibleChangelist: identity.compatibleChangelist,
  })),
  missing: inventory.missing,
  duplicates: inventory.duplicates,
  unusable: inventory.unusable,
  folderNameContradictions: inventory.folderNameContradictions,
  identifiedBy: 'Engine/Build/Build.version, corroborated by Version.h; git describe refines the channel only',
};

const nativeGates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });
const matrix = buildProfileMatrix({ records, profiles: profiles(allPlugins), nativeGates });
aggregator.document.environment.offlineSimulation = {
  capabilityRecords: records.length,
  optionalPluginsConsidered: allPlugins,
  nativeGateCensus: {
    conditionsFound: nativeGates.conditions.length,
    distinctConditions: nativeGates.distinctConditions,
    compatibilityHeader: String(nativeGates.compatibilityHeader).replace(`${REPO}/`, ''),
  },
  compiledFeaturesByEngine: [0, 1, 2, 3, 4, 5, 6, 7, 8].map((minor) => {
    const profile = defineProfile({ id: `5.${minor}`, engine: { major: 5, minor, patch: 0 }, plugins: allPlugins });
    const evaluated = evaluateNativeFeatures(profile, nativeGates);
    return {
      engine: `5.${minor}`,
      compiled: evaluated.compiledCount,
      excluded: evaluated.excludedCount,
      undecided: evaluated.undecidedCount,
      widgetGuidMap: evaluated.macros.MCP_HAS_WIDGET_VARIABLE_GUID_MAP,
      movieSceneShotMetadata: evaluated.macros.MCP_HAS_MOVIE_SCENE_SHOT_METADATA,
      materialEditorOnlyData: evaluated.macros.MCP_HAS_MATERIAL_EDITOR_ONLY_DATA,
    };
  }),
  matrix: matrix.rows.map(({ unavailableIds, ...row }) => ({ ...row, unavailableSample: unavailableIds.slice(0, 5) })),
};

// ── the live half, carried over verbatim ────────────────────────────────────
if (!existsSync(CERTIFICATION)) {
  log(`no certification document at ${CERTIFICATION}; the live half cannot be claimed`);
  aggregator.addNotProven(`LIVE CERTIFICATION: no run document at ${CERTIFICATION}. The offline simulation below stands on its own; nothing here claims an engine was exercised.`);
} else {
  const certification = JSON.parse(readFileSync(CERTIFICATION, 'utf8'));
  aggregator.document.engine = certification.engine;
  aggregator.document.clients = certification.clients ?? [];
  aggregator.document.commands = certification.commands ?? [];
  aggregator.document.transcripts = certification.transcripts ?? [];
  aggregator.document.observations = certification.observations ?? [];
  aggregator.document.claims = certification.claims ?? [];
  aggregator.document.cleanup = certification.cleanup ?? [];
  aggregator.document.artifacts = certification.artifacts ?? [];
  aggregator.document.environment.processes = certification.environment?.processes ?? [];
  aggregator.document.environment.certification = {
    source: CERTIFICATION,
    generatedAt: certification.generatedAt,
    verdict: certification.verdict,
    stages: certification.environment?.stages ?? [],
    workspace: certification.environment?.workspace ?? null,
    package: certification.environment?.package ?? null,
    binaryFreshness: certification.environment?.binaryFreshness ?? null,
    portCheck: certification.environment?.portCheck ?? null,
    editor: certification.environment?.editor ?? null,
    automation: certification.environment?.automation ?? null,
    drivers: certification.environment?.drivers ?? null,
    cleanup: certification.environment?.cleanup ?? null,
    blocked: certification.environment?.blocked ?? [],
  };
}

// ── superseded runs, kept because a refusal that fired is evidence ──────────
// Carried as a SUMMARY, not as claims/observations: those ids belong to a run
// whose pids and workspace are gone, and re-registering them would ask the
// validator to re-check readings that can no longer be re-checked. What survives
// is the stage table and the failing detail, verbatim.
if (SUPERSEDED.length > 0) {
  aggregator.document.environment.certificationHistory = SUPERSEDED.map((file) => {
    if (!existsSync(file)) return { source: file, present: false, note: 'referenced but not found at write time' };
    const prior = JSON.parse(readFileSync(file, 'utf8'));
    const stages = prior.environment?.stages ?? [];
    const failed = stages.filter((entry) => entry.ok !== true);
    return {
      source: file,
      present: true,
      generatedAt: prior.generatedAt,
      verdict: prior.verdict,
      supersededBy: CERTIFICATION,
      stagesPassed: stages.length - failed.length,
      stagesTotal: stages.length,
      failedStages: failed.map((entry) => ({ name: entry.name, detail: entry.detail })),
      whyItWasNotAProductDefect: failed.some((entry) => String(entry.detail).includes('STALE_BUILD'))
        ? 'assertDistFresh refused to drive `node dist/cli.js` because a concurrent lane regenerated '
          + 'src/utils/commands/console-command-policy.generated.ts after that build. The guard measured nothing '
          + 'rather than reporting a stale build\'s behaviour as the tree\'s. The re-run rebuilt dist/ first and the '
          + 'same guard passed, so this row records the guard working, not the product failing.'
        : null,
    };
  });
}

// ── residue, checked at the moment this document is written ─────────────────
const survey = surveyOwnedParent();
const strays = findOrphanedProcesses();
aggregator.document.environment.residueAtWrite = {
  ownedParent: survey.parent,
  liveRuns: survey.runs,
  strayProcesses: strays,
  clean: survey.runs.filter((entry) => entry.ownerAlive !== true).length === 0 && strays.length === 0,
};

const verdict = aggregator.document.environment.certification === undefined
  ? 'OFFLINE SIMULATION ONLY — no live certification document was available'
  : `${aggregator.document.environment.certification.verdict}; offline matrix over ${records.length} capabilities and ${nativeGates.conditions.length} native gates`;
const document = aggregator.finalize(verdict);
const validation = validateEvidence(document, { projectRoot: REPO });
document.environment.selfValidation = validation;
const written = aggregator.write(OUT);

log(describeRejections(validation));
log(`residue at write: ${JSON.stringify(document.environment.residueAtWrite.clean)}`);
log(`wrote ${written}`);
if (!validation.valid) process.exitCode = 1;
