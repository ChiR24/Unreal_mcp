import { readFileSync, readdirSync } from 'node:fs';
import { join, relative, resolve, sep } from 'node:path';
import { describe, expect, it } from 'vitest';

// Detect, don't hardcode: engine version comparisons live only in Core/Compatibility/. Code that predates the rule is
// listed in version-allowlist.json, which may only shrink: a new comparison fails, and so does an entry whose code
// was migrated (lower or remove it). The caps below hold the list itself to today's size.
const sourceRoot = resolve(process.cwd(), 'plugins/EngineWire/Source');
const registryPath = resolve(process.cwd(), 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json');
const allow = JSON.parse(readFileSync(resolve(process.cwd(), 'tests/unit/plugin/version-allowlist.json'), 'utf8')) as {
  cpp: Record<string, number>;
  buildCs: Record<string, number>;
  minEngineVersionRecords: string[];
};
const VERSION_TEST = /(#\s*(el)?if|\bif\s*\()[^\n]*\b(ENGINE_(MAJOR|MINOR)_VERSION|UE_VERSION_(NEWER|OLDER)_THAN)\b/;
const BUILD_CS_TEST = /Target\.Version\.(Major|Minor)Version\s*(==|!=|>=|<=|>|<)/g;
// buildCsLines: ApplyMsvcCompatibility's test, the one named build quirk (FeatureDetection.json buildQuirks).
// records: the MVVM family, reflection-only, waits for the engine adapter's roles; the rest moved to availability.compiled.
const CAPS = { cppFiles: 83, cppLines: 111, buildCsLines: 2, records: 6 };

function walk(dir: string, out: string[] = []): string[] {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) walk(path, out);
    else out.push(relative(sourceRoot, path).split(sep).join('/'));
  }
  return out;
}

function countVersionTests(): { cpp: Record<string, number>; buildCs: Record<string, number> } {
  const cpp: Record<string, number> = {};
  const buildCs: Record<string, number> = {};
  for (const file of walk(sourceRoot)) {
    const text = readFileSync(join(sourceRoot, file), 'utf8');
    if (file.endsWith('.Build.cs')) {
      const n = (text.match(BUILD_CS_TEST) ?? []).length;
      if (n) buildCs[file] = n;
    } else if (/\.(h|cpp|inl)$/.test(file) && !file.includes('/Core/Compatibility/')) {
      const n = text.split(/\r?\n/).filter((line) => VERSION_TEST.test(line)).length;
      if (n) cpp[file] = n;
    }
  }
  return { cpp, buildCs };
}

function grownAndStale(actual: Record<string, number>, allowed: Record<string, number>): { grown: string[]; stale: string[] } {
  const grown = Object.entries(actual).filter(([f, n]) => n > (allowed[f] ?? 0)).map(([f, n]) => `${f}: ${n} > ${allowed[f] ?? 0}`);
  const stale = Object.entries(allowed).filter(([f, n]) => (actual[f] ?? 0) < n).map(([f, n]) => `${f}: ${actual[f] ?? 0} < ${n}`);
  return { grown, stale };
}

const sum = (o: Record<string, number>): number => Object.values(o).reduce((a, b) => a + b, 0);

