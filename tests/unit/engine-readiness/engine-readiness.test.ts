// tests/unit/engine-readiness/engine-readiness.test.ts
// Task 56 — proving the readiness judge separates the three ways a root fails.
//
// The failure this suite is written against is the one the live run actually hit:
// a packaging attempt died inside the ENGINE's C# toolchain and could easily have
// been filed as "the plugin does not compile on 5.0". Those two findings have
// different owners and different consequences — one invalidates prior evidence and
// returns to source, the other is an operator provisioning gap — so the judge must
// keep them apart on synthetic inputs, with no engine present.

import { describe, expect, it } from 'vitest';

import {
  BLOCKER_SEVERITY, BLOCKER_STATUS, READINESS, READINESS_FILES,
  buildUnbuiltRootBlocker, diagnoseBuildToolchain, judgeCertificationReadiness,
  judgeDotnetSupport, probeEngineRoot,
} from './engine-readiness.mjs';

/** A filesystem that exists only as a set of paths. */
function fakeIo(present: readonly string[], executable: readonly string[] = [], files: Record<string, string> = {}) {
  return {
    exists: (path: string) => present.includes(path),
    isExecutable: (path: string) => executable.includes(path),
    readFile: (path: string) => {
      if (files[path] === undefined) throw new Error(`ENOENT ${path}`);
      return files[path];
    },
    describe: () => null
  };
}

const ROOT = '/fake/UnrealEngine-5.0.3';
const at = (relative: string) => `${ROOT}/${relative}`;
const identityOf = (versionString: string) => ({
  usable: true, versionString, sources: { buildVersion: { sha256: 'a'.repeat(64) } }
});

describe('judgeDotnetSupport', () => {
  it('refuses a netcoreapp3.1 tool when only a modern SDK major is installed', () => {
    const verdict = judgeDotnetSupport({ targetFramework: 'netcoreapp3.1', sdkMajorVersions: [9] });
    expect(verdict.supported).toBe(false);
    expect(verdict.reason).toBe('SDK_TOO_NEW');
    expect(verdict.detail).toContain('NETSDK1151');
  });

  it('accepts it when a matching SDK major is installed', () => {
    expect(judgeDotnetSupport({ targetFramework: 'netcoreapp3.1', sdkMajorVersions: [3, 9] }).supported).toBe(true);
  });

  // An unknown is not a pass. A framework this function has no ruling for must
  // come back null so the caller reports UNDECIDABLE rather than inventing a
  // verdict in either direction.
  it('reports null rather than guessing for a framework it has no ruling for', () => {
    const verdict = judgeDotnetSupport({ targetFramework: 'net8.0', sdkMajorVersions: [] });
    expect(verdict.supported).toBeNull();
    expect(verdict.reason).toBe('UNKNOWN_TARGET_FRAMEWORK');
  });
});

describe('diagnoseBuildToolchain', () => {
  it('treats a tree with prebuilt UBT and UAT assemblies as bootstrappable regardless of host SDK', () => {
    const io = fakeIo([at(READINESS_FILES.runUat), at(READINESS_FILES.prebuiltUbt), at(READINESS_FILES.prebuiltUat)]);
    const toolchain = diagnoseBuildToolchain({ probe: probeEngineRoot({ root: ROOT, io }), sdkMajorVersions: [9] });
    expect(toolchain.bootstrappable).toBe(true);
    expect(toolchain.reason).toBe('PREBUILT_ASSEMBLIES');
  });

  // The observed 5.0.3 shape: RunUAT.sh is present, so Task 52's inventory calls
  // the root "buildable", and nothing can actually compile.
  it('refuses a root that has only the RunUAT script, no prebuilt tools and no bundled SDK', () => {
    const io = fakeIo(
      [at(READINESS_FILES.runUat), at(READINESS_FILES.uatProject)], [],
      { [at(READINESS_FILES.uatProject)]: '<TargetFramework>netcoreapp3.1</TargetFramework>' }
    );
    const probe = probeEngineRoot({ root: ROOT, io });
    expect(probe.hasRunUat).toBe(true);
    const toolchain = diagnoseBuildToolchain({ probe, sdkMajorVersions: [9] });
    expect(toolchain.bootstrappable).toBe(false);
    expect(toolchain.reason).toBe('CANNOT_COMPILE_TOOLCHAIN');
    expect(toolchain.missing).toContain(READINESS_FILES.bundledDotnet);
  });

  it('says UNDECIDABLE, not unbuildable, when the target framework cannot be read', () => {
    const io = fakeIo([at(READINESS_FILES.runUat)]);
    const toolchain = diagnoseBuildToolchain({ probe: probeEngineRoot({ root: ROOT, io }), sdkMajorVersions: [9] });
    expect(toolchain.bootstrappable).toBeNull();
    expect(toolchain.reason).toBe('UNDECIDABLE');
  });
});

