// tests/unit/task-61/external-blocker.test.ts
// Task 61 — proving the blocker records cannot be read as a pass.
//
// The plan has been bitten repeatedly by one shape: a gate that would fail being
// indistinguishable from one that passed. Task 54 wired `npm audit` into CI at
// position 15 and never executed it. A BLOCKED_EXTERNAL record is the same hazard
// in data form — the moment it carries a `pass`, a later aggregator sums the
// wrong column and four uncertifiable UE minors become a green support matrix.
//
// So this suite is written the way Task 50's validator suite is: THREE FORGERIES
// AND A POSITIVE CONTROL. A validator that refused everything would pass every
// rejection test below while gating nothing, which is why the first test proves a
// well-formed blocker is ACCEPTED. Without it the rejections prove only that the
// validator is broken in a convenient direction.
//
// EVERY TEST IS OFFLINE AND HOST-INDEPENDENT. The fleet below is a fake that
// MIRRORS this host — 5.0.3 at two roots, 5.3.2, 5.5.4, an unnamed root holding
// 5.7.4, and a `-preview-1` directory holding 5.8.0 — so the suite still means
// something on a CI runner with no /data, and so "5.1 is missing" is a property
// of the fixture rather than a property of the machine that happened to run it.

import { createHash } from 'node:crypto';
import { describe, expect, it } from 'vitest';

import { buildEngineInventory } from '../task-52/engine-inventory.mjs';
import {
  ADVERTISED_RANGE, buildMissingMinorBlocker, describeDetection, digestOfFiles,
} from './external-blocker.mjs';
import {
  AGGREGATE_STATUSES, BLOCKER_REJECTIONS, FORBIDDEN_PASS_KEYS, FORBIDDEN_SKIP_KEYS,
  aggregateExternalBlockers, syntheticHashReason, validateBlockerRecord,
} from './external-blocker-validator.mjs';

const BUILD = 'Engine/Build/Build.version';
const HEADER = 'Engine/Source/Runtime/Launch/Resources/Version.h';
const EDITOR = 'Engine/Binaries/Linux/UnrealEditor-Cmd';
const RUNUAT = 'Engine/Build/BatchFiles/RunUAT.sh';

const sha256 = (text: string) => createHash('sha256').update(text).digest('hex');

/**
 * The fleet really installed on the host this task ran against, reproduced as
 * data. Two roots carry names that say nothing or the wrong thing about their
 * contents, because that is the whole reason identity comes from Build.version.
 */
const FLEET: Record<string, { version: [number, number, number]; editor: boolean; describe: string }> = {
  '/data/UnrealEngine': { version: [5, 7, 4], editor: true, describe: '5.7.0-preview-1-4009-g0bcfaffa52e9' },
  '/data/UnrealEngine-5.0.3': { version: [5, 0, 3], editor: false, describe: '5.0.3-release' },
  '/data/UnrealEngine-5.0-branch': { version: [5, 0, 3], editor: false, describe: '5.0.0-preview-2-1244-gf7e9b48a637e' },
  '/data/UnrealEngine-5.3.2': { version: [5, 3, 2], editor: false, describe: '5.3.2-release' },
  '/data/UnrealEngine-5.5.4': { version: [5, 5, 4], editor: false, describe: '5.5.4-release' },
  '/data/UnrealEngine-5.8.0-preview-1': { version: [5, 8, 0], editor: true, describe: '5.8.0-release' },
};

function contentsOf(root: string, relative: string): string | null {
  const spec = FLEET[root];
  if (spec === undefined) return null;
  const [major, minor, patch] = spec.version;
  if (relative === BUILD) {
    return JSON.stringify({
      MajorVersion: major, MinorVersion: minor, PatchVersion: patch,
      Changelist: 0, CompatibleChangelist: 0, IsLicenseeVersion: 0, IsPromotedBuild: 0, BranchName: 'UE5',
    });
  }
  if (relative === HEADER) {
    return `#define ENGINE_MAJOR_VERSION\t${major}\n#define ENGINE_MINOR_VERSION\t${minor}\n#define ENGINE_PATCH_VERSION\t${patch}\n`;
  }
  if (relative === RUNUAT) return '#!/bin/sh\n';
  if (relative === EDITOR) return spec.editor ? '' : null;
  return null;
}

/** Which fake root owns an absolute path — longest prefix, so `-5.0.3` never swallows `-5.0-branch`. */
const ownerOf = (path: string) => Object.keys(FLEET)
  .filter((root) => path.startsWith(`${root}/`))
  .sort((a, b) => b.length - a.length)[0];

