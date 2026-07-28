// tests/unit/task-57/unbuilt-engine-blocker.test.ts
// Tasks 57 and 58 — locking the two properties a blocked certification record
// gets wrong most easily, on synthetic inputs with no engine present.
//
//   1. THE THIRD STATE. Nineteen of Task 52's twenty stages were never attempted
//      on these engines. Written as `false` the record asserts the plugin was
//      compiled and failed on UE 5.3/5.5 — a claim about code nobody built.
//      Written as `true` it is a fabrication. Only NOT_REACHED/null is honest,
//      and the mapping must survive an aggregator that wants booleans.
//
//   2. THE BLOCKER CLASS. Task 61 records ABSENT minors and this module records
//      PRESENT_BUT_UNBUILT ones. They share a status token and nothing else: one
//      operator has to obtain an engine, the other has to spend hours compiling
//      one they already have. A record that carries the wrong class, or a
//      remediation copied from a root whose toolchain was diagnosed differently,
//      sends somebody to do the wrong work.

import { describe, expect, it } from 'vitest';

import {
  BLOCKER_CLASSES, BLOCKER_SEVERITY, BLOCKER_STATUS, BUILD_PROGRESS, CLASS_REASONS,
  EDITOR_BUILD_FILES, READINESS, READINESS_FILES, STAGE_OUTCOMES, STAGE_OUTCOME_CONTRACT,
  STAGE_OUTCOME_TO_BOOLEAN, TASK52_STAGES, buildPresentButUnbuiltBlocker, buildStageTable,
  notReachedDetail, probeEditorBuildProgress, remediationStepsFor, summarizeStageTable,
} from './unbuilt-engine-blocker.mjs';

const ROOT = '/fake/UnrealEngine-5.3.2';
const at = (relative: string) => `${ROOT}/${relative}`;

/** A filesystem that exists only as a set of paths plus their contents. */
function fakeIo(present: readonly string[], files: Record<string, string> = {}, dirs: Record<string, string[]> = {}) {
  return {
    exists: (path: string) => present.includes(path),
    readFile: (path: string) => {
      if (files[path] === undefined) throw new Error(`ENOENT ${path}`);
      return files[path];
    },
    listDir: (path: string) => dirs[path] ?? null,
  };
}

const identityOf = (versionString: string) => ({
  usable: true,
  versionString,
  sources: {
    buildVersion: { file: at('Engine/Build/Build.version'), sha256: 'b'.repeat(64) },
    versionHeader: { agrees: true },
    gitDescribe: { raw: `${versionString}-release` },
  },
});

const readinessOf = (root: string) => ({
  ready: false,
  reason: READINESS.NO_COMPILED_EDITOR,
  root,
  detail: `${root} identifies as 5.3.2 but has no compiled ${READINESS_FILES.editorCmd}`,
  blockingConstraint: READINESS_FILES.editorCmd,
});

const bootstrappableToolchain = {
  bootstrappable: true,
  reason: 'PREBUILT_ASSEMBLIES',
  detail: 'UnrealBuildTool and AutomationTool are already compiled in the tree',
};

const brokenToolchain = {
  bootstrappable: false,
  reason: 'CANNOT_COMPILE_TOOLCHAIN',
  detail: 'netcoreapp3.1 needs a .NET Core 3.x SDK',
};

const partialProgress = {
  state: BUILD_PROGRESS.PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR,
  declaredModules: 455,
  moduleLibrariesInEngineBinaries: 276,
  sourceDistribution: true,
  makefilePresent: false,
};

// ─────────────────────────────── the third state ─────────────────────────────

