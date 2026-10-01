/**
 * Source contracts for the listing details and the refusals the add shares with them.
 *
 * The page script is checked by running it (fab_details_script_behaviour.test.ts); these pin the C++
 * around it: the handler passes on only the fields it names, the adapter tells the page which engine is
 * running, and each refusal the details predict is the one the add answers with, in words.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const plugin = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source');
const fab = (file: string): string => readFileSync(resolve(plugin, 'McpAutomationBridgeFab/Private', file), 'utf8');
const core = (file: string): string => readFileSync(resolve(plugin, 'McpAutomationBridge', file), 'utf8');

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (text: string): string =>
  text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('the details handler', () => {
  const handler = code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabDetails.cpp'));

  it('passes on only the fields it names, so nothing else the page returns reaches a reply', () => {
    for (const field of [
      'category', 'categoryPath', 'currency', 'publishedAt', 'distributionMethod', 'runningEngine', 'versionName',
      'pickedEngineVersion', 'engineMatch', 'addFormat', 'downloadFile', 'quality', 'addBlockedCode', 'addBlockedReason',
      'averageRating', 'ratingCount', 'price', 'downloadBytes', 'licenseNames', 'engineVersions', 'qualities',
      'addWarnings', 'formats', 'canAddToProject', 'isFree', 'isCc0', 'supportsRunningEngine', 'downloadSizeKnown',
    ]) {
      expect(handler, field).toContain(`TEXT("${field}")`);
    }
    // The listing's own flag disagrees with its price, so it is never passed on.
    expect(handler).not.toContain('rawIsFree');
    // The reply is built field by field: the parsed page object is never forwarded as it is.
    expect(handler).not.toMatch(/SetObjectField\([^)]*Parsed\)/u);
  });

  it('reports a number or a flag only when the page carried it, so absent means unknown', () => {
    expect(handler).toMatch(/TryGetNumberField\(Field, Number\)\) \{\s*Data->SetNumberField\(Field, Number\)/u);
    expect(handler).toMatch(/TryGetBoolField\(Flag, bValue\)\) \{\s*Data->SetBoolField\(Flag, bValue\)/u);
  });
});

describe('the adapter tells the details page which engine is running', () => {
  const module = code(fab('McpAutomationBridgeFabModule.cpp'));

  it('formats it as major.minor for both the add and the details', () => {
    expect(module).toContain('McpFabDetailsOperation::Start(ListingId,');
    expect(module.match(/FString::Printf\(TEXT\("%u\.%u"\), Version\.GetMajor\(\), Version\.GetMinor\(\)\)/gu)?.length).toBe(2);
  });

  it('hands the page that string only through a printf slot, after the listing id has been validated', () => {
    const op = code(fab('McpFabDetailsOperation.cpp'));
    expect(op.indexOf('IsSafeListingId(ListingId)')).toBeGreaterThan(-1);
    expect(op).toContain('*RequestId, *ListingId, *EngineVersion, McpFabSelection::Script(), McpFabListing::Script()');
    expect(op.indexOf('IsSafeListingId(ListingId)')).toBeLessThan(op.indexOf('McpFabBridgeDispatch::Dispatch('));
  });
});

describe('the refusals the details predict', () => {
  const reply = code(fab('McpFabAddReply.cpp'));

  it('are worded by the add as well, so a caller who ignored the details is told the same thing', () => {
    expect(reply).toContain('TEXT("METAHUMAN_FORMAT")');
    expect(reply).toContain('TEXT("COMPLETE_PROJECT")');
    expect(reply).toMatch(/Nothing was claimed or downloaded/u);
    expect(reply).toMatch(/Create it as a new project from Fab, then migrate its content/u);
  });

  it('carry the engine the add took, into the add reply and the status read alike', () => {
    expect(reply).toContain('TEXT("engineMatch")');
    expect(reply).toContain('TEXT("engineVersion")');
    const json = code(core('Private/Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h'));
    expect(json).toContain('TEXT("engineMatch")');
    expect(json).toContain('TEXT("engineVersion")');
    const provider = code(core('Public/McpFabTypes.h'));
    expect(provider).toContain('FString EngineMatch;');
    expect(provider).toContain('FString EngineVersion;');
  });
});

describe('the catalog search request', () => {
  const op = code(fab('McpFabSearchOperation.cpp'));
  const handler = code(core('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabSearch.cpp'));

  it('refuses a publisher or a content kind that could break out of the script or ride along in the query', () => {
    expect(op).toContain('TEXT("INVALID_SELLER")');
    expect(op).toContain('TEXT("INVALID_LISTING_TYPE")');
    // Every refusal comes before anything is composed or dispatched.
    expect(op.indexOf('INVALID_LISTING_TYPE')).toBeLessThan(op.indexOf('McpFabBridgeDispatch::Dispatch('));
    // The publisher gets the same text check as the free text; a content kind is one ASCII token.
    expect(op).toContain('!IsSafeQuery(Request.Seller)');
    expect(op).toMatch(/\(C >= TEXT\('a'\) && C <= TEXT\('z'\)\)[\s\S]*C == TEXT\('-'\) \|\| C == TEXT\('_'\)/u);
  });

  it('hands the page the filters only through printf slots, and the page encodes them', () => {
    expect(op).toContain('*RequestId, *Request.Query, *Request.Seller, *Request.ListingType, Limit,');
    const script = rawScript();
    expect(script).toContain('encodeURIComponent(types)');
    expect(script).toContain('encodeURIComponent(seller)');
  });

  it('reports a fact only when the row carried it, and never the listing flag that disagrees with price', () => {
    expect(handler).not.toContain('rawIsFree');
    for (const guarded of ['AverageRating', 'RatingCount', 'Price', 'bIsCc0']) {
      expect(handler, guarded).toContain(`Listing.${guarded}.IsSet()`);
    }
    expect(code(core('Public/McpFabTypes.h'))).not.toContain('bRawIsFree');
  });
});

/** The raw page script of the search, for pins on what it does with the filters. */
function rawScript(): string {
  const source = fab('McpFabSearchOperation.cpp').replace(/\r\n/gu, '\n');
  return [...source.matchAll(/R"JS\(([\s\S]*?)\)JS"/gu)].map((match) => match[1] ?? '').join('');
}
