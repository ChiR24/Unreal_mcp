// tests/unit/evidence-oracles/oracle-selftest.test.ts
// Task 50 — CAN THE ORACLES CATCH A LIE?
//
// Every case here works on REAL bytes in a real owned temp directory, never on a
// mocked filesystem. A stub `fs` would let an oracle pass against a shape the
// filesystem never produces — Task 46's finding F3 had a live fix and a green
// test and was still broken, because the fixture was hand-authored in a shape the
// plugin never sends.
//
// The suite is organised around the one question that matters: hand each oracle a
// response that CLAIMS success while the world did not change, and watch it fail.
// Then, because a suite of only-negative assertions passes identically against an
// oracle that reports "absent" for everything, every negative here is paired with
// a POSITIVE CONTROL that watches the same mechanism read the other way.

import { mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import {
  INDEPENDENCE,
  UE_PACKAGE_MAGIC,
  decodeImageHeader,
  findNameEntry,
  observation,
  observeAssetPackage,
  observeEditorLog,
  observeIniSetting,
  observeLevelActor,
  observeListener,
  observeProcess,
  observeRenderOutput,
  observeTree,
  packageBasePath,
  crossTransportObservation,
} from './state-oracles.mjs';
import {
  VERDICTS,
  assertSetupObserved,
  auditPositiveControls,
  forgedSuccessClaim,
  judgeClaim,
  judgeCleanup,
} from './oracle-judgement.mjs';

/** Owned scratch. Under /tmp/opencode as the task requires, in a suite-specific child. */
const ROOT = join('/tmp/opencode/task-50', `selftest-${process.pid}`);
const PROJECT = join(ROOT, 'Project');
const CONTENT = join(PROJECT, 'Content');

/** Write a byte-accurate UE package: the real magic, then a real FName entry. */
function writePackage(gameRelative: string, names: readonly string[], extension = '.uasset'): string {
  const file = join(CONTENT, `${gameRelative}${extension}`);
  mkdirSync(join(file, '..'), { recursive: true });
  const chunks: Buffer[] = [UE_PACKAGE_MAGIC, Buffer.from([0xf7, 0xff, 0xff, 0xff])];
  for (const name of names) {
    const length = Buffer.alloc(4);
    length.writeInt32LE(name.length + 1);
    chunks.push(length, Buffer.from(`${name}\0`, 'latin1'));
  }
  writeFileSync(file, Buffer.concat(chunks), { mode: 0o600 });
  return file;
}

function png(width: number, height: number): Buffer {
  const head = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  const ihdrLength = Buffer.alloc(4);
  ihdrLength.writeUInt32BE(13);
  const body = Buffer.alloc(13);
  body.writeUInt32BE(width, 0);
  body.writeUInt32BE(height, 4);
  body[8] = 8;
  return Buffer.concat([head, ihdrLength, Buffer.from('IHDR', 'latin1'), body]);
}

beforeAll(() => { mkdirSync(CONTENT, { recursive: true }); });
afterAll(() => { rmSync(ROOT, { recursive: true, force: true }); });

describe('Task 50 — path mapping is a pure transform, never a question to the subject', () => {
  it('maps an Unreal object path to the package that would hold it', () => {
    expect(packageBasePath('/proj', '/Game/MCPTest/run/M_Thing')).toBe('/proj/Content/MCPTest/run/M_Thing');
  });

  it('strips the object suffix so /Game/A/B.B resolves to the B package', () => {
    expect(packageBasePath('/proj', '/Game/A/B.B')).toBe('/proj/Content/A/B');
  });

  it('refuses a path outside /Game rather than inventing a location for it', () => {
    expect(packageBasePath('/proj', '/Engine/Foo')).toBeNull();
  });
});

describe('Task 50 — the asset oracle reads packages, not responses', () => {
  it('POSITIVE CONTROL: reads a real package present, with its digest', () => {
    writePackage('MCPTest/ctl/M_Present', ['M_Present']);
    const seen = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Present', expectName: 'M_Present' });
    expect(seen.present).toBe(true);
    expect(seen.conclusive).toBe(true);
    expect(seen.independence).toBe(INDEPENDENCE.OUT_OF_BAND);
    expect(seen.digest).toMatch(/^[0-9a-f]{64}$/u);
  });

  it('reads an absent package absent', () => {
    const seen = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_NeverMade' });
    expect(seen.present).toBe(false);
    expect(seen.conclusive).toBe(true);
  });

  it('a file that exists but is NOT a UE package reads absent, with the reason', () => {
    const file = join(CONTENT, 'MCPTest/ctl/M_Corrupt.uasset');
    mkdirSync(join(file, '..'), { recursive: true });
    writeFileSync(file, Buffer.from('this is not a package'), { mode: 0o600 });
    const seen = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Corrupt' });
    expect(seen.present).toBe(false);
    expect(seen.detail.magicOk).toBe(false);
    expect(String(seen.detail.note)).toContain('does not carry the UE package magic');
  });

  it('demands a framed FName entry, so M_Foo is not found inside M_FooBar', () => {
    writePackage('MCPTest/ctl/M_FooBar', ['M_FooBar']);
    const decoy = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_FooBar', expectName: 'M_Foo' });
    expect(decoy.present).toBe(false);
    const real = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_FooBar', expectName: 'M_FooBar' });
    expect(real.present).toBe(true);
  });

  it('reports NOT_UNDER_GAME as inconclusive, never as absent', () => {
    const seen = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Engine/Transient' });
    expect(seen.present).toBeNull();
    expect(seen.conclusive).toBe(false);
  });

  it('findNameEntry rejects an unframed substring match', () => {
    const buffer = Buffer.from('....Actor_Decoy\0', 'latin1');
    expect(findNameEntry(buffer, 'Actor_Decoy')).toBe(-1);
  });
});

describe('Task 50 — FORGED SUCCESS is the headline detection', () => {
  const target = '/Game/MCPTest/ctl/M_Forged';

  it('FAILS a response that claims it created something when nothing was created', () => {
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: target });
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: target });
    const verdict = judgeClaim({ claim: forgedSuccessClaim({ target }), before, after });
    expect(verdict.verdict).toBe(VERDICTS.FORGED_SUCCESS);
    expect(verdict.pass).toBe(false);
    expect(verdict.reason).toContain('still absent');
  });

  it('POSITIVE CONTROL: the identical judgement PASSES when the asset really appears', () => {
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Real' });
    writePackage('MCPTest/ctl/M_Real', ['M_Real']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Real', expectName: 'M_Real' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.PROVEN);
    expect(verdict.pass).toBe(true);
  });

  it('FAILS a claimed modification whose bytes are byte-identical', () => {
    writePackage('MCPTest/ctl/M_Unmoved', ['M_Unmoved']);
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Unmoved' });
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Unmoved' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'modified' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.FORGED_MODIFICATION);
  });

  it('POSITIVE CONTROL: a real byte change is accepted as a modification', () => {
    writePackage('MCPTest/ctl/M_Moved', ['M_Moved']);
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Moved' });
    writePackage('MCPTest/ctl/M_Moved', ['M_Moved', 'ExtraProperty']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Moved' });
    expect(before.digest).not.toBe(after.digest);
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'modified' }, before, after }).verdict).toBe(VERDICTS.PROVEN);
  });

  it('FAILS a claimed deletion whose target survives', () => {
    writePackage('MCPTest/ctl/M_Survivor', ['M_Survivor']);
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Survivor' });
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Survivor' });
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'deleted' }, before, after }).verdict).toBe(VERDICTS.FORGED_DELETION);
  });

  it('POSITIVE CONTROL: a real deletion is accepted', () => {
    const file = writePackage('MCPTest/ctl/M_Doomed', ['M_Doomed']);
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Doomed' });
    rmSync(file);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Doomed' });
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'deleted' }, before, after }).verdict).toBe(VERDICTS.PROVEN);
  });

  it('FAILS a REFUSED call that mutated anyway — the silent partial write', () => {
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Sneaky' });
    writePackage('MCPTest/ctl/M_Sneaky', ['M_Sneaky']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Sneaky' });
    const verdict = judgeClaim({ claim: { outcome: 'error', effect: 'created' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.UNCLAIMED_MUTATION);
  });

  it('POSITIVE CONTROL: a refusal that really changed nothing is PROVEN', () => {
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Refused' });
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Refused' });
    const verdict = judgeClaim({ claim: { outcome: 'error', effect: 'created' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.PROVEN);
    expect(verdict.pass).toBe(true);
  });
});

describe('Task 50 — the rules that stop a green run proving nothing', () => {
  it('PRE-STATE: a fixture that already existed cannot be attributed to this call', () => {
    writePackage('MCPTest/ctl/M_Leftover', ['M_Leftover']);
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Leftover' });
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Leftover' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.PRE_STATE_CONTAMINATED);
    expect(verdict.reason).toContain('leftover');
  });

  it('PRE-STATE: a mutation with NO pre-state reading is UNPROVEN, not passed', () => {
    writePackage('MCPTest/ctl/M_NoPre', ['M_NoPre']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_NoPre' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before: null, after });
    expect(verdict.verdict).toBe(VERDICTS.UNPROVEN);
  });

  it('AN ORACLE THAT COULD NOT LOOK never passes and is never read as "absent"', () => {
    const blind = observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'x', present: null, conclusive: false });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before: null, after: blind });
    expect(verdict.verdict).toBe(VERDICTS.UNPROVEN);
    expect(verdict.reason).toContain('not an oracle that saw nothing');
  });

  it('VACUOUS SETUP: a claim whose setup failed is refused before it is judged', () => {
    writePackage('MCPTest/ctl/M_Vacuous', ['M_Vacuous']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Vacuous' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before: null, after, setupOk: false });
    expect(verdict.verdict).toBe(VERDICTS.VACUOUS);
  });

  it('assertSetupObserved refuses a setup nobody could observe', () => {
    const blind = observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'x', present: null, conclusive: false });
    expect(assertSetupObserved({ label: 'seed', observation: blind }).ok).toBe(false);
    const real = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Present' });
    expect(assertSetupObserved({ label: 'seed', observation: real }).ok).toBe(true);
  });

  it('INDEPENDENCE: a mutation corroborated only cross-transport is NOT_INDEPENDENT', () => {
    const before = crossTransportObservation({ target: '/Game/x', present: false, transport: 'native' });
    const after = crossTransportObservation({ target: '/Game/x', present: true, transport: 'native' });
    const verdict = judgeClaim({ claim: { outcome: 'success', effect: 'created' }, before, after });
    expect(verdict.verdict).toBe(VERDICTS.NOT_INDEPENDENT);
    expect(verdict.reason).toContain('shares every failure mode');
  });

  it('INDEPENDENCE: the same cross-transport reading is fine as CORROBORATION beside an out-of-band one', () => {
    const before = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Both' });
    writePackage('MCPTest/ctl/M_Both', ['M_Both']);
    const after = observeAssetPackage({ projectRoot: PROJECT, objectPath: '/Game/MCPTest/ctl/M_Both' });
    const verdict = judgeClaim({
      claim: { outcome: 'success', effect: 'created' }, before, after,
      corroboration: [crossTransportObservation({ target: '/Game/MCPTest/ctl/M_Both', present: true, transport: 'stdio' })],
    });
    expect(verdict.verdict).toBe(VERDICTS.PROVEN);
    expect(verdict.independence).toBe(INDEPENDENCE.OUT_OF_BAND);
  });

  it('a read-only claim that moved the world is UNCLAIMED_MUTATION', () => {
    const root = join(ROOT, 'readonly');
    mkdirSync(root, { recursive: true });
    writeFileSync(join(root, 'a.txt'), 'one', { mode: 0o600 });
    const before = observeTree({ root });
    writeFileSync(join(root, 'a.txt'), 'two', { mode: 0o600 });
    const after = observeTree({ root });
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'unchanged' }, before, after }).verdict).toBe(VERDICTS.UNCLAIMED_MUTATION);
  });

  it('POSITIVE CONTROL: a genuinely read-only claim is PROVEN', () => {
    const root = join(ROOT, 'readonly-ok');
    mkdirSync(root, { recursive: true });
    writeFileSync(join(root, 'a.txt'), 'stable', { mode: 0o600 });
    const before = observeTree({ root });
    const after = observeTree({ root });
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'unchanged' }, before, after }).verdict).toBe(VERDICTS.PROVEN);
  });
});

