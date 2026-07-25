import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Task 40 remediation — enforcement-wiring contracts.
//
// The gate itself is unit-tested in Private/Tests/McpPrequeueGateTests.cpp, but
// a passing predicate proves nothing if nobody CALLS it. Every assertion here
// fails if the corresponding enforcement line is deleted, so the wiring cannot
// silently regress the way it did before this remediation.

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts
    ),
    'utf8'
  );

const wsMessages = (): string =>
  privateSource('Transport', 'Connection', 'McpConnectionManagerMessages.cpp');
const wsAuthority = (): string =>
  privateSource('Transport', 'Connection', 'McpConnectionManagerAuthority.cpp');
const nativeExecute = (): string =>
  privateSource('MCP', 'Execute', 'McpNativeTransportGatewayExecute.cpp');
const nativeConnection = (): string =>
  privateSource('MCP', 'Transport', 'McpNativeTransportConnection.cpp');
const nativePrimitives = (): string =>
  privateSource('MCP', 'Transport', 'McpNativeTransportPrimitives.cpp');
const gateDemand = (): string => privateSource('Core', 'Security', 'McpPrequeueDemand.cpp');
const gateOrder = (): string => privateSource('Core', 'Security', 'McpPrequeueGate.cpp');
const pathScan = (): string => privateSource('Foundation', 'McpCapabilityPathScan.cpp');
const authorization = (): string => privateSource('Foundation', 'McpCapabilityAuthorization.cpp');

const PRIVATE_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);
const CANONICAL_HEADER = 'McpAutomationBridgeHelpersAssetPathCanonical.h';

// A `/Content` -> `/Game` alias map cannot be written without a `/Content`
// STRING LITERAL, so "no such literal outside the one shared place" tests the
// claim itself rather than a proxy for it. Prose mentions of /Content are not
// matched, and Private/Tests is exempt because those literals are test INPUTS.
const CONTENT_LITERAL = /"[^"\n]*\/Content/u;

const pluginSourceFiles = (): string[] =>
  readdirSync(PRIVATE_ROOT, { recursive: true, encoding: 'utf8' })
    .map((entry) => entry.replace(/\\/gu, '/'))
    .filter((rel) => rel.endsWith('.h') || rel.endsWith('.cpp'));

// Every site that used to carry its own alias map. Three of them (the first
// group) resolve a WRITE target and must run the whole canonicalizer; the rest
// only need the shared boundary-aware root map.
const CONVERGED_EXECUTORS = [
  'Core/Subsystem/McpAutomationBridgeSubsystemEditorCommands.cpp',
  'Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.cpp',
  'Domains/Texture/McpAutomationBridge_TextureHandlersShared.cpp'
];
const CONVERGED_ALIAS_SITES = [
  'MCP/Resources/McpResourceUri.h',
  'Domains/SystemControl/McpAutomationBridge_SystemControlHandlersAssetValidation.cpp',
  'Domains/Sequence/McpAutomationBridge_SequenceHandlersAssetCreation.cpp',
  'Domains/Sequence/Media/McpAutomationBridge_SequenceMediaReflection.cpp',
  'Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersAssetSupport.cpp',
  'Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowRedirectors.cpp',
  'Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkRename.cpp',
  'Domains/AssetWorkflow/Analysis/McpAutomationBridge_AssetWorkflowReports.cpp',
  'Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.cpp'
];

describe('Task 40 — both transports gate before anything is enqueued', () => {
  it('the WebSocket bridge authorizes before dispatching to the subsystem queue', () => {
    const source = wsMessages();
    const authorizeAt = source.indexOf('AuthorizeAutomationRequest(Socket, RootObj)');
    const dispatchAt = source.indexOf('OnMessageReceived.Execute(');
    expect(authorizeAt, 'WebSocket path must call AuthorizeAutomationRequest').toBeGreaterThan(-1);
    expect(dispatchAt).toBeGreaterThan(-1);
    expect(authorizeAt).toBeLessThan(dispatchAt);
  });

  it('the WebSocket authorization hop runs the shared pre-queue gate', () => {
    expect(wsAuthority()).toContain('McpPrequeueGate::Authorize(Request)');
  });

  it('the native /mcp execute path runs the same gate before streaming the call', () => {
    const source = nativeExecute();
    const gateAt = source.indexOf('McpPrequeueGate::Authorize(AuthRequest)');
    const streamAt = source.indexOf('StreamToolCall(');
    expect(gateAt, 'native execute must call McpPrequeueGate::Authorize').toBeGreaterThan(-1);
    expect(streamAt).toBeGreaterThan(-1);
    expect(gateAt).toBeLessThan(streamAt);
  });
});

