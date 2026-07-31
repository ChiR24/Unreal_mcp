// tests/unit/engine-certification/engine-inventory.test.ts
// Task 52 — the inventory is a TRUTHFUL census, not a wish list.
//
// The acceptance criterion names specific engines, and the temptation is to make
// the inventory produce that list. It must instead produce whatever is really
// installed and say plainly what is not, because the whole point of the census is
// to stop a certification report claiming coverage of a minor nobody ran.
//
// Two failure modes are encoded as assertions rather than prose:
//   - a minor that is present TWICE (this host has 5.0.3 at two roots) must be
//     reported as a duplicate and resolved DETERMINISTICALLY, or two runs of the
//     same command certify different trees under the same heading.
//   - a root that can compile a plugin but cannot launch an editor is not the
//     same thing as an available engine, and merging the two would let a report
//     say "certified on 5.5" when 5.5 has no editor binary on this machine.

import { describe, expect, it } from 'vitest';

import { EXPECTED_MINORS, buildEngineInventory, formatInventoryTable } from './engine-inventory.mjs';

const BUILD = 'Engine/Build/Build.version';
const HEADER = 'Engine/Source/Runtime/Launch/Resources/Version.h';
const EDITOR = 'Engine/Binaries/Linux/UnrealEditor-Cmd';
const RUNUAT = 'Engine/Build/BatchFiles/RunUAT.sh';

type RootSpec = { version: [number, number, number]; editor?: boolean; describe?: string; noBuildVersion?: boolean };

/** An offline fleet of engine roots, keyed by root path. */
function fakeFleet(fleet: Record<string, RootSpec>) {
  const owner = (path: string) => Object.keys(fleet)
    .filter((root) => path.startsWith(`${root}/`))
    .sort((a, b) => b.length - a.length)[0];
  const contentsOf = (root: string, relative: string) => {
    const spec = fleet[root] as RootSpec;
    const [major, minor, patch] = spec.version;
    if (relative === BUILD) {
      return spec.noBuildVersion === true ? null : JSON.stringify({
        MajorVersion: major, MinorVersion: minor, PatchVersion: patch,
        Changelist: 0, CompatibleChangelist: 0, IsLicenseeVersion: 0, IsPromotedBuild: 0, BranchName: 'UE5',
      });
    }
    if (relative === HEADER) {
      return `#define ENGINE_MAJOR_VERSION\t${major}\n#define ENGINE_MINOR_VERSION\t${minor}\n#define ENGINE_PATCH_VERSION\t${patch}\n`;
    }
    if (relative === RUNUAT) return '#!/bin/sh\n';
    if (relative === EDITOR) return spec.editor === false ? null : '';
    return null;
  };
  const lookup = (path: string) => {
    const root = owner(path);
    if (root === undefined) return null;
    return contentsOf(root, path.slice(root.length + 1));
  };
  return {
    readFile: (path: string) => {
      const found = lookup(path);
      if (found === null) throw new Error(`ENOENT ${path}`);
      return found;
    },
    exists: (path: string) => lookup(path) !== null,
    isExecutable: (path: string) => path.endsWith(EDITOR) && lookup(path) !== null,
    describe: (root: string) => fleet[root]?.describe ?? null,
  };
}

/** This host, reproduced offline. Every version below is what the root CONTAINS. */
const THIS_HOST = {
  '/data/UnrealEngine': { version: [5, 7, 4] as [number, number, number], describe: '5.7.0-preview-1-4009-g0bcfaffa52e9' },
  '/data/UnrealEngine-5.0.3': { version: [5, 0, 3] as [number, number, number], editor: false, describe: '5.0.3-release' },
  '/data/UnrealEngine-5.0-branch': { version: [5, 0, 3] as [number, number, number], editor: false, describe: '5.0.0-preview-2-1244-gf7e9b48a637e' },
  '/data/UnrealEngine-5.3.2': { version: [5, 3, 2] as [number, number, number], editor: false, describe: '5.3.2-release' },
  '/data/UnrealEngine-5.5.4': { version: [5, 5, 4] as [number, number, number], editor: false, describe: '5.5.4-release' },
  '/data/UnrealEngine-5.8.0-preview-1': { version: [5, 8, 0] as [number, number, number], describe: '5.8.0-release' },
};

const inventoryOfThisHost = () => buildEngineInventory({
  roots: Object.keys(THIS_HOST),
  io: fakeFleet(THIS_HOST),
});