describe('judgeCertificationReadiness', () => {
  const toolchainOk = { bootstrappable: true, reason: 'PREBUILT_ASSEMBLIES', detail: 'ok', dotnet: null };
  const toolchainBad = { bootstrappable: false, reason: 'CANNOT_COMPILE_TOOLCHAIN', detail: 'no SDK', dotnet: null };

  it('passes a root that has both an executable editor and a usable toolchain', () => {
    const io = fakeIo([at(READINESS_FILES.runUat), at(READINESS_FILES.editorCmd)], [at(READINESS_FILES.editorCmd)]);
    const verdict = judgeCertificationReadiness({
      identity: identityOf('5.0.3'), probe: probeEngineRoot({ root: ROOT, io }), toolchain: toolchainOk
    });
    expect(verdict.ready).toBe(true);
    expect(verdict.reason).toBe(READINESS.READY);
  });

  // THE LOAD-BEARING CASE. A missing editor binds even when the toolchain is
  // perfect, because repairing packaging would still leave nothing to launch.
  it('reports NO_COMPILED_EDITOR even when the build toolchain is fine', () => {
    const io = fakeIo([at(READINESS_FILES.runUat), at(READINESS_FILES.prebuiltUbt), at(READINESS_FILES.prebuiltUat)]);
    const verdict = judgeCertificationReadiness({
      identity: identityOf('5.0.3'), probe: probeEngineRoot({ root: ROOT, io }), toolchain: toolchainOk
    });
    expect(verdict.ready).toBe(false);
    expect(verdict.reason).toBe(READINESS.NO_COMPILED_EDITOR);
    expect(verdict.blockingConstraint).toBe(READINESS_FILES.editorCmd);
  });

  // Both broken: the editor still wins, and the detail must SAY the toolchain is
  // also broken rather than silently dropping it, because an operator who fixes
  // only the reported constraint would come back to a second refusal.
  it('names the editor as the binding constraint and still reports the broken toolchain', () => {
    const io = fakeIo([at(READINESS_FILES.runUat)]);
    const verdict = judgeCertificationReadiness({
      identity: identityOf('5.0.3'), probe: probeEngineRoot({ root: ROOT, io }), toolchain: toolchainBad
    });
    expect(verdict.reason).toBe(READINESS.NO_COMPILED_EDITOR);
    expect(verdict.detail).toContain('CANNOT_COMPILE_TOOLCHAIN');
  });

  it('reports NO_BUILD_TOOLCHAIN only when an editor exists but packaging cannot run', () => {
    const io = fakeIo([at(READINESS_FILES.runUat), at(READINESS_FILES.editorCmd)], [at(READINESS_FILES.editorCmd)]);
    const verdict = judgeCertificationReadiness({
      identity: identityOf('5.0.3'), probe: probeEngineRoot({ root: ROOT, io }), toolchain: toolchainBad
    });
    expect(verdict.reason).toBe(READINESS.NO_BUILD_TOOLCHAIN);
  });

  it('refuses to judge a root it could not identify', () => {
    const verdict = judgeCertificationReadiness({
      identity: { usable: false, detail: 'Build.version is absent' },
      probe: probeEngineRoot({ root: ROOT, io: fakeIo([]) }), toolchain: toolchainBad
    });
    expect(verdict.reason).toBe(READINESS.UNIDENTIFIED);
  });
});

describe('buildUnbuiltRootBlocker', () => {
  const record = buildUnbuiltRootBlocker({
    minorKey: '5.0',
    versionString: '5.0.3',
    detection: { command: 'node scripts/qa/evidence.mjs', detectedAt: '2026-07-28T00:00:00.000Z' },
    rootReports: [{
      identity: identityOf('5.0.3'),
      toolchain: { bootstrappable: false, reason: 'CANNOT_COMPILE_TOOLCHAIN', detail: 'no SDK' },
      readiness: {
        ready: false, reason: READINESS.NO_COMPILED_EDITOR, root: ROOT,
        detail: 'no editor', blockingConstraint: READINESS_FILES.editorCmd
      }
    }]
  });

  it('carries the shared blocker vocabulary', () => {
    expect(record.status).toBe(BLOCKER_STATUS);
    expect(record.severity).toBe(BLOCKER_SEVERITY);
    expect(record.blocksTasks).toEqual([62, 63, 64]);
  });

  // The record must state that the minor IS installed. Filing a present-but-unbuilt
  // root as an absent one would send an operator to install an engine they have.
  it('states that the minor is present on the host', () => {
    expect(record.subject.presentOnHost).toBe(true);
    expect(record.unreadiness.rootsAbleToHostCertification).toBe(0);
  });

  // The finding this whole task turns on.
  it('states explicitly that no plugin source was compiled', () => {
    expect(record.unreadiness.notAPluginDefect).toContain('No plugin source was compiled');
  });

  // A blocker that can be read as a result is how an uncertified minor turns green
  // in somebody's summary. Enforced structurally, at every depth, not by memory.
  it('carries no pass, ok, success, skip or verdict field at any depth', () => {
    const forbidden = /"(pass|passed|ok|success|succeeded|skip|skipped|verdict|result)"\s*:/iu;
    expect(forbidden.test(JSON.stringify(record))).toBe(false);
  });

  it('names an acceptance command that would falsify it', () => {
    expect(record.requiredOperatorInput.acceptanceCommand).toContain('certify-engine.mjs');
    expect(record.requiredOperatorInput.requiredFileAbsolutePaths).toEqual([`${ROOT}/${READINESS_FILES.editorCmd}`]);
  });
});

// Two lanes, one vocabulary. Task 61 records absent minors and Task 56 records
// present-but-unbuildable ones; Task 62 sums them. If the two ever spell "blocked"
// differently this fails here rather than in that aggregation.
describe('blocker vocabulary agreement with Task 61', () => {
  it('uses the same status and severity tokens Task 61 uses', async () => {
    let task61: { BLOCKER_STATUS?: string; BLOCKER_SEVERITY?: string } | null = null;
    try {
      task61 = await import('../engine-external-blocker/external-blocker.mjs');
    } catch {
      task61 = null;
    }
    if (task61 === null) return;
    expect(BLOCKER_STATUS).toBe(task61.BLOCKER_STATUS);
    expect(BLOCKER_SEVERITY).toBe(task61.BLOCKER_SEVERITY);
  });
});