function lookup(path: string): string | null {
  const root = ownerOf(path);
  if (root === undefined) return null;
  return contentsOf(root, path.slice(root.length + 1));
}

const fleetIo = {
  readFile: (path: string) => {
    const found = lookup(path);
    if (found === null) throw new Error(`ENOENT ${path}`);
    return found;
  },
  exists: (path: string) => lookup(path) !== null,
  isExecutable: (path: string) => lookup(path) !== null && path.endsWith(EDITOR),
  describe: (root: string) => FLEET[root]?.describe ?? null,
};

/** The detector's own source, as the record records it: paths plus real-looking digests. */
const DETECTOR_FILES = [
  { path: 'tests/unit/task-61/external-blocker.mjs', sha256: sha256('external-blocker.mjs@fixture') },
  { path: 'tests/unit/task-61/external-blocker-validator.mjs', sha256: sha256('external-blocker-validator.mjs@fixture') },
  { path: 'scripts/qa/task61-detect-missing-engines.mjs', sha256: sha256('task61-detect-missing-engines.mjs@fixture') },
];

/**
 * Filesystem access for the validator, answering ONLY about the fake fleet and
 * the fake detector tree. Injected rather than stubbed globally so a test that
 * forgets to inject reads the real machine and fails loudly instead of quietly
 * proving nothing.
 */
const blockerIo = {
  hashOf: (path: string) => {
    const recorded = DETECTOR_FILES.find((entry) => entry.path === path);
    if (recorded !== undefined) return recorded.sha256;
    const found = lookup(path);
    return found === null ? null : sha256(found);
  },
  minorAt: (buildVersionFile: string) => {
    const root = ownerOf(buildVersionFile);
    if (root === undefined || !buildVersionFile.endsWith(BUILD)) return null;
    const [major, minor] = FLEET[root].version;
    return `${major}.${minor}`;
  },
  exists: (path: string) => lookup(path) !== null,
};

const inventory = buildEngineInventory({ roots: Object.keys(FLEET), io: fleetIo });

const detection = describeDetection({
  command: 'node scripts/qa/task61-detect-missing-engines.mjs --search-dir /data',
  commandExitCode: 3,
  reproducibleShellCommand: 'grep -H -o \'"\\(Major\\|Minor\\|Patch\\)Version": *[0-9]*\' /data/*/Engine/Build/Build.version',
  searchDirs: ['/data'],
  detectorTree: { files: DETECTOR_FILES, sourceDigest: digestOfFiles(DETECTOR_FILES) },
  inventory,
  detectedAt: '2026-07-28T13:03:53.691Z',
});

const MISSING_MINORS = ['5.1', '5.2', '5.4', '5.6'];

const buildRecord = (minorKey: string) => buildMissingMinorBlocker({
  minorKey,
  inventory,
  detection,
  projectRelativeDetector: 'scripts/qa/task61-detect-missing-engines.mjs',
  advertisedBy: ['plugins/McpAutomationBridge/McpAutomationBridge.uplugin: "UE 5.0-5.8 Preview"'],
  observedTagExample: '5.3.2-release',
});

const validate = (record: unknown, extra: Record<string, unknown> = {}) => validateBlockerRecord(record, {
  io: blockerIo as never,
  now: () => new Date('2026-07-28T13:10:00.000Z'),
  ...extra,
});

/** Deep-clone so a mutation in one test cannot leak into the next. */
const clone = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T;

/**
 * Collect every key at every depth. Written here rather than reused from the
 * validator on purpose: asking the validator whether the builder is clean would
 * be the parser comparing itself to itself.
 */
function allKeys(node: unknown, found: string[] = []): string[] {
  if (Array.isArray(node)) {
    for (const entry of node) allKeys(entry, found);
    return found;
  }
  if (node !== null && typeof node === 'object') {
    for (const [key, value] of Object.entries(node)) {
      found.push(key.toLowerCase().replace(/[^a-z0-9]/gu, ''));
      allKeys(value, found);
    }
  }
  return found;
}

const codesOf = (result: { rejections: readonly { code: string }[] }) => result.rejections.map((entry) => entry.code);

