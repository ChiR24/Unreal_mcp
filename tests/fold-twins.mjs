// Twin cases for folded capability families.
//
// A folded family advertises one primary action whose selector value picks the
// old handler action; every old name is still callable as a folded legacy pair
// carrying the selector value it implied. The static suites keep exercising the
// old names, so for each (tool, primary) this derives ONE twin from the first
// case that names a folded member: identical arguments, `action` set to the
// primary, plus the member's pinned selector value. The twin runs right after
// its source case, so it sees the same setup state and captured values. Both
// the live runner and the parameter audit apply it, so the advertised action
// is covered by the same evidence as the name it replaced.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const registryPath = path.join(
  path.dirname(fileURLToPath(import.meta.url)),
  '../src/tools/catalog/capabilities/generated/canonical-registry.generated.json'
);

let foldIndex;

/** tool -> folded action -> { primary, pins } */
function loadFoldIndex() {
  if (foldIndex) return foldIndex;
  foldIndex = new Map();
  const registry = JSON.parse(fs.readFileSync(registryPath, 'utf8'));
  for (const record of Array.isArray(registry.records) ? registry.records : []) {
    const tool = record?.routing?.parentTool;
    const legacyIds = Array.isArray(record?.legacyIds) ? record.legacyIds : [];
    const primary = legacyIds[0]?.action;
    if (typeof tool !== 'string' || typeof primary !== 'string') continue;
    // A pure alias carries no pins; the twin then names the selector's default
    // (the primary's own variant) so the parameter is exercised explicitly.
    const selector = record?.routing?.dispatchBy?.param;
    const selectorDefault = typeof selector === 'string' ? record?.schemas?.input?.properties?.[selector]?.default : undefined;
    const fallbackPins = typeof selector === 'string' && selectorDefault !== undefined ? { [selector]: selectorDefault } : {};
    for (const legacy of legacyIds.slice(1)) {
      if (legacy?.folded === undefined || typeof legacy.action !== 'string') continue;
      if (!foldIndex.has(tool)) foldIndex.set(tool, new Map());
      const pins = Object.keys(legacy.folded).length > 0 ? legacy.folded : fallbackPins;
      foldIndex.get(tool).set(legacy.action, { primary, pins });
    }
  }
  return foldIndex;
}

const caseKey = (tool, action) => `${tool}${action}`;

function twinExpectation(expected) {
  if (typeof expected !== 'string' || !expected.startsWith('success')) return expected;
  // A twin repeats a mutation its source already made, so the narrow
  // idempotency alternatives the grammar allows on success-primary cases apply.
  const alternatives = expected.split('|').map((entry) => entry.trim());
  for (const extra of ['already exists', 'not found']) {
    if (!alternatives.includes(extra)) alternatives.push(extra);
  }
  return alternatives.join('|');
}

export function withFoldTwins(cases) {
  if (!Array.isArray(cases)) return cases;
  const index = loadFoldIndex();
  // One twin per (tool, primary), even when the primary already has a direct
  // case: the twin is what exercises the selector parameter the fold added.
  const covered = new Set();
  const out = [];
  for (const testCase of cases) {
    out.push(testCase);
    const tool = testCase?.toolName;
    const action = testCase?.arguments?.action;
    const fold = typeof tool === 'string' && typeof action === 'string' ? index.get(tool)?.get(action) : undefined;
    if (fold === undefined || covered.has(caseKey(tool, fold.primary))) continue;
    covered.add(caseKey(tool, fold.primary));
    out.push({
      scenario: `${testCase.scenario ?? action} [twin: ${fold.primary}]`,
      toolName: tool,
      arguments: { ...testCase.arguments, action: fold.primary, ...fold.pins },
      expected: twinExpectation(testCase.expected),
      ...(testCase.timeoutMs === undefined ? {} : { timeoutMs: testCase.timeoutMs }),
      ...(testCase.consent === undefined ? {} : { consent: testCase.consent }),
      foldTwinOf: action
    });
  }
  return out;
}