describe('buildStageTable', () => {
  const table = buildStageTable({
    attemptStages: [
      { name: 'inventory.resolve', ok: false, detail: 'no compiled UnrealEditor-Cmd' },
      { name: 'tree.stable', ok: true, detail: '10 recorded source file(s) are byte-identical' },
    ],
    stoppedAt: 'inventory.resolve',
  });

  it('reports all twenty of Task 52 stages, in order', () => {
    expect(table).toHaveLength(20);
    expect(table.map((row) => row.stage)).toEqual([...TASK52_STAGES]);
  });

  // The load-bearing case: everything the run never got to is NEITHER outcome.
  it('marks every unattempted stage NOT_REACHED, never PASSED and never FAILED', () => {
    const unattempted = table.filter((row) => row.stage !== 'inventory.resolve' && row.stage !== 'tree.stable');
    expect(unattempted).toHaveLength(18);
    for (const row of unattempted) {
      expect(row.outcome).toBe(STAGE_OUTCOMES.NOT_REACHED);
      expect(row.outcome).not.toBe(STAGE_OUTCOMES.PASSED);
      expect(row.outcome).not.toBe(STAGE_OUTCOMES.FAILED);
      expect(row.detail).toBe(notReachedDetail('inventory.resolve'));
    }
  });

  // Byte-identical to the sentence Task 56 recorded, so the two records read the
  // same way in one aggregation instead of as two dialects of "not attempted".
  it('carries the same NOT_REACHED sentence Task 56 used', () => {
    expect(notReachedDetail('inventory.resolve')).toBe(
      'the run stopped at inventory.resolve; this stage was never attempted and no result may be inferred for it'
    );
  });

  it('records the stages that were attempted with their real outcome', () => {
    expect(table[0]).toMatchObject({ stage: 'inventory.resolve', outcome: STAGE_OUTCOMES.FAILED });
    expect(table[19]).toMatchObject({ stage: 'tree.stable', outcome: STAGE_OUTCOMES.PASSED });
  });

  it('summarises without folding NOT_REACHED into either column', () => {
    expect(summarizeStageTable(table)).toEqual({ total: 20, passed: 1, failed: 1, notReached: 18, attempted: 2 });
  });
});

describe('STAGE_OUTCOME_TO_BOOLEAN', () => {
  // An aggregator that wants a boolean must get an explicit null, not a default.
  it('maps NOT_REACHED to null — neither true nor false', () => {
    expect(STAGE_OUTCOME_TO_BOOLEAN[STAGE_OUTCOMES.PASSED]).toBe(true);
    expect(STAGE_OUTCOME_TO_BOOLEAN[STAGE_OUTCOMES.FAILED]).toBe(false);
    expect(STAGE_OUTCOME_TO_BOOLEAN[STAGE_OUTCOMES.NOT_REACHED]).toBeNull();
  });

  it('publishes the coercion rule with the mapping, so it travels with the evidence', () => {
    expect(STAGE_OUTCOME_CONTRACT.outcomes.NOT_REACHED).toBeNull();
    expect(STAGE_OUTCOME_CONTRACT.rule).toContain('never default them');
  });
});

// ────────────────────────── how far did the build get? ───────────────────────

describe('probeEditorBuildProgress', () => {
  const manifest = JSON.stringify({ BuildId: 'abc', Modules: { A: 'libUnrealEditor-A.so', B: 'libUnrealEditor-B.so', C: 'libUnrealEditor-C.so' } });

  // The observed 5.3.2/5.5.4 shape: a module manifest, most libraries, no
  // receipt, no executable. This is a RESUMED build, and telling an operator it
  // is a build from scratch misstates the cost by hours.
  it('separates a partially built editor from an unbuilt tree', () => {
    const io = fakeIo(
      [at(EDITOR_BUILD_FILES.modulesManifest), at(EDITOR_BUILD_FILES.sourceDistribution)],
      { [at(EDITOR_BUILD_FILES.modulesManifest)]: manifest },
      { [at(EDITOR_BUILD_FILES.binariesDir)]: ['libUnrealEditor-A.so', 'libUnrealEditor-B.so', 'UnrealEditor.modules'] }
    );
    const progress = probeEditorBuildProgress({ root: ROOT, io });
    expect(progress.state).toBe(BUILD_PROGRESS.PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR);
    expect(progress.declaredModules).toBe(3);
    expect(progress.moduleLibrariesInEngineBinaries).toBe(2);
    expect(progress.declaredModulesWithoutLibrary).toBe(1);
    expect(progress.targetReceiptPresent).toBe(false);
    expect(progress.editorExecutablePresent).toBe(false);
  });

  it('reports NO_BUILD_ARTIFACTS when the binaries directory holds no module library', () => {
    const io = fakeIo([], {}, { [at(EDITOR_BUILD_FILES.binariesDir)]: ['README.txt'] });
    expect(probeEditorBuildProgress({ root: ROOT, io }).state).toBe(BUILD_PROGRESS.NO_BUILD_ARTIFACTS);
  });

  // An unreadable directory is not an empty one. A probe that could not look must
  // not report "nothing was built".
  it('reports UNMEASURED rather than empty when the binaries directory cannot be listed', () => {
    const progress = probeEditorBuildProgress({ root: ROOT, io: fakeIo([]) });
    expect(progress.state).toBe(BUILD_PROGRESS.UNMEASURED);
    expect(progress.moduleLibrariesInEngineBinaries).toBeNull();
  });

  it('reports EDITOR_PRESENT the moment the launched binary exists', () => {
    const io = fakeIo([at(READINESS_FILES.editorCmd)], {}, { [at(EDITOR_BUILD_FILES.binariesDir)]: [] });
    expect(probeEditorBuildProgress({ root: ROOT, io }).state).toBe(BUILD_PROGRESS.EDITOR_PRESENT);
  });
});