describe('no engine version comparisons outside Core/Compatibility/', () => {
  const actual = countVersionTests();

  it('C++ adds none and the allowlist only shrinks', () => {
    const { grown, stale } = grownAndStale(actual.cpp, allow.cpp);
    expect(grown, 'detect the feature (an MCP_HAS_* macro) or move the difference into a Core/Compatibility/ shim').toEqual([]);
    expect(stale, 'migrated: lower or remove these entries in version-allowlist.json').toEqual([]);
    expect(Object.keys(allow.cpp).length).toBeLessThanOrEqual(CAPS.cppFiles);
    expect(sum(allow.cpp)).toBeLessThanOrEqual(CAPS.cppLines);
  });

  it('Build.cs adds no version comparison', () => {
    const { grown, stale } = grownAndStale(actual.buildCs, allow.buildCs);
    expect(grown, 'decide the feature from the engine files (FeatureDetection), not Target.Version').toEqual([]);
    expect(stale, 'migrated: lower or remove these entries in version-allowlist.json').toEqual([]);
    expect(sum(allow.buildCs)).toBeLessThanOrEqual(CAPS.buildCsLines);
  });

  it('every Build.cs Detect name has a FeatureDetection.json row, and every row is used', () => {
    const buildCs = readFileSync(join(sourceRoot, 'EngineWire/EngineWire.Build.cs'), 'utf8');
    const table = JSON.parse(readFileSync(join(sourceRoot, 'EngineWire/FeatureDetection.json'), 'utf8')) as {
      features: { name: string; paths: string[]; marker?: string; define?: boolean }[];
    };
    // A row marked define is used by DefineDetectedFeatures (MCP_HAS_<name>); every other row by a Detect call by name.
    expect(buildCs).toContain('DefineDetectedFeatures(EngineDir);');
    // The table is a UBT input: an edited row re-runs Build.cs instead of compiling against the cached macros.
    expect(buildCs).toContain('ExternalDependencies.Add(Table);');
    const called = [...buildCs.matchAll(/Detect\(EngineDir, "([A-Z0-9_]+)"\)/g)].map((m) => m[1])
      .concat(table.features.filter((row) => row.define).map((row) => row.name)).sort();
    const rows = table.features.map((row) => row.name).sort();
    expect(rows.filter((name, i) => rows.indexOf(name) !== i), 'duplicate rows').toEqual([]);
    expect([...new Set(called)]).toEqual(rows);
    expect(table.features.filter((row) => !Array.isArray(row.paths) || row.paths.length === 0).map((row) => row.name)).toEqual([]);
  });

  it('every MCP_HAS_* an #if reads is defined somewhere (a misspelt one is 0 and silently compiles code out)', () => {
    const table = JSON.parse(readFileSync(join(sourceRoot, 'EngineWire/FeatureDetection.json'), 'utf8')) as {
      features: { name: string; define?: boolean }[];
    };
    const defined = new Set(table.features.filter((row) => row.define).map((row) => `MCP_HAS_${row.name}`));
    const used: string[] = [];
    for (const file of walk(sourceRoot)) {
      const text = readFileSync(join(sourceRoot, file), 'utf8');
      for (const m of text.matchAll(/"(MCP_HAS_[A-Z0-9_]+)=|#\s*define\s+(MCP_HAS_[A-Z0-9_]+)/g)) defined.add(m[1] ?? m[2] ?? '');
      for (const line of text.split(/\r?\n/).filter((l) => /^\s*#\s*(if|elif)\b/.test(l))) {
        for (const m of line.replace(/defined\s*\(\s*\w+\s*\)/g, '').matchAll(/MCP_HAS_[A-Z0-9_]+/g)) used.push(`${file}: ${m[0]}`);
      }
    }
    expect(used.filter((entry) => !defined.has(entry.split(': ')[1]))).toEqual([]);
  });

  it('records add no minEngineVersion gate', () => {
    const registry = JSON.parse(readFileSync(registryPath, 'utf8')) as {
      records: { id: string; availability?: { minEngineVersion?: string } }[];
    };
    const gated = registry.records.filter((r) => r.availability?.minEngineVersion !== undefined).map((r) => r.id);
    expect(gated.filter((id) => !allow.minEngineVersionRecords.includes(id)), 'gate on roles or compiled features').toEqual([]);
    expect(allow.minEngineVersionRecords.filter((id) => !gated.includes(id)), 'migrated: remove from version-allowlist.json').toEqual([]);
    expect(allow.minEngineVersionRecords.length).toBeLessThanOrEqual(CAPS.records);
  });
});
