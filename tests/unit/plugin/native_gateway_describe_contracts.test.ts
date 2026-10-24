/**
 * Source contract for discovery on the native /mcp gateway: it answers the replies the TypeScript door answers, field
 * for field. describe takes a capability ID as execute does (the ID every search row and execute nextCall carries);
 * the native door used to ignore the field, so describe {capability} answered with the whole tool list. Its search
 * rows, browse levels and describe replies used to carry their own field names. The C++ cannot run here, so each
 * native builder's field names are read from its body and compared with the TypeScript reply.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { capabilitySearchRow } from '../../../src/server/gateway/gateway-capability-view.js';
import { describeCatalog, describeDomain, describeFamily } from '../../../src/server/gateway/gateway-describe-browse.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';
import { compactGatewayReply } from '../../../src/utils/responses/gateway-reply-compaction.js';

const mcp = (file: string): string =>
  readFileSync(resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private/MCP', file), 'utf8');

const gateway = mcp('Transport/McpNativeTransportGateway.cpp');
const catalog = mcp('Gateway/McpNativeGatewayCatalog.cpp');
const overview = mcp('Gateway/McpNativeGatewayDescribeOverview.cpp');
const describeSource = mcp('Gateway/McpNativeGatewayDescribe.cpp');
const search = mcp('Gateway/McpNativeGatewaySearch.cpp');

/** One C++ function's body: from its signature to the closing brace in column 0. */
function body(text: string, signature: string): string {
  const start = text.indexOf(signature);
  expect(start, `${signature} not found`).toBeGreaterThanOrEqual(0);
  const end = /\r?\n\}\r?\n/u.exec(text.slice(start));
  return text.slice(start, start + (end?.index ?? text.length));
}

/**
 * The field names a body sets on one object: Receiver->Set*Field(TEXT("name"), SetObjectOrEmpty(Receiver, TEXT("name"),
 * and the paging fields SetListing names.
 */
function setKeys(code: string, receiver: string): string[] {
  const keys = [
    ...code.matchAll(new RegExp(`\\b${receiver}->Set\\w+Field\\(TEXT\\("(\\w+)"\\)`, 'gu')),
    ...code.matchAll(new RegExp(`SetObjectOrEmpty\\(${receiver}, TEXT\\("(\\w+)"\\)`, 'gu'))
  ].map((match) => match[1] ?? '');
  for (const match of code.matchAll(new RegExp(`SetListing\\(${receiver}, TEXT\\("(\\w+)"\\), TEXT\\("(\\w+)"\\)`, 'gu'))) {
    const [, rows = '', prefix = ''] = match;
    keys.push(rows, `${prefix}Count`, `${prefix}Offset`, `${prefix}Limit`, `${prefix}HasMore`);
  }
  return [...new Set(keys)].sort();
}

const keysOf = (value: unknown, without: readonly string[] = []): string[] =>
  Object.keys(value as Record<string, unknown>).filter((key) => !without.includes(key)).sort();

const page = { limit: 50, offset: 0 };
const browseEnvelope = setKeys(body(overview, 'TSharedPtr<FJsonObject> BrowseEnvelope('), 'Out');
const browseKeys = (signature: string): string[] =>
  [...new Set([...browseEnvelope, ...setKeys(body(overview, signature), 'Out')])].sort();