// ───────────────────────────── remediation accuracy ──────────────────────────

describe('remediationStepsFor', () => {
  it('does NOT tell an operator to install a .NET SDK when this root already has compiled build tools', () => {
    const steps = remediationStepsFor({
      root: ROOT, versionString: '5.3.2', toolchain: bootstrappableToolchain, progress: partialProgress,
    });
    expect(steps.join('\n')).not.toMatch(/install a \.net/iu);
    expect(steps[0]).toContain('No .NET work is required');
    expect(steps[0]).toContain('PREBUILT_ASSEMBLIES');
  });

  it('demands a toolchain repair first when this root cannot compile its own build tools', () => {
    const steps = remediationStepsFor({
      root: ROOT, versionString: '5.0.3', toolchain: brokenToolchain, progress: partialProgress,
    });
    expect(steps[0]).toContain('CANNOT_COMPILE_TOOLCHAIN');
    expect(steps[0]).toContain('Repair');
  });

  it('names Setup.sh, GenerateProjectFiles.sh and the UnrealEditor target build, in that order', () => {
    const steps = remediationStepsFor({
      root: ROOT, versionString: '5.3.2', toolchain: bootstrappableToolchain, progress: partialProgress,
    });
    const text = steps.join('\n');
    expect(text.indexOf('Setup.sh')).toBeLessThan(text.indexOf('GenerateProjectFiles.sh'));
    expect(text.indexOf('GenerateProjectFiles.sh')).toBeLessThan(text.indexOf('make UnrealEditor'));
    expect(text).toContain(`${ROOT}/${READINESS_FILES.editorCmd}`);
  });

  // Cost, from counts. "just re-run the build" is the reading this prevents.
  it('states the remaining work from measured module counts, not from a guess', () => {
    const steps = remediationStepsFor({
      root: ROOT, versionString: '5.3.2', toolchain: bootstrappableToolchain, progress: partialProgress,
    });
    const text = steps.join('\n');
    expect(text).toContain('276 of the 455 modules');
    expect(text).toContain('resumed build');
    expect(text).toContain('hours of');
  });
});

// ───────────────────────────────── the record ────────────────────────────────

