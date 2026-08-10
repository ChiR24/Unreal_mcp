import { afterAll, describe, expect, it } from 'vitest';
import { copyFileSync, mkdtempSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { createHash } from 'node:crypto';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { readNativeShard, listNativeShardFiles } from './native-shard-records.js';
import { diffPointers } from './support.js';

const NEUTRAL_JSON_PATH = resolve(
  process.cwd(),
  'src/tools/catalog/capabilities/generated/canonical-registry.generated.json',
);

const scratch = mkdtempSync(join(tmpdir(), 'task-29-mutation-'));
afterAll(() => {
  rmSync(scratch, { recursive: true, force: true });
});

const sha256 = (path: string): string =>
  createHash('sha256').update(readFileSync(path)).digest('hex');

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

interface NeutralRegistry {
  records: Record<string, unknown>[];
}

const copyNeutralToScratch = (name: string): string => {
  const target = join(scratch, name);
  copyFileSync(NEUTRAL_JSON_PATH, target);
  return target;
};

const loadScratch = (path: string): NeutralRegistry =>
  JSON.parse(readFileSync(path, 'utf8')) as NeutralRegistry;

const recordById = (reg: NeutralRegistry, id: string): Record<string, unknown> => {
  const found = reg.records.find((r) => String(r.id) === id);
  if (found === undefined) throw new Error(`Task 29 probe: ${id} absent from the scratch copy.`);
  return found;
};

const tsRecord = (id: string): unknown => {
  const found = ALL_CAPABILITY_RECORDS.find((r) => String(r.id) === id);
  if (found === undefined) throw new Error(`Task 29 probe: ${id} absent from TypeScript records.`);
  return plain(found);
};

describe('Task 29 - a one-field mutation on a COPY is localised to capability + JSON pointer', () => {
  const PROBE_ID = 'animation_physics.create_socket';

  it('a mutated input-schema property description is reported at its exact pointer', () => {
    const copy = copyNeutralToScratch('mutate-input-description.json');
    const reg = loadScratch(copy);
    const record = recordById(reg, PROBE_ID);

    const schemas = record.schemas as Record<string, Record<string, Record<string, Record<string, unknown>>>>;
    schemas.input.properties.socketName.description = '__task29_mutation__';
    writeFileSync(copy, JSON.stringify(reg));

    const diffs = diffPointers(PROBE_ID, tsRecord(PROBE_ID), recordById(loadScratch(copy), PROBE_ID));

    expect(diffs.length).toBeGreaterThan(0);
    expect(diffs[0]?.id).toBe(PROBE_ID);
    expect(diffs[0]?.pointer).toBe('/schemas/input/properties/socketName/description');
    expect(diffs[0]?.actual).toContain('__task29_mutation__');
  });

  it('a mutated behavior flag is reported at its exact pointer', () => {
    const copy = copyNeutralToScratch('mutate-behavior.json');
    const reg = loadScratch(copy);
    const record = recordById(reg, PROBE_ID);

    const behavior = record.behavior as Record<string, unknown>;
    behavior.supportsUndo = !(behavior.supportsUndo as boolean);
    writeFileSync(copy, JSON.stringify(reg));

    const diffs = diffPointers(PROBE_ID, tsRecord(PROBE_ID), recordById(loadScratch(copy), PROBE_ID));

    expect(diffs.some((d) => d.id === PROBE_ID && d.pointer === '/behavior/supportsUndo')).toBe(true);
  });

  it('a mutated availability plugin is reported at its exact array pointer', () => {
    const copy = copyNeutralToScratch('mutate-availability.json');
    const reg = loadScratch(copy);
    const record = recordById(reg, PROBE_ID);

    const availability = record.availability as Record<string, string[]>;
    availability.requiredPlugins = [...availability.requiredPlugins, '__task29_ghost_plugin__'];
    writeFileSync(copy, JSON.stringify(reg));

    const diffs = diffPointers(PROBE_ID, tsRecord(PROBE_ID), recordById(loadScratch(copy), PROBE_ID));

    expect(diffs.some((d) => d.id === PROBE_ID && d.pointer.startsWith('/availability/requiredPlugins/'))).toBe(true);
  });

  it('a mutated content hash is reported at its exact pointer', () => {
    const copy = copyNeutralToScratch('mutate-hash.json');
    const reg = loadScratch(copy);
    const record = recordById(reg, PROBE_ID);

    const hashes = record.hashes as Record<string, string>;
    hashes.content = '0'.repeat(hashes.content.length);
    writeFileSync(copy, JSON.stringify(reg));

    const diffs = diffPointers(PROBE_ID, tsRecord(PROBE_ID), recordById(loadScratch(copy), PROBE_ID));

    expect(diffs.some((d) => d.id === PROBE_ID && d.pointer === '/hashes/content')).toBe(true);
  });

  it('a mutated NATIVE shard record is reported at its exact pointer', () => {
    const source = listNativeShardFiles().find((p) => p.includes('ANIMATION_PHYSICS'));
    if (source === undefined) throw new Error('Task 29 probe: animation_physics shard not found.');

    const copy = join(scratch, 'shard-mutated.cpp');
    copyFileSync(source, copy);
    const text = readFileSync(copy, 'utf8');
    const mutated = text.replace('Create a socket on a bone.', 'Create a socket on a bone!!');
    expect(mutated, 'probe anchor text must exist in the shard').not.toBe(text);
    writeFileSync(copy, mutated);

    const record = readNativeShard(copy).records.find((r) => String(r.id) === PROBE_ID);
    expect(record).toBeDefined();
    if (record === undefined) throw new Error('unreachable');

    const diffs = diffPointers(PROBE_ID, tsRecord(PROBE_ID), record);

    expect(diffs.some((d) => d.id === PROBE_ID && d.pointer === '/discovery/summary')).toBe(true);
  });

  it('the probe never touches the working tree', () => {
    const before = sha256(NEUTRAL_JSON_PATH);
    const copy = copyNeutralToScratch('isolation-check.json');
    const reg = loadScratch(copy);
    recordById(reg, PROBE_ID).id = '__mutated__';
    writeFileSync(copy, JSON.stringify(reg));

    expect(sha256(NEUTRAL_JSON_PATH)).toBe(before);
    expect(statSync(copy).isFile()).toBe(true);
    expect(copy.startsWith(scratch)).toBe(true);
  });
});