describe('native gateway describe', () => {
  it('resolves a capability ID (or a unique alias) to its tool and action, and refuses an unknown one', () => {
    const branch = gateway.slice(gateway.indexOf('if (Operation == TEXT("describe"))'), gateway.indexOf('if (Operation == TEXT("configure"))'));
    expect(branch).toContain('Params->TryGetStringField(TEXT("capability"), Capability)');
    expect(branch).toContain('Index.FindById(Capability)');
    expect(branch).toContain('Index.ResolveAlias(Capability, AliasTarget) == EMcpAliasResolution::Unique');
    expect(branch).toContain('TEXT("UNKNOWN_CAPABILITY")');
    expect(branch).toContain('DiscoveryQuery.Action = McpCapabilityPublicAction(*Record);');
    expect(branch).toContain('TEXT("resolvedFromAlias")');
    expect(branch).toContain('TEXT("MISSING_ACTION")');
  });

  it('browses by domain and family when no tool is named, as the TypeScript door does', () => {
    expect(describeSource).toContain('if (Input.Tool.IsEmpty()) return McpGatewayDescribeBrowse(Input, Store, IsToolEnabled);');
    const branch = gateway.slice(gateway.indexOf('if (Operation == TEXT("describe"))'), gateway.indexOf('if (Operation == TEXT("configure"))'));
    expect(branch).toContain('Params->TryGetStringField(TEXT("domain"), DiscoveryQuery.Domain)');
    expect(branch).toContain('Params->TryGetStringField(TEXT("family"), DiscoveryQuery.Family)');
  });

  it('answers each browse level with the TypeScript fields', () => {
    const catalogReply = describeCatalog(page);
    expect(browseKeys('TSharedPtr<FJsonObject> DescribeCatalog(')).toEqual(keysOf(catalogReply));
    expect(setKeys(body(overview, 'TSharedPtr<FJsonObject> DescribeCatalog('), 'Row')).toEqual(keysOf((catalogReply.domains as unknown[])[0]));
    const domainReply = describeDomain('actor', page);
    expect(browseKeys('TSharedPtr<FJsonObject> DescribeDomain(')).toEqual(keysOf(domainReply));
    expect(setKeys(body(overview, 'TSharedPtr<FJsonObject> DescribeDomain('), 'Row')).toEqual(keysOf((domainReply.families as unknown[])[0]));
    const familyReply = describeFamily('actor', 'spawn', page);
    expect(browseKeys('TSharedPtr<FJsonObject> DescribeFamily(')).toEqual(keysOf(familyReply));
    expect(setKeys(body(overview, 'TSharedPtr<FJsonObject> DescribeFamily('), 'Row')).toEqual(keysOf((familyReply.capabilities as unknown[])[0]));
  });

  it('answers a tool summary, a contract and one parameter with the TypeScript fields', () => {
    expect(setKeys(body(overview, 'TSharedPtr<FJsonObject> McpGatewayToolSummary('), 'Out'))
      .toEqual(keysOf(describeGatewayCapability({ operation: 'describe', tool: 'manage_blueprint' })));
    // Bookkeeping each door's reply compaction removes (gateway-reply-compaction.ts, McpJsonRpcReplyDiscovery.cpp).
    expect(setKeys(body(describeSource, 'TSharedPtr<FJsonObject> CapabilityContract('), 'Out').filter((key) => key !== 'exampleCount'))
      .toEqual(keysOf(describeGatewayCapability({ operation: 'describe', capability: 'asset.delete' }),
        ['category', 'topics', 'perActionSchemas', 'parameterCount']));
    const record = capabilityIndex().byId.get('asset.delete');
    const param = Object.keys(record?.schemas.input.properties ?? {}).find((name) => name !== 'action') ?? '';
    expect(setKeys(body(describeSource, 'TSharedPtr<FJsonObject> McpGatewayDescribeCapability('), 'Out'))
      .toEqual(keysOf(describeGatewayCapability({ operation: 'describe', capability: 'asset.delete', param }),
        ['perActionSchemas', 'variants', 'requiredFor', 'variantRequired']));
  });
});

describe('native gateway search', () => {
  it('builds the row a client reads on the TypeScript door', () => {
    const record = capabilityIndex().byId.get('control_actor.spawn');
    if (record === undefined) throw new Error('control_actor.spawn is not in the catalog');
    const shown = compactGatewayReply({ operation: 'search', success: true, results: [capabilitySearchRow(record, [])] }) as {
      results: unknown[];
    };
    expect(setKeys(body(catalog, 'TSharedPtr<FJsonObject> McpSearchRow('), 'Row')).toEqual(keysOf(shown.results[0]));
  });

  it('sets every envelope field the TypeScript door can answer with', () => {
    const replies = [
      { query: 'actor', limit: 2 },
      { query: 'zzzqqq xxyyzz' },
      { query: 'actor', limit: 100 },
      { query: 'spawn actor', maxBytes: 512 }
    ].map((args) => searchGatewayCapabilities({ operation: 'search', ...args }));
    const stdio = [...new Set(replies.flatMap((reply) => Object.keys(reply)))].sort();
    expect(setKeys(body(search, 'TSharedPtr<FJsonObject> McpGatewaySearchCapabilities('), 'Out')).toEqual(stdio);
  });

  it('reads a cursor, the capability and action filters, and reports clamped integers', () => {
    const branch = gateway.slice(gateway.indexOf('if (Operation == TEXT("search"))'), gateway.indexOf('if (Operation == TEXT("describe"))'));
    for (const field of ['action', 'capability', 'cursor']) expect(branch).toContain(`TEXT("${field}")`);
    expect(branch).toContain('DiscoveryQuery.Coercions.Add(');
    expect(search).toContain('TEXT("INVALID_CURSOR")');
    expect(search).toContain('FString::Printf(TEXT("c:%d"), Offset + Results.Num())');
  });
});

describe('native execute refuses params with the TypeScript guidance', () => {
  it('names the declared parameters and the closest ones, and describes the closest', () => {
    const guidance = setKeys(body(mcp('Gateway/McpNativeGatewayGuidance.cpp'), 'TSharedPtr<FJsonObject> GatewaySchemaViolationGuidance('), 'Guidance');
    expect(guidance).toEqual(['allowedParameters', 'nextCall', 'suggestions']);
    const validation = mcp('Execute/McpNativeGatewayValidation.cpp');
    expect(validation).toContain('GatewaySchemaViolationGuidance(ParentTool, LegacyAction, Violation.Pointer, InputSchema)');
    expect(validation).toContain('TEXT("%s for %s. Call describe before execution.")');
  });
});
