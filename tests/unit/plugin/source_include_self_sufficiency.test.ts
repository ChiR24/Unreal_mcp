// Every translation unit that calls a shared bridge helper must be able to see
// where that helper is declared, through its OWN include graph.
//
// Unreal compiles this module as a unity build: several .cpp files are
// concatenated into one blob. A file that calls GetJsonStringField without
// including the header that declares it still compiles, silently, as long as
// some other file in the same blob happened to include it. Blob membership is
// an artefact of how many files precede it in the module, so adding an
// unrelated .cpp ANYWHERE earlier reshuffles the blobs and the borrowed
// declaration disappears.
//
// That is not hypothetical. Adding one file under Domains/ControlEditor broke
// Domains/Misc/McpAutomationBridge_MiscHandlersWorldObjects.cpp with twelve
// "identifier not found" errors for helpers it had always used and never
// included; the Misc domain prelude included only CoreMinimal.h. The failure
// points at a file nobody touched, which makes it expensive to read.
//
// Nothing else catches this. CI never compiles the plugin, so the first signal
// is a full editor build on someone's machine. This test reads the include
// graph as text and fails on the borrowed declaration instead.

import { readFileSync, readdirSync } from 'node:fs';
import { relative, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

/** Helpers declared in one shared header and used across many domains. */
const SHARED_HELPERS =
  /\b(?:GetJsonStringField|GetJsonBoolField|GetJsonNumberField|ExtractVectorField|ExtractRotatorField)\s*\(/u;

/** Either the umbrella or the specific header satisfies the requirement. */
const DECLARING_HEADERS: readonly string[] = [
  'Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h',
  'Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h',
];

/** The asset registry reads that replaced UEditorAssetLibrary's, which refuse every call during Play. */
const ASSET_HELPERS = /\b(?:McpAssetExists|McpLoadAsset)\s*\(/u;
const ASSET_HEADERS: readonly string[] = [
  'Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h',
  'Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h',
];

/** The wording of a refused path, declared where SanitizeProjectRelativePath is: every caller of one needs the other. */
const PATH_REFUSAL_HELPERS = /\bMcpPathRefusalMessage\s*\(/u;
const PATH_REFUSAL_HEADERS: readonly string[] = [
  'Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h',
  'Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h',
  'Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPathsRefusal.h',
];

const INCLUDE = /#include\s+"([^"]+)"/gu;

const collectSources = (directory: string, out: Map<string, string>): void => {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      // Generated shards are emitted with their own includes by the generator.
      if (entry.name !== 'Generated') collectSources(entryPath, out);
      continue;
    }
    if (!/\.(?:cpp|h)$/u.test(entry.name)) continue;
    out.set(relative(privateRoot, entryPath).replace(/\\/gu, '/'), readFileSync(entryPath, 'utf8'));
  }
};

const sources = new Map<string, string>();
collectSources(privateRoot, sources);

/** Walks a file's include graph, resolving only includes rooted at Private/. */
const reachesDeclaringHeader = (
  path: string,
  headers: readonly string[] = DECLARING_HEADERS,
  seen = new Set<string>(),
): boolean => {
  if (seen.has(path)) return false;
  seen.add(path);
  const body = sources.get(path);
  if (body === undefined) return false;
  for (const [, included] of body.matchAll(INCLUDE)) {
    if (headers.includes(included)) return true;
    if (sources.has(included) && reachesDeclaringHeader(included, headers, seen)) return true;
  }
  return false;
};

describe('plugin translation units do not borrow declarations from unity-blob neighbours', () => {
  it('finds the shared sources it is supposed to be checking', () => {
    // Guards against the whole suite passing vacuously if the tree moves: a
    // zero-offender result only means something when there are users to check.
    const users = [...sources].filter(([path, body]) => path.endsWith('.cpp') && SHARED_HELPERS.test(body));
    expect(users.length, 'no translation unit uses the shared helpers; the pattern or root is wrong').toBeGreaterThan(50);
  });

  it('every user of the shared JSON helpers includes a header that declares them', () => {
    const borrowed = [...sources]
      .filter(([path, body]) => path.endsWith('.cpp') && SHARED_HELPERS.test(body))
      .map(([path]) => path)
      .filter((path) => !reachesDeclaringHeader(path))
      .sort();

    expect(
      borrowed,
      'these compile only while a unity-blob neighbour includes the helpers for them; ' +
        `include one of ${DECLARING_HEADERS.join(' or ')} from the file or its domain prelude`,
    ).toEqual([]);
  });

  it('every user of the asset registry reads includes the header that declares them', () => {
    const users = [...sources].filter(([path, body]) => path.endsWith('.cpp') && ASSET_HELPERS.test(body));
    expect(users.length).toBeGreaterThan(50);
    expect(users.map(([path]) => path).filter((path) => !reachesDeclaringHeader(path, ASSET_HEADERS)).sort()).toEqual([]);
  });

  it('every user of the path refusal wording includes the header that declares it', () => {
    const users = [...sources].filter(([path, body]) => /\.(?:cpp|h)$/u.test(path) && PATH_REFUSAL_HELPERS.test(body));
    expect(users.length).toBeGreaterThan(50);
    const borrowed = users
      .map(([path]) => path)
      .filter((path) => !PATH_REFUSAL_HEADERS.includes(path) && !reachesDeclaringHeader(path, PATH_REFUSAL_HEADERS))
      .sort();
    expect(borrowed).toEqual([]);
  });
});