describe('task 61 fixture fidelity', () => {
  it('Given the fake fleet, When the inventory runs, Then exactly 5.1, 5.2, 5.4 and 5.6 are missing', () => {
    expect(inventory.missing).toEqual(MISSING_MINORS);
  });

  it('Given a root whose NAME carries no version, When it is identified, Then it is filed by what Build.version contains', () => {
    const unnamed = inventory.available.find((entry: { minorKey: string }) => entry.minorKey === '5.7') as Record<string, unknown>;
    expect(unnamed.preferredRoot).toBe('/data/UnrealEngine');
    expect(unnamed.versionString).toBe('5.7.4');
  });

  it('Given a directory named -preview-1 that contains a release tag, When identified, Then the folder label is contradicted rather than believed', () => {
    const previewRoot = inventory.folderNameContradictions
      .find((entry: { root: string }) => entry.root === '/data/UnrealEngine-5.8.0-preview-1') as Record<string, unknown>;
    expect(previewRoot.kind).toBe('CHANNEL_LABEL');
  });
});

// ─────────────────────── THE POSITIVE CONTROL, FIRST ─────────────────────────
// Without this, every rejection below is also satisfied by a validator that
// refuses its own input unconditionally, and the suite gates nothing.
describe('task 61 positive control: a well-formed blocker is ACCEPTED', () => {
  for (const minorKey of MISSING_MINORS) {
    it(`Given a well-formed BLOCKED_EXTERNAL record for ${minorKey}, When validated, Then it is ACCEPTED with no rejection`, () => {
      const result = validate(buildRecord(minorKey));
      expect(result.rejections).toEqual([]);
      expect(result.outcome).toBe('ACCEPTED');
    });
  }

  it('Given the accepted record, When its checks are counted, Then digests and cited paths were really re-derived rather than skipped', () => {
    const result = validate(buildRecord('5.1'));
    expect(result.checked.hashes).toBeGreaterThan(DETECTOR_FILES.length);
    expect(result.checked.citedPaths).toBeGreaterThan(0);
  });

  it('Given four well-formed records, When aggregated, Then the set is ACCEPTED and every minor is blocked', () => {
    const aggregate = aggregateExternalBlockers({
      records: MISSING_MINORS.map(buildRecord),
      detectedMissingMinors: MISSING_MINORS,
      io: blockerIo as never,
      now: () => new Date('2026-07-28T13:10:00.000Z'),
    });
    expect(aggregate.outcome).toBe('ACCEPTED');
    expect(aggregate.status).toBe(AGGREGATE_STATUSES.BLOCKED_EXTERNAL);
    expect(aggregate.blockedMinors).toEqual(MISSING_MINORS);
  });
});

// ────────────────────────── FORGERY 1: A PASS FIELD ──────────────────────────
describe('task 61 refuses a record that could be read as success', () => {
  it('Given the builder output, When every key at every depth is collected, Then none is a pass or skip field', () => {
    const keys = new Set(allKeys(buildRecord('5.1')));
    const offenders = [...FORBIDDEN_PASS_KEYS, ...FORBIDDEN_SKIP_KEYS].filter((key) => keys.has(key));
    expect(offenders).toEqual([]);
  });

  it('Given a top-level `pass: true`, When validated, Then the record is REFUSED', () => {
    const result = validate({ ...buildRecord('5.1'), pass: true });
    expect(result.outcome).toBe('REJECTED');
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.PASS_FIELD);
  });

  it.each([
    ['ok', true],
    ['success', true],
    ['verdict', 'PASSED'],
    ['certified', true],
    ['isOk', true],
    ['is_ok', true],
  ])('Given a `%s` field, When validated, Then the record is REFUSED as a pass field', (key, value) => {
    const result = validate({ ...buildRecord('5.2'), [key]: value });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.PASS_FIELD);
  });

  it('Given a pass field buried deep inside detection, When validated, Then it is still found', () => {
    const record = clone(buildRecord('5.4')) as Record<string, any>;
    record.detection.hostEngineTree.roots[0].verified = true;
    const result = validate(record);
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.PASS_FIELD);
  });

  it('Given a scalar "success" anywhere in the record, When validated, Then the value alone is refused', () => {
    const record = clone(buildRecord('5.6')) as Record<string, any>;
    record.remediation.steps.push('success');
    const result = validate(record);
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.PASS_FIELD);
  });

  it.each(['skipped', 'waived', 'optional', 'nonBlocking'])('Given a `%s` field, When validated, Then the record is REFUSED as a skip', (key) => {
    const result = validate({ ...buildRecord('5.1'), [key]: true });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.SKIP_FIELD);
  });

  it('Given a softened severity, When validated, Then the record is REFUSED for not blocking', () => {
    const result = validate({ ...buildRecord('5.1'), severity: 'warning' });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.NOT_BLOCKING);
  });

  it('Given a status other than BLOCKED_EXTERNAL, When validated, Then the record is REFUSED', () => {
    const result = validate({ ...buildRecord('5.1'), status: 'RESOLVED' });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.WRONG_STATUS);
  });

  it('Given a detector that exited 0, When validated, Then the record is REFUSED: exit 0 describes a host where nothing was missing', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection.commandExitCode = 0;
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NOT_BLOCKING);
  });

  it('Given a narrowed advertised range, When validated, Then the record is REFUSED for hiding the gap instead of reporting it', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.subject.advertisedRange = '5.0-5.0';
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.MALFORMED);
  });
});

