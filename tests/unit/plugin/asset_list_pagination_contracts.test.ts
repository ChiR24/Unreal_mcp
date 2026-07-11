import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginRoot = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const listingPath = resolve(
    pluginRoot,
    'Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowListing.cpp'
);
const cursorHeaderPath = resolve(
    pluginRoot,
    'Private/Domains/AssetWorkflow/Operations/McpAutomationBridgeAssetListCursor.h'
);

const countPureLines = (source: string): number =>
    source
        .split(/\r?\n/u)
        .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:\/\/|#)/u.test(line))
        .length;

describe('asset list pagination contracts (T4)', () => {
    const listing = readFileSync(listingPath, 'utf8');
    const cursorHeader = readFileSync(cursorHeaderPath, 'utf8');

    it('canonicalizes the path with NormalizeAssetPath and rejects invalid input', () => {
        expect(listing).toMatch(/NormalizeAssetPath\s*\(/);
        expect(listing).toMatch(/INVALID_ARGUMENT/);
    });

    it('decodes and validates the opaque cursor (revision + path containment)', () => {
        expect(listing).toMatch(/McpDecodeAssetListCursor\s*\(/);
        expect(listing).toMatch(/McpGetAssetListRevision\s*\(\)/);
        expect(listing).toMatch(/STALE_CURSOR/);
        expect(listing).toMatch(/INVALID_CURSOR/);
    });

    it('applies validated bounded limits (hard max 500, min 1, non-negative offset)', () => {
        expect(listing).toMatch(/FMath::Clamp\s*\(\s*Limit\s*,\s*1\s*,\s*MaxLimit\s*\)/);
        expect(listing).not.toMatch(/int32\s+Limit\s*=\s*-1/);
    });

    it('sorts the asset list deterministically before slicing', () => {
        expect(listing).toMatch(/AssetList\.Sort\s*\(/);
        expect(listing).toMatch(/PackagePath.*Compare/);
    });

    it('returns a consistent pagination envelope (hasMore / nextOffset / cursor / nextCursor)', () => {
        for (const field of ['hasMore', 'nextOffset', 'cursor', 'nextCursor']) {
            expect(listing).toMatch(new RegExp(`Set(?:Bool|Number|String)Field\\s*\\(\\s*TEXT\\("${field}"\\)`));
        }
    });

    it('exposes cursor encode/decode helpers in a dedicated header', () => {
        expect(cursorHeader).toMatch(/FString\s+McpEncodeAssetListCursor\s*\(/);
        expect(cursorHeader).toMatch(/bool\s+McpDecodeAssetListCursor\s*\(/);
        expect(cursorHeader).toMatch(/McpGetAssetListRevision\s*\(/);
    });

    it('keeps the listing handler within the 250 pure-line ceiling', () => {
        expect(countPureLines(listing)).toBeLessThanOrEqual(250);
    });
});