describe('Task 40 Blocker 1 — the gate and the dispatcher resolve the SAME action', () => {
  it('demand resolution reuses the canonical shared normalizer', () => {
    expect(gateDemand()).toContain('McpHandlerUtils::NormalizeAction');
  });

  it('no gate source reads a client-supplied payload "action" decoy', () => {
    for (const source of [gateDemand(), privateSource('Core', 'Security', 'McpPrequeueGate.cpp')]) {
      expect(source).not.toMatch(/TryGetStringField\(\s*TEXT\("action"\)/u);
    }
  });

  it('dispatch-target narrowing fails closed when it yields no candidate', () => {
    const source = gateDemand();
    expect(source).toMatch(/Narrowed\.Num\(\)\s*==\s*0/u);
    expect(
      source,
      'a zero-candidate narrowing must not silently keep the un-narrowed list'
    ).not.toMatch(/if\s*\(\s*Narrowed\.Num\(\)\s*>\s*0\s*\)/u);
  });
});

describe('Task 40 Blocker 2 — path confinement canonicalizes before it checks', () => {
  it('payload path collection runs the shared canonicalizer', () => {
    expect(pathScan()).toContain('McpCanonicalizeContentPath');
  });

  it('collection no longer gates on a raw literal-prefix allowlist', () => {
    expect(pathScan()).not.toMatch(/StartsWith\(\s*TEXT\("\/Game\/"\)\s*\)/u);
  });

  it('the /Content alias mapping lives in exactly one shared place', () => {
    const canonical = readFileSync(
      resolve(PRIVATE_ROOT, 'Foundation/BridgeHelpers/Security', CANONICAL_HEADER),
      'utf8'
    );
    expect(canonical).toContain('McpCanonicalizeContentPath');
    expect(canonical).toMatch(/TEXT\("\/Content"\)/u);

    const offenders = pluginSourceFiles()
      .filter((rel) => !rel.endsWith(CANONICAL_HEADER) && !rel.startsWith('Tests/'))
      .filter((rel) => CONTENT_LITERAL.test(readFileSync(resolve(PRIVATE_ROOT, rel), 'utf8')));

    expect(
      offenders,
      `these files still carry their own /Content alias literal: ${offenders.join(', ')}`
    ).toEqual([]);
  });

  it('every converged site routes through the shared canonicalizer', () => {
    for (const rel of [...CONVERGED_EXECUTORS, ...CONVERGED_ALIAS_SITES]) {
      const source = readFileSync(resolve(PRIVATE_ROOT, rel), 'utf8');
      expect(source, `${rel} must call the shared canonicalizer`).toMatch(
        /McpAssetPathCanonical::MapContentRootInline|McpCanonicalizeContentPath/u
      );
    }
  });
});

describe('Task 40 Blocker 7 — the executors resolve paths exactly as the gate did', () => {
  it('each write executor runs the WHOLE canonicalizer, not a re-ordered replay', () => {
    for (const rel of CONVERGED_EXECUTORS) {
      const source = readFileSync(resolve(PRIVATE_ROOT, rel), 'utf8');
      expect(source, `${rel} must canonicalize in one call`).toContain(
        'McpCanonicalizeContentPath'
      );
      // Calling only the alias-map step is how the ordering bug returns: the
      // step maps `/Content` BEFORE separators are normalized, so a
      // backslash-prefixed value stays invisible to it.
      expect(
        source,
        `${rel} must not replay the canonicalizer's steps itself`
      ).not.toContain('MapContentRootInline');
    }
  });
});

describe('Task 40 Blocker 8 — a truncated payload scan fails closed', () => {
  it('the path scan reports truncation at BOTH bounds', () => {
    const source = pathScan();
    const marks = source.match(/Out->bTruncated = true/gu) ?? [];
    expect(marks.length, 'node-budget AND depth exhaustion must both be reported').toBeGreaterThanOrEqual(3);
  });

  it('the gate refuses on a truncated scan instead of admitting it', () => {
    expect(authorization()).toMatch(/Scan\.bTruncated/u);
    expect(authorization()).toContain('PathNotPermitted');
  });

  it('the console-command scan gets the same fail-closed treatment', () => {
    const source = gateOrder();
    expect(source).toMatch(/State\.bTruncated = true/u);
    expect(source).toMatch(/if\s*\(\s*State\.bTruncated\s*\)/u);
  });
});

describe('Task 40 Blocker 9 — a confined write must prove an in-prefix target', () => {
  it('coverage runs after containment, on the same scan result', () => {
    const source = gateOrder();
    const scanAt = source.indexOf('CollectPayloadPaths(');
    const pathsAt = source.indexOf('CheckPaths(Principal, Scan.Paths)');
    const coverageAt = source.indexOf('CheckPathCoverage(Principal, Demand, Scan)');
    expect(scanAt, 'the gate must collect once').toBeGreaterThan(-1);
    expect(pathsAt, 'containment must run').toBeGreaterThan(scanAt);
    expect(coverageAt, 'coverage must run after containment').toBeGreaterThan(pathsAt);
  });

  it('the narrowing is read from the capability record, not a hardcoded action list', () => {
    const source = gateDemand();
    expect(source).toContain('DeclaresPathParameter');
    expect(source).toContain('McpCapabilityAuthorization::IsPathParameterKey');
    expect(source, 'an unreadable schema must fail closed').toMatch(
      /if\s*\(!Record\.InputSchema\.IsValid\(\)\)\s*\{\s*return true;/u
    );
  });

  it('the coverage rule only fires for a mutating, path-declaring capability', () => {
    const source = authorization();
    expect(source).toMatch(/EMcpCapabilityScope::Write\s*\|\|/u);
    expect(source).toContain('Demand.bDeclaresPathParameter');
    expect(source, 'an unrestricted principal must be exempt').toMatch(
      /if\s*\(!Principal\.IsPathRestricted\(\)\)/u
    );
  });
});

describe('Task 40 — describe advertises the grant execute demands, on BOTH surfaces', () => {
  it('the native capability contract emits consentGrant', () => {
    const source = privateSource('MCP', 'Gateway', 'McpNativeGatewayDescribe.cpp');
    expect(source, 'native describe must emit the field its own tool description names').toContain(
      'SetObjectField(TEXT("consentGrant")'
    );
    // "none" must stay absent rather than emit an empty grant, or a client would
    // send a consent sibling the gate never asked for.
    expect(source).toMatch(/TEXT\("none"\)[^)]*\)\)\s*return nullptr;/u);
    expect(source).toMatch(/TEXT\("elevated"\)\s*:\s*TEXT\("explicit"\)/u);
  });

  it('the TypeScript surface answers the same question the same way', () => {
    const source = readFileSync(
      resolve(process.cwd(), 'src/server/gateway/gateway-capability-view.ts'),
      'utf8'
    );
    expect(source).toContain('capabilityConsentGrant');
    expect(source).toMatch(/consent === 'none'\) return undefined/u);
    expect(source).toMatch(/consent === 'elevated' \? 'elevated' : 'explicit'/u);
  });
});