// ──────────────────────── FORGERY 2: A SYNTHETIC HASH ────────────────────────
describe('task 61 refuses a fabricated hash', () => {
  it.each([
    ['a repeated character', 'a'.repeat(64)],
    ['the sha256 of an empty string', sha256('')],
    ['the sha256 of a placeholder word', sha256('todo')],
    ['uppercase hex', sha256('anything').toUpperCase()],
    ['a truncated digest', sha256('anything').slice(0, 40)],
    ['a non-hex string', 'not-a-hash'],
  ])('Given %s as the detector digest, When validated, Then it is refused as synthetic', (_label, digest) => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection.detectorTree.sourceDigest = digest;
    // skipFilesystem proves the shape check stands ALONE: a record whose hashes
    // nobody can re-derive is refused even where the files cannot be reached.
    const result = validate(record, { skipFilesystem: true });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.SYNTHETIC_HASH);
  });

  it('Given a PLAUSIBLE invented digest, When validated, Then recomputation catches what shape alone cannot', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    const invented = sha256('a digest that looks entirely real');
    record.detection.detectorTree.sourceDigest = invented;
    expect(syntheticHashReason(invented)).toBeNull();
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.HASH_MISMATCH);
  });

  it('Given a fabricated per-file hash, When validated, Then the file list itself is refused', () => {
    const record = clone(buildRecord('5.2')) as Record<string, any>;
    record.detection.detectorTree.files[0].sha256 = sha256('a source file that was never read');
    record.detection.detectorTree.sourceDigest = digestOfFiles(record.detection.detectorTree.files);
    // The digest is honestly recomputed from the tampered list, so ONLY the
    // filesystem can catch this one. That is why recomputation is not optional.
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.HASH_MISMATCH);
  });

  it('Given a fabricated host engine-tree digest, When validated, Then the absence proof is refused', () => {
    const record = clone(buildRecord('5.4')) as Record<string, any>;
    record.detection.hostEngineTree.digest = sha256('a host that was never scanned');
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.HASH_MISMATCH);
  });

  it('Given a Build.version hash that no longer matches the bytes, When validated, Then the record is refused as stale', () => {
    const record = clone(buildRecord('5.6')) as Record<string, any>;
    record.detection.hostEngineTree.roots[0].buildVersionSha256 = sha256('{"MajorVersion":5,"MinorVersion":1,"PatchVersion":1}');
    record.detection.hostEngineTree.digest = digestOfFiles(
      record.detection.hostEngineTree.roots.map((entry: any) => ({ path: entry.buildVersionFile, sha256: entry.buildVersionSha256 })),
    );
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.HASH_MISMATCH);
  });

  it('Given an empty detector tree, When validated, Then a digest that summarises nothing is refused', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection.detectorTree.files = [];
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_TREE_HASH);
  });
});