describe('Task 50 — CHANGED and RESIDUE, via tree digests', () => {
  const root = join(ROOT, 'tree');

  it('POSITIVE CONTROL: an empty namespace and a populated one have different digests', () => {
    mkdirSync(root, { recursive: true });
    const empty = observeTree({ root });
    writeFileSync(join(root, 'made.uasset'), 'x', { mode: 0o600 });
    const filled = observeTree({ root });
    expect(empty.present).toBe(false);
    expect(filled.present).toBe(true);
    expect(empty.digest).not.toBe(filled.digest);
  });

  it('the digest is content-addressed: same names, different bytes, different digest', () => {
    writeFileSync(join(root, 'made.uasset'), 'y', { mode: 0o600 });
    const changed = observeTree({ root });
    writeFileSync(join(root, 'made.uasset'), 'z', { mode: 0o600 });
    expect(observeTree({ root }).digest).not.toBe(changed.digest);
  });

  it('RESIDUE: cleanup that left a file behind is caught by the digest, not by a delete response', () => {
    const owned = join(ROOT, 'residue');
    mkdirSync(owned, { recursive: true });
    const baseline = observeTree({ root: owned });
    writeFileSync(join(owned, 'leaked.uasset'), 'still here', { mode: 0o600 });
    const afterCleanup = observeTree({ root: owned });
    const verdict = judgeCleanup({ baseline, afterCleanup, owned });
    expect(verdict.verdict).toBe(VERDICTS.RESIDUE);
    expect(verdict.pass).toBe(false);
  });

  it('POSITIVE CONTROL: cleanup that really restored the baseline PASSES', () => {
    const owned = join(ROOT, 'residue-ok');
    mkdirSync(owned, { recursive: true });
    const baseline = observeTree({ root: owned });
    writeFileSync(join(owned, 'temp.uasset'), 'transient', { mode: 0o600 });
    rmSync(join(owned, 'temp.uasset'));
    const verdict = judgeCleanup({ baseline, afterCleanup: observeTree({ root: owned }), owned });
    expect(verdict.verdict).toBe(VERDICTS.PROVEN);
    expect(verdict.pass).toBe(true);
  });

  it('a NON-EMPTY baseline restored exactly is PROVEN — presence alone would call it residue', () => {
    const owned = join(ROOT, 'residue-preexisting');
    mkdirSync(owned, { recursive: true });
    writeFileSync(join(owned, 'PreExisting.uasset'), 'was here before this run', { mode: 0o600 });
    const baseline = observeTree({ root: owned });
    writeFileSync(join(owned, 'M_Mine.uasset'), 'made by this run', { mode: 0o600 });
    rmSync(join(owned, 'M_Mine.uasset'));
    const afterCleanup = observeTree({ root: owned });
    expect(afterCleanup.present).toBe(true);
    const verdict = judgeCleanup({ baseline, afterCleanup, owned });
    expect(verdict.verdict).toBe(VERDICTS.PROVEN);
    expect(verdict.pass).toBe(true);
  });

  it('OVER-DELETION is caught: cleanup that also removed pre-existing content is NOT clean', () => {
    const owned = join(ROOT, 'residue-overdelete');
    mkdirSync(owned, { recursive: true });
    writeFileSync(join(owned, 'SomebodyElses.uasset'), 'not ours', { mode: 0o600 });
    const baseline = observeTree({ root: owned });
    // A cleanup that swept the folder instead of removing only what it created.
    rmSync(join(owned, 'SomebodyElses.uasset'));
    const afterCleanup = observeTree({ root: owned });
    expect(afterCleanup.present).toBe(false);
    const verdict = judgeCleanup({ baseline, afterCleanup, owned });
    expect(verdict.verdict).toBe(VERDICTS.RESIDUE);
    expect(verdict.pass).toBe(false);
  });

  it('cleanup verified by an INCONCLUSIVE reading is UNPROVEN, never clean', () => {
    const baseline = observeTree({ root: join(ROOT, 'nope') });
    const blind = observation({ kind: 'namespace', mechanism: 'fs:tree-digest', target: 'x', present: null, conclusive: false });
    expect(judgeCleanup({ baseline, afterCleanup: blind, owned: 'x' }).pass).toBe(false);
  });
});

