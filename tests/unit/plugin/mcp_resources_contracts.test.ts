import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const root = process.cwd();
const nativeRoot = resolve(
  root,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP',
);

const revisionHeader = readFileSync(resolve(nativeRoot, 'Primitives/McpResourceRevision.h'), 'utf8');
const catalogHeader = readFileSync(resolve(nativeRoot, 'Resources/McpResourceCatalog.h'), 'utf8');
const uriHeader = readFileSync(resolve(nativeRoot, 'Resources/McpResourceUri.h'), 'utf8');

const tsRevision = readFileSync(resolve(root, 'src/server/mcp-primitives/resource-revision.ts'), 'utf8');
const tsCatalog = readFileSync(resolve(root, 'src/resources/resource-catalog.ts'), 'utf8');
// The path/secret vocabulary these contracts compare against the native side
// now lives in one shared policy module, so it is appended to the TS text
// under test: the parity assertion is about what the TS SURFACE enforces,
// not about which file happens to hold the literal.
const tsPathPolicy = readFileSync(resolve(root, 'src/utils/paths/content-path-policy.ts'), 'utf8');
const tsErrors = readFileSync(resolve(root, 'src/resources/resource-errors.ts'), 'utf8') + tsPathPolicy;

const SUBSCRIBABLE_URIS = [
  'ue://capability/catalog',
  'ue://project',
  'ue://level',
  'ue://selection',
  'ue://asset-registry',
  'ue://pie',
  'ue://build',
  'ue://render',
  'ue://logs',
];
const STATIC_RESOURCES = [
  'ue://capability/catalog',
  'ue://project',
  'ue://editor',
  'ue://selection',
  'ue://state/revisions',
];
const TEMPLATES = [
  'ue://capability/{capabilityId}',
  'ue://knowledge/{engineVersion}/{topic}',
  'ue://object/{objectPath}',
  'ue://asset/{assetPath}',
];

const countPureLines = (source: string): number =>
  source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;

describe('mcp resources native source contracts', () => {
  it('mirrors the nine subscribable URIs in the revision primitive on both surfaces', () => {
    for (const uri of SUBSCRIBABLE_URIS) {
      expect(revisionHeader).toContain(uri);
      expect(tsRevision).toContain(uri);
    }
    expect(revisionHeader).toContain('FMcpResourceRevision');
    expect(revisionHeader).toContain('TMcpRevisionedResource');
    expect(revisionHeader).toContain('McpIsSubscribableUri');
  });

  it('mirrors the static resources and templates between TS and native catalogs', () => {
    for (const uri of STATIC_RESOURCES) {
      expect(catalogHeader).toContain(uri);
      expect(tsCatalog).toContain(uri);
    }
    for (const template of TEMPLATES) {
      expect(catalogHeader).toContain(template);
      expect(tsCatalog).toContain(template);
    }
    expect(catalogHeader).toContain('application/json');
  });

  it('mirrors the URI guards and byte budget between TS and native', () => {
    expect(uriHeader).toContain('65536');
    expect(tsErrors).toContain('65536');
    expect(uriHeader).toContain('/Game');
    expect(uriHeader).toContain('/Niagara');
    expect(tsErrors).toContain('/Niagara');
    expect(uriHeader).toContain('RESOURCE_TRAVERSAL_REJECTED');
    expect(uriHeader).toContain('RESOURCE_INVALID_URI');
    expect(uriHeader).toContain('RedactProjectName');
    expect(uriHeader).toContain('TryNormalizeContentPath');
  });

  it('never emits a host path or reads the project-path env in native metadata', () => {
    for (const header of [revisionHeader, catalogHeader]) {
      for (const forbidden of ['C:\\', '/home/', '/Users/', '.uproject', 'UE_PROJECT_PATH']) {
        expect(header).not.toContain(forbidden);
      }
    }
    // The URI guard legitimately lists host-root prefixes to REJECT them, so it
    // is exempt from the host-token scan, but it must not read the project env
    // or emit a concrete .uproject value.
    expect(uriHeader).not.toContain('UE_PROJECT_PATH');
    expect(uriHeader).not.toContain('.uproject');
  });

  it('keeps the new native headers within the 250 pure-line ceiling', () => {
    for (const header of [revisionHeader, catalogHeader, uriHeader]) {
      expect(countPureLines(header)).toBeLessThanOrEqual(250);
    }
  });
});