// ─────────────────────── FORGERY 3: A WRONG-MINOR PATH ───────────────────────
describe('task 61 refuses a record pointing at the wrong minor', () => {
  it('Given a root filed under a minor it does not contain, When validated, Then the record is refused', () => {
    const record = clone(buildRecord('5.4')) as Record<string, any>;
    const neighbour = record.detection.hostEngineTree.roots
      .find((entry: any) => entry.root === '/data/UnrealEngine-5.3.2');
    neighbour.minorKey = '5.4';
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.WRONG_MINOR_PATH);
  });

  it('Given remediation pointing at another minor\u2019s existing root, When validated, Then the substitution is refused', () => {
    const record = clone(buildRecord('5.4')) as Record<string, any>;
    record.requiredOperatorInput.engineRoot = '/data/UnrealEngine-5.3.2';
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.WRONG_MINOR_PATH);
  });

  it.each([
    ['5.1', '/data/UnrealEngine-5.0.3'],
    ['5.2', '/data/UnrealEngine-5.3.2'],
    ['5.4', '/data/UnrealEngine-5.3.2'],
    ['5.6', '/data/UnrealEngine-5.5.4'],
    ['5.6', '/data/UnrealEngine'],
  ])('Given %s remediated by the neighbouring root %s, When validated, Then it is refused', (minorKey, neighbour) => {
    const record = clone(buildRecord(minorKey)) as Record<string, any>;
    record.requiredOperatorInput.engineRoot = neighbour;
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.WRONG_MINOR_PATH);
  });

  it('Given required Build.version content for the wrong minor, When validated, Then the operator input is refused', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.requiredOperatorInput.requiredFields.MinorVersion = 0;
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.WRONG_MINOR_INPUT);
  });

  it('Given a pinned PatchVersion, When validated, Then the invented release is refused', () => {
    const record = clone(buildRecord('5.2')) as Record<string, any>;
    record.requiredOperatorInput.requiredFields.PatchVersion = 1;
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.WRONG_MINOR_INPUT);
  });

  it('Given a record whose subject minor is actually installed, When validated, Then the blocker is refuted rather than published', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.subject.minorKey = '5.3';
    record.requiredOperatorInput.requiredFields.MinorVersion = 3;
    const codes = codesOf(validate(record));
    expect(codes).toContain(BLOCKER_REJECTIONS.ROOT_NOW_PRESENT);
  });

  it('Given a record that names a root containing its own subject minor, When validated, Then it is self-refuting', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.absence.rootsContainingSubjectMinor = ['/data/UnrealEngine-5.1.1'];
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.ROOT_NOW_PRESENT);
  });

  it('Given the built record, When its non-substitutes are read, Then every installed minor is named and the subject is not', () => {
    const record = buildRecord('5.4') as Record<string, any>;
    const named = record.absence.notSubstitutableBy.map((entry: any) => entry.minorKey);
    expect(named).toEqual(['5.0', '5.3', '5.5', '5.7', '5.8']);
    expect(named).not.toContain('5.4');
  });
});

// ───────────────────── detection command, time and inputs ────────────────────
describe('task 61 requires a re-runnable detection and an executable remediation', () => {
  it.each([
    ['command', BLOCKER_REJECTIONS.NO_DETECTION_COMMAND],
    ['reproducibleShellCommand', BLOCKER_REJECTIONS.NO_DETECTION_COMMAND],
  ])('Given a record with no detection %s, When validated, Then it is refused', (field, code) => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection[field] = '';
    expect(codesOf(validate(record))).toContain(code);
  });

  it.each([
    ['a missing time', ''],
    ['a date without a time', '2026-07-28'],
    ['a non-ISO string', 'last Tuesday'],
  ])('Given %s, When validated, Then the record is refused for being unre-checkable', (_label, value) => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection.detectedAt = value;
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_DETECTION_TIME);
  });

  it('Given a detection time in the future, When validated, Then it is refused as written rather than observed', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.detection.detectedAt = '2027-01-01T00:00:00.000Z';
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_DETECTION_TIME);
  });

  it('Given a record with no acceptance command, When validated, Then nobody could tell when the blocker clears', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.requiredOperatorInput.acceptanceCommand = '';
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT);
  });

  it('Given a record demanding only Build.version, When validated, Then it is refused: a hand-written file would parse', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.requiredOperatorInput.alsoRequired = [];
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT);
  });

  it('Given the built remediation, When read, Then it names the engine root, the version file and the acceptance command', () => {
    const input = (buildRecord('5.6') as Record<string, any>).requiredOperatorInput;
    expect(input.engineRoot).toBe('/data/UnrealEngine-5.6');
    expect(input.requiredFile).toBe(BUILD);
    expect(input.requiredFields).toEqual({ MajorVersion: 5, MinorVersion: 6, PatchVersion: null });
    expect(input.acceptanceCommand).toContain('--minor 5.6');
    expect(input.alsoRequired.join(' ')).toContain(HEADER);
    expect(input.alsoRequired.join(' ')).toContain(EDITOR);
  });

  it('Given a record with no remediation steps, When validated, Then a vague blocker is refused', () => {
    const record = clone(buildRecord('5.1')) as Record<string, any>;
    record.remediation.steps = [];
    expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.NO_REMEDIATION);
  });

  it('Given an unknown top-level key, When validated, Then the typo that would disable a check is refused', () => {
    const result = validate({ ...buildRecord('5.1'), detectio: {} });
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.UNKNOWN_FIELD);
  });

  it.each(['detection', 'requiredOperatorInput', 'remediation', 'absence', 'subject', 'consequence'])(
    'Given a record missing %s, When validated, Then it is refused',
    (section) => {
      const record = clone(buildRecord('5.1')) as Record<string, any>;
      delete record[section];
      expect(codesOf(validate(record))).toContain(BLOCKER_REJECTIONS.MISSING_FIELD);
    },
  );
});