describe('Task 50 — actors, settings, renders, processes, ports, sessions, logs', () => {
  it('ACTOR present/absent from a saved level package', () => {
    writePackage('MCPTest/ctl/L_Actors', ['PersistentLevel', 'Actor_Owned_1'], '.umap');
    const present = observeLevelActor({ projectRoot: PROJECT, levelPath: '/Game/MCPTest/ctl/L_Actors', actorName: 'Actor_Owned_1' });
    const absent = observeLevelActor({ projectRoot: PROJECT, levelPath: '/Game/MCPTest/ctl/L_Actors', actorName: 'Actor_NeverSpawned' });
    expect(present.present).toBe(true);
    expect(absent.present).toBe(false);
    expect(String(present.detail.provesDiskOnly)).toContain('unsaved in-memory actor is invisible');
  });

  it('ACTOR found through the One-File-Per-Actor directory when the level is partitioned', () => {
    const external = join(ROOT, 'ExternalActors');
    mkdirSync(external, { recursive: true });
    const length = Buffer.alloc(4);
    length.writeInt32LE('BP_Partitioned_C_1'.length + 1);
    writeFileSync(join(external, 'A1.uasset'), Buffer.concat([UE_PACKAGE_MAGIC, length, Buffer.from('BP_Partitioned_C_1\0', 'latin1')]), { mode: 0o600 });
    const seen = observeLevelActor({
      projectRoot: PROJECT, levelPath: '/Game/MCPTest/ctl/L_Missing',
      actorName: 'BP_Partitioned_C_1', externalActorsRoot: external,
    });
    expect(seen.present).toBe(true);
    expect(seen.detail.external).toBe(true);
  });

  it('SETTINGS read from the ini, with a changed value producing a changed digest', () => {
    const ini = join(ROOT, 'DefaultEngine.ini');
    writeFileSync(ini, '[/Script/McpAutomationBridge.Settings]\nbEnableNativeMcp=True\nNativeMcpPort=3000\n', { mode: 0o600 });
    const before = observeIniSetting({ file: ini, section: '/Script/McpAutomationBridge.Settings', key: 'NativeMcpPort' });
    expect(before.present).toBe(true);
    expect(before.detail.value).toBe('3000');
    writeFileSync(ini, '[/Script/McpAutomationBridge.Settings]\nbEnableNativeMcp=True\nNativeMcpPort=3001\n', { mode: 0o600 });
    const after = observeIniSetting({ file: ini, section: '/Script/McpAutomationBridge.Settings', key: 'NativeMcpPort' });
    expect(after.digest).not.toBe(before.digest);
    expect(judgeClaim({ claim: { outcome: 'success', effect: 'modified' }, before, after }).verdict).toBe(VERDICTS.PROVEN);
  });

  it('SETTINGS: a key in a DIFFERENT section is absent, not accidentally matched', () => {
    const ini = join(ROOT, 'Sections.ini');
    writeFileSync(ini, '[A]\nPort=1\n[B]\nOther=2\n', { mode: 0o600 });
    expect(observeIniSetting({ file: ini, section: 'B', key: 'Port' }).present).toBe(false);
    expect(observeIniSetting({ file: ini, section: 'A', key: 'Port' }).present).toBe(true);
  });

  it('RENDER: a real PNG reports its true dimensions', () => {
    const file = join(ROOT, 'shot.png');
    writeFileSync(file, png(1920, 1080), { mode: 0o600 });
    const seen = observeRenderOutput({ file });
    expect(seen.present).toBe(true);
    expect(seen.detail).toMatchObject({ format: 'png', width: 1920, height: 1080 });
  });

  it('RENDER: a 0x0 PNG is reported ABSENT — a render that claims success and wrote nothing usable', () => {
    const file = join(ROOT, 'degenerate.png');
    writeFileSync(file, png(0, 0), { mode: 0o600 });
    const seen = observeRenderOutput({ file });
    expect(seen.present).toBe(false);
    expect(seen.detail.areaOk).toBe(false);
  });

  it('RENDER: an empty file that a receipt calls a frame is refused', () => {
    const file = join(ROOT, 'empty.png');
    writeFileSync(file, Buffer.alloc(0), { mode: 0o600 });
    expect(observeRenderOutput({ file }).present).toBe(false);
  });

  it('RENDER: a forged render receipt is caught end to end', () => {
    const file = join(ROOT, 'never-rendered.png');
    const before = observeRenderOutput({ file });
    const after = observeRenderOutput({ file });
    expect(judgeClaim({ claim: forgedSuccessClaim({ target: file }), before, after }).verdict).toBe(VERDICTS.FORGED_SUCCESS);
  });

  it('RENDER: EXR and JPEG headers are recognised rather than reported unknown', () => {
    const exr = Buffer.alloc(8);
    exr.writeUInt32LE(0x01312f76, 0);
    expect(decodeImageHeader(exr).format).toBe('exr');
    // A FULL 17-byte SOF0 as a real encoder emits it (precision, h, w, then three
    // component specs) — not a truncated stub. A fixture in a shape production
    // never produces is how Task 46's F3 kept a green test over a live defect.
    const jpeg = Buffer.concat([
      Buffer.from([0xff, 0xd8]),
      Buffer.from([0xff, 0xc0, 0x00, 0x11, 0x08, 0x02, 0x00, 0x03, 0x00, 0x03]),
      Buffer.from([0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01]),
    ]);
    expect(decodeImageHeader(jpeg)).toMatchObject({ format: 'jpeg', height: 512, width: 768 });
  });

  it('PROCESS: this process is observed alive with a start time; a dead pid is absent', () => {
    const alive = observeProcess({ pid: process.pid });
    expect(alive.present).toBe(true);
    expect(typeof alive.detail.startTicks).toBe('number');
    // pid 0 is never a userspace process, so /proc/0 never exists.
    expect(observeProcess({ pid: 0 }).present).toBe(false);
  });

  it('PORT: an unbound high port is absent; the listener check is conclusive either way', () => {
    const seen = observeListener({ port: 65_530 });
    expect(seen.conclusive).toBe(true);
    expect(seen.present).toBe(false);
  });

  it('PORT: a real listener this test opens is observed present', async () => {
    const { createServer } = await import('node:net');
    const server = createServer();
    await new Promise<void>((settle) => server.listen(0, '127.0.0.1', settle));
    const address = server.address();
    const port = typeof address === 'object' && address !== null ? address.port : 0;
    try {
      expect(observeListener({ port }).present).toBe(true);
    } finally {
      await new Promise<void>((settle) => server.close(() => settle()));
    }
  });

  it('LOG: a marker present in the editor log is found; a missing log is INCONCLUSIVE, not absent', () => {
    const log = join(ROOT, 'Editor.log');
    writeFileSync(log, 'LogMcpNativeTransport: Native MCP server started on http://localhost:3000/mcp\n', { mode: 0o600 });
    expect(observeEditorLog({ file: log, pattern: /Native MCP server started/u }).present).toBe(true);
    expect(observeEditorLog({ file: log, pattern: /NeverLoggedThis/u }).present).toBe(false);
    const missing = observeEditorLog({ file: join(ROOT, 'nope.log'), pattern: /x/u });
    expect(missing.present).toBeNull();
    expect(missing.conclusive).toBe(false);
  });
});

describe('Task 50 — the suite-level positive-control audit', () => {
  it('FAILS a suite whose oracle only ever answered one way', () => {
    const blindSuite = [
      observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'a', present: false }),
      observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'b', present: false }),
    ];
    const audit = auditPositiveControls(blindSuite);
    expect(audit.ok).toBe(false);
    expect(audit.missing[0]).toContain('never saw a present reading');
  });

  it('PASSES only when the same mechanism was watched reading both ways', () => {
    const audit = auditPositiveControls([
      observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'a', present: false }),
      observation({ kind: 'asset', mechanism: 'fs:uasset-package', target: 'b', present: true }),
    ]);
    expect(audit.ok).toBe(true);
  });

  it('counts inconclusive readings separately, so they cannot stand in for either polarity', () => {
    const audit = auditPositiveControls([
      observation({ kind: 'port', mechanism: 'procfs:net-tcp', target: 'a', present: null, conclusive: false }),
      observation({ kind: 'port', mechanism: 'procfs:net-tcp', target: 'b', present: true }),
    ]);
    expect(audit.ok).toBe(false);
    expect(audit.mechanisms[0].inconclusive).toBe(1);
  });
});
