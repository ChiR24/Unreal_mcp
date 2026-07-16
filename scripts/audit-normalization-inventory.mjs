// Literal inventory audit CLI for Task 5 manual QA.
// Run: node --loader ts-node/esm scripts/audit-normalization-inventory.mjs
// Prints exact counts and exercises one proven synonym mapping + the four
// delete-target non-merges with binary PASS/FAIL lines (exit 1 on any failure).
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { readArtifact } from '../src/tools/catalog/capabilities/normalization/io.ts';
import { naiveNameOnlyCanonicalId } from '../src/tools/catalog/capabilities/normalization/naive.ts';

const here = dirname(fileURLToPath(import.meta.url));
const ARTIFACT = resolve(here, '../src/tools/catalog/capabilities/normalization-inventory.json');

const inv = readArtifact(ARTIFACT);
const m = inv.metrics;
const byKey = new Map(inv.occurrences.map((o) => [o.occurrenceKey, o]));

console.log('=== Task 5 normalization inventory audit ===');
console.log(`occurrences              : ${m.occurrenceCount}`);
console.log(`distinct action names    : ${m.distinctActionNames}`);
console.log(`duplicate names          : ${m.duplicateNames}`);
console.log(`duplicate-name occ        : ${m.duplicateNameOccurrences}`);
console.log(`max exact-name reductions : ${m.maxExactNameReductions}`);
console.log(`verb-family (a/c/s/cfg)   : ${m.verbFamilyAddCreateSetConfigure}`);
console.log(`unclassified occurrences  : ${m.unclassifiedOccurrences}`);
console.log(`canonical collisions      : ${m.canonicalCollisions}`);
console.log(`canonical definitions     : ${inv.canonicalDefinitions.length}`);
console.log(
  `class counts (A-F only)  : A=${m.classificationCounts.A} B=${m.classificationCounts.B} ` +
    `C=${m.classificationCounts.C} D=${m.classificationCounts.D} E=${m.classificationCounts.E} ` +
    `F=${m.classificationCounts.F}`,
);
console.log(`class P (removed)         : 0 (taxonomy is strictly A-F)`);
console.log(
  `route dispositions       : total=${inv.routeDispositions.length} ` +
    `unresolved=${m.routeDispositionUnresolved} ` +
    `status(hidden/raw/dead)=${m.routeStatusCounts.hidden}/${m.routeStatusCounts.raw}/${m.routeStatusCounts.dead} ` +
    `disp(promote/map/remove)=${m.routeDispositionCounts.promote}/${m.routeDispositionCounts.map}/${m.routeDispositionCounts.remove}`,
);
console.log('');

let failures = 0;
function check(label, cond) {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${label}`);
  if (!cond) failures += 1;
}

check('occurrenceCount === 1288', m.occurrenceCount === 1288);
check('duplicateNames === 36', m.duplicateNames === 36);
check('duplicateNameOccurrences === 83', m.duplicateNameOccurrences === 83);
check('maxExactNameReductions === 47', m.maxExactNameReductions === 47);
check('verbFamilyAddCreateSetConfigure === 801', m.verbFamilyAddCreateSetConfigure === 801);
check('unclassifiedOccurrences === 0', m.unclassifiedOccurrences === 0);
check('canonicalCollisions === 0', m.canonicalCollisions === 0);
check('taxonomy is strictly A-F (no P class)', !('P' in m.classificationCounts));
check('routeDispositions total === 67 (v2 expected)', inv.routeDispositions.length === 67);
check('routeDisposition unresolved === 0', m.routeDispositionUnresolved === 0);
check(
  'route status: dead20/raw14/hidden33 (v2)',
  m.routeStatusCounts.dead === 20 && m.routeStatusCounts.raw === 14 && m.routeStatusCounts.hidden === 33,
);
check(
  'route disposition: promote43/map16/remove8 (v2)',
  m.routeDispositionCounts.promote === 43 && m.routeDispositionCounts.map === 16 && m.routeDispositionCounts.remove === 8,
);
check(
  'invented MRQ audio routes absent',
  !inv.routeDispositions.some((r) => ['cancel_render', 'get_render_progress', 'get_render_status'].includes(r.route) && r.domain === 'audio'),
);

// Proven synonym mapping: console_command surfaced under two tools collapses to one canonical.
const editorConsole = byKey.get('control_editor:console_command');
const systemConsole = byKey.get('system_control:console_command');
check(
  'proven synonym: control_editor:console_command and system_control:console_command share one canonical id',
  editorConsole !== undefined &&
    systemConsole !== undefined &&
    editorConsole.canonicalId === systemConsole.canonicalId &&
    editorConsole.canonicalId === 'cap:shared:console_command',
);

// Four delete-target non-merges (kept distinct by semantic namespace).
const deleteKeys = [
  'manage_asset:delete',
  'control_actor:delete',
  'manage_level:delete',
  'manage_sequence:delete',
];
const deleteCanon = deleteKeys.map((k) => byKey.get(k)?.canonicalId);
check(
  'four delete targets (asset/actor/level/sequence) remain distinct canonical ids',
  new Set(deleteCanon).size === 4,
);

// Also: build_environment:delete stays distinct from the other four.
check(
  'build_environment:delete also distinct (5 delete routes, none merged)',
  byKey.get('build_environment:delete')?.canonicalId !== undefined &&
    !deleteCanon.includes(byKey.get('build_environment:delete')?.canonicalId),
);

// The naive name-only classifier would wrongly collapse all deletes into one id.
const naiveIds = deleteKeys.map((k) => naiveNameOnlyCanonicalId('any', k.split(':')[1]));
check(
  'naive name-only classifier wrongly merges deletes (proves inventory is correct to reject it)',
  new Set(naiveIds).size === 1,
);

console.log('');
if (failures === 0) {
  console.log('AUDIT RESULT: PASS');
  process.exit(0);
} else {
  console.log(`AUDIT RESULT: FAIL (${failures} failed)`);
  process.exit(1);
}