// ─────────────────────────── the aggregate treatment ─────────────────────────
describe('task 61 aggregate treats every record as a blocker', () => {
  const aggregate = (records: unknown[], missingMinors: string[] = MISSING_MINORS) => aggregateExternalBlockers({
    records,
    detectedMissingMinors: missingMinors,
    io: blockerIo as never,
    now: () => new Date('2026-07-28T13:10:00.000Z'),
  });

  it('Given the aggregate status enum, When read, Then it contains no member meaning pass or skip', () => {
    const statuses = Object.values(AGGREGATE_STATUSES).map((value) => String(value).toLowerCase());
    for (const status of statuses) {
      expect(FORBIDDEN_PASS_KEYS).not.toContain(status);
      expect(FORBIDDEN_SKIP_KEYS).not.toContain(status);
    }
    expect(statuses).toEqual(['blocked_external', 'no_blockers_recorded']);
  });

  it('Given four valid records, When aggregated, Then nothing is counted as skipped or passed', () => {
    const result = aggregate(MISSING_MINORS.map(buildRecord));
    expect(result.blockerCount).toBe(4);
    expect(result.skippedCount).toBe(0);
    expect(result.passedCount).toBe(0);
  });

  it('Given an INVALID record in the set, When aggregated, Then it still blocks while the document is refused', () => {
    const records = MISSING_MINORS.map(buildRecord);
    (records[0] as Record<string, any>).pass = true;
    const result = aggregate(records);
    expect(result.outcome).toBe('REJECTED');
    expect(result.status).toBe(AGGREGATE_STATUSES.BLOCKED_EXTERNAL);
    expect(result.blockerCount).toBe(4);
    expect(result.skippedCount).toBe(0);
    expect(result.passedCount).toBe(0);
  });

  it('Given a detected missing minor with no record, When aggregated, Then the unrecorded gap is refused', () => {
    const result = aggregate(['5.1', '5.2', '5.4'].map(buildRecord));
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.UNRECORDED_MISSING_MINOR);
  });

  it('Given two records for one minor, When aggregated, Then the weaker-of-two ambiguity is refused', () => {
    const result = aggregate([...MISSING_MINORS.map(buildRecord), buildRecord('5.1')]);
    expect(codesOf(result)).toContain(BLOCKER_REJECTIONS.DUPLICATE_RECORD);
  });

  it('Given no records at all, When aggregated, Then the status is NOT a pass', () => {
    const result = aggregate([], []);
    expect(result.status).toBe(AGGREGATE_STATUSES.NO_BLOCKERS_RECORDED);
    expect(result.passedCount).toBe(0);
  });

  it('Given every forgery this suite knows, When aggregated, Then no input yields a passed or skipped count', () => {
    const forgeries: unknown[][] = [
      [{ ...buildRecord('5.1'), pass: true }],
      [{ ...buildRecord('5.2'), skipped: true }],
      [{ ...buildRecord('5.4'), severity: 'info' }],
      [{ ...buildRecord('5.6'), status: 'RESOLVED' }],
      [null],
      [{}],
    ];
    for (const records of forgeries) {
      const result = aggregate(records, []);
      expect(result.passedCount).toBe(0);
      expect(result.skippedCount).toBe(0);
      expect(result.blockerCount).toBe(records.length);
      expect(result.status).toBe(AGGREGATE_STATUSES.BLOCKED_EXTERNAL);
    }
  });

  it('Given the advertised range, When a record is built, Then it is carried verbatim so the blocked claim is the claim made', () => {
    expect((buildRecord('5.1') as Record<string, any>).subject.advertisedRange).toBe(ADVERTISED_RANGE);
    expect(ADVERTISED_RANGE).toBe('5.0-5.8');
  });
});