describe('buildPresentButUnbuiltBlocker', () => {
  const record = buildPresentButUnbuiltBlocker({
    task: 57,
    minorKey: '5.3',
    versionString: '5.3.2',
    advertisedBy: ['README.md "Unreal Engine 5.0-5.8"'],
    detection: { command: 'node scripts/qa/task52-certify-engine.mjs --engine-version 5.3', detectedAt: '2026-07-28T00:00:00.000Z' },
    rootReports: [{
      identity: identityOf('5.3.2'),
      toolchain: bootstrappableToolchain,
      readiness: readinessOf(ROOT),
      progress: partialProgress,
      observedState: { editorCmdPresent: false },
    }],
  });

  it('carries the blocker vocabulary Tasks 56 and 61 use', () => {
    expect(record.status).toBe(BLOCKER_STATUS);
    expect(record.severity).toBe(BLOCKER_SEVERITY);
    expect(record.blocksTasks).toEqual([62, 63, 64]);
  });

  // Filing a present-but-unbuilt root as an absent one sends an operator to
  // install an engine they already have 86GB of.
  it('is classified PRESENT_BUT_UNBUILT and never ABSENT', () => {
    expect(record.blockerClass).toBe(BLOCKER_CLASSES.PRESENT_BUT_UNBUILT);
    expect(record.blockerClass).not.toBe(BLOCKER_CLASSES.ABSENT);
    expect(record.subject.presentOnHost).toBe(true);
    expect(record.unreadiness.reason).toBe(CLASS_REASONS[BLOCKER_CLASSES.PRESENT_BUT_UNBUILT]);
    // The absent-class reason may only ever appear as prose explaining what this
    // record is NOT. It must never be the value of a reason or class field, which
    // is what an aggregator reads.
    const serialized = JSON.stringify(record);
    expect(serialized).not.toContain(`"reason":"${CLASS_REASONS[BLOCKER_CLASSES.ABSENT]}"`);
    expect(serialized).not.toContain(`"blockerClass":"${BLOCKER_CLASSES.ABSENT}"`);
    expect(serialized.split(CLASS_REASONS[BLOCKER_CLASSES.ABSENT])).toHaveLength(2);
    expect(record.blockerClassNote).toContain(CLASS_REASONS[BLOCKER_CLASSES.ABSENT]);
  });

  it('states that the classes must not be merged, inside the record itself', () => {
    expect(record.blockerClassNote).toContain(BLOCKER_CLASSES.ABSENT);
    expect(record.blockerClassNote).toContain('merges the two classes');
  });

  it('reports that no root can host a certification', () => {
    expect(record.unreadiness.rootsExamined).toBe(1);
    expect(record.unreadiness.rootsAbleToHostCertification).toBe(0);
  });

  // The single most important sentence in the document.
  it('draws no compatibility conclusion in either direction', () => {
    expect(record.unreadiness.notAPluginDefect).toContain('No plugin source was compiled');
    expect(record.unreadiness.notAPluginDefect).toContain('no packaging was attempted');
    expect(record.unreadiness.notAPluginDefect).toContain('is evidence that it does not');
  });

  it('refuses substitution by a neighbouring minor', () => {
    expect(record.unreadiness.notSubstitutable).toContain('never reused across minors');
  });

  // A blocker that can be read as a result is how an uncertified minor turns
  // green in somebody's summary. Enforced structurally, at every depth.
  it('carries no pass, ok, success, skip, verdict or result field at any depth', () => {
    const forbidden = /"(pass|passed|ok|success|succeeded|skip|skipped|verdict|result)"\s*:/iu;
    expect(forbidden.test(JSON.stringify(record))).toBe(false);
  });

  it('names an acceptance command that would falsify it', () => {
    expect(record.requiredOperatorInput.acceptanceCommand).toBe('node scripts/qa/task52-certify-engine.mjs --engine-version 5.3');
    expect(record.requiredOperatorInput.requiredFileAbsolutePaths).toEqual([`${ROOT}/${READINESS_FILES.editorCmd}`]);
    expect(record.requiredOperatorInput.handWrittenFileIsRefused).toContain('real completed');
  });

  it('demands the target receipt as well as the executable, so a stub cannot clear it', () => {
    expect(record.requiredOperatorInput.alsoRequired.join('\n')).toContain(EDITOR_BUILD_FILES.targetReceipt);
  });

  it('routes the remediation to the operator and says this lane did not attempt it', () => {
    expect(record.remediation.performedBy).toBe('operator');
    expect(record.remediation.notPerformedHere).toContain('Task 57');
    expect(record.remediation.summary).toContain(ROOT);
    expect(record.remediation.thenReRun.join('\n')).toContain('--engine-version 5.3');
  });

  it('keeps the advertised range intact rather than narrowing it', () => {
    expect(record.subject.advertisedRange).toBe('5.0-5.8');
    expect(record.consequence).toContain('may not be narrowed to hide it');
  });
});

// Three lanes, one vocabulary. Task 61 records absent minors, Task 56 records a
// present-but-unbuildable one and this module records present-but-unbuilt ones;
// Task 62 sums all three. If they ever spell "blocked" differently this fails
// here rather than in that aggregation.
describe('blocker vocabulary agreement across lanes', () => {
  it('uses the status and severity tokens Task 56 uses', async () => {
    const task56 = await import('../task-56/engine-readiness.mjs');
    expect(BLOCKER_STATUS).toBe(task56.BLOCKER_STATUS);
    expect(BLOCKER_SEVERITY).toBe(task56.BLOCKER_SEVERITY);
    expect(READINESS.NO_COMPILED_EDITOR).toBe(task56.READINESS.NO_COMPILED_EDITOR);
  });

  it('names the same absence reason Task 61 records, without adopting it', async () => {
    let task61: { BLOCKER_STATUS?: string; ABSENCE_REASON?: string } | null = null;
    try {
      task61 = await import('../task-61/external-blocker.mjs');
    } catch {
      task61 = null;
    }
    if (task61 === null) return;
    expect(BLOCKER_STATUS).toBe(task61.BLOCKER_STATUS);
    expect(CLASS_REASONS[BLOCKER_CLASSES.ABSENT]).toBe(task61.ABSENCE_REASON);
    expect(CLASS_REASONS[BLOCKER_CLASSES.PRESENT_BUT_UNBUILT]).not.toBe(task61.ABSENCE_REASON);
  });
});