describe('Task 40 Blocker 4 — a presented token that fails to resolve is refused', () => {
  it('the WebSocket handshake refuses regardless of bRequireCapabilityToken', () => {
    const source = wsAuthority();
    expect(source).toMatch(
      /\(\s*Settings->bRequireCapabilityToken\s*\|\|\s*!PresentedToken\.IsEmpty\(\)\s*\)\s*&&\s*!Principal\.bAuthenticated/u
    );
  });

  it('the native transport refuses a non-empty unresolvable token', () => {
    const source = nativeConnection();
    expect(source).toMatch(/!HttpReq\.CapabilityToken\.IsEmpty\(\)/u);
    expect(source).toContain('McpResolveNativePrincipal(HttpReq.CapabilityToken)');
  });
});

describe('Task 40 Blocker 6 — native MCP primitives are not an ungated read channel', () => {
  it('primitive dispatch consults the session principal', () => {
    expect(nativePrimitives()).toContain('McpAuthorizePrimitiveRead');
  });

  it('tools/list is gated on the same read demand before it builds a listing', () => {
    const source = privateSource('MCP', 'Transport', 'McpNativeTransportToolDiscovery.cpp');
    const gateAt = source.indexOf('McpAuthorizePrimitiveRead');
    const buildAt = source.indexOf('BuildUnrealGatewayToolDefinition()');
    expect(gateAt, 'tools/list must be gated').toBeGreaterThan(-1);
    expect(buildAt).toBeGreaterThan(-1);
    expect(gateAt).toBeLessThan(buildAt);
  });

  it('the primitive gate charges the request budget, not the tool-call budget', () => {
    const source = privateSource('MCP', 'Execute', 'McpNativeGatewayAuthorization.cpp');
    expect(source).toMatch(/Request\.bIsToolCall\s*=\s*false/u);
  });
});