describe('buildEngineInventory', () => {
  it('reports the minors that are really present', () => {
    expect(inventoryOfThisHost().available.map((entry) => entry.minorKey)).toEqual(['5.0', '5.3', '5.5', '5.7', '5.8']);
  });

  it('reports the minors that are really absent, without softening the list', () => {
    expect(inventoryOfThisHost().missing).toEqual(['5.1', '5.2', '5.4', '5.6']);
  });

  it('files each root under the version it CONTAINS', () => {
    const byMinor = new Map(inventoryOfThisHost().available.map((entry) => [entry.minorKey, entry]));
    expect(byMinor.get('5.7')?.versionString).toBe('5.7.4');
    expect(byMinor.get('5.7')?.preferredRoot).toBe('/data/UnrealEngine');
    // The folder says preview-1; the engine's own tag says 5.8.0-release.
    expect(byMinor.get('5.8')?.versionString).toBe('5.8.0');
    expect(byMinor.get('5.8')?.channel).toBe('stable');
  });

  it('resolves a minor present at two roots deterministically and names both', () => {
    const duplicate = inventoryOfThisHost().duplicates.find((entry) => entry.minorKey === '5.0');
    expect(duplicate?.roots).toEqual(['/data/UnrealEngine-5.0-branch', '/data/UnrealEngine-5.0.3']);
    // Neither 5.0 root has an editor and both are 5.0.3, so the tie breaks on the
    // root path — the same way on every run, which is the only property that matters.
    expect(duplicate?.preferredRoot).toBe('/data/UnrealEngine-5.0-branch');
    expect(inventoryOfThisHost().available.find((entry) => entry.minorKey === '5.0')?.preferredRoot)
      .toBe(duplicate?.preferredRoot);
  });

  it('separates "a plugin can be built here" from "a certification can be RUN here"', () => {
    const inventory = inventoryOfThisHost();
    expect(inventory.available.filter((entry) => entry.runnable).map((entry) => entry.minorKey)).toEqual(['5.7', '5.8']);
    expect(inventory.available.every((entry) => entry.buildable)).toBe(true);
    expect(inventory.certifiable.map((entry) => entry.minorKey)).toEqual(['5.7', '5.8']);
  });

  it('records folder names that contradict their contents', () => {
    const contradicted = inventoryOfThisHost().folderNameContradictions;
    expect(contradicted.map((entry) => entry.root)).toEqual(['/data/UnrealEngine-5.8.0-preview-1']);
    expect(contradicted[0]?.kind).toBe('CHANNEL_LABEL');
  });

  it('lists an unreadable root as unusable instead of silently dropping it', () => {
    const fleet = { ...THIS_HOST, '/data/UnrealEngine-5.6.0': { version: [5, 6, 0] as [number, number, number], noBuildVersion: true } };
    const inventory = buildEngineInventory({ roots: Object.keys(fleet), io: fakeFleet(fleet) });
    expect(inventory.unusable.map((entry) => entry.root)).toEqual(['/data/UnrealEngine-5.6.0']);
    // and it does NOT become an available 5.6 on the strength of its name
    expect(inventory.missing).toContain('5.6');
  });

  it('resolves a requested minor to the root that contains it, and refuses one that is absent', () => {
    const inventory = inventoryOfThisHost();
    expect(inventory.resolve('5.7').ok).toBe(true);
    expect(inventory.resolve('5.7').root).toBe('/data/UnrealEngine');
    expect(inventory.resolve('5.4').ok).toBe(false);
    expect(inventory.resolve('5.4').reason).toBe('MINOR_NOT_INSTALLED');
    expect(inventory.resolve('5.5').reason).toBe('NO_COMPILED_EDITOR');
  });

  it('covers every expected minor exactly once, as available or missing', () => {
    const inventory = inventoryOfThisHost();
    expect([...inventory.available.map((entry) => entry.minorKey), ...inventory.missing].sort())
      .toEqual([...EXPECTED_MINORS].sort());
  });
});

describe('formatInventoryTable', () => {
  it('renders one row per expected minor with its proven identity', () => {
    const table = formatInventoryTable(inventoryOfThisHost());
    expect(table).toContain('5.7.4');
    expect(table).toContain('/data/UnrealEngine');
    expect(table).toContain('MISSING');
    expect(table.split('\n').filter((line) => line.startsWith('| 5.')).length).toBe(EXPECTED_MINORS.length);
  });
});
