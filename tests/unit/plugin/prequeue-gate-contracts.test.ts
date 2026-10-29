import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { CANONICAL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';

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
      'plugins/EngineWire/Source/EngineWire/Private',
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
  'plugins/EngineWire/Source/EngineWire/Private'
);
const CANONICAL_HEADER = 'McpAssetPathCanonical.h';

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
  'Core/Subsystem/EngineWireSubsystemEditorCommands.cpp',
  'Domains/AnimationAuthoring/McpAnimSupport.cpp',
  'Domains/Texture/McpTextureShared.cpp'
];
const CONVERGED_ALIAS_SITES = [
  'Domains/SystemControl/McpSystemAssetValidate.cpp',
  'Domains/Sequence/McpSeqAssetCreation.cpp',
  'Domains/Sequence/Media/McpSeqMediaReflect.cpp',
  'Domains/AudioAuthoring/McpAudioAssetSupport.cpp',
  'Domains/AssetWorkflow/Operations/McpAssetRedirectors.cpp',
  'Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h',
  'Domains/AssetWorkflow/Analysis/McpAssetReports.cpp',
  'Domains/GameFramework/McpGameFrameworkContext.cpp'
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

  // sample_motion is a read, but its inputs press keys in the game (Escape stops Play): a read-only token drove the
  // game. The record says which parameter raises the scope; both ways a call resolves to a record honour it.
  it('a parameter listed in policy.raisedBy raises the demand, by capability id and by old action name', () => {
    const transform = CANONICAL_CAPABILITY_RECORDS.find((record) => record.id === 'control_actor.get_transform');
    expect(transform?.policy.requiredScope).toBe('read');
    expect(transform?.policy.raisedBy).toEqual({ inputs: 'write' });
    expect(transform?.behavior.safeToRetry, 'a retry presses the keys again').toBe(false);
    const source = gateDemand();
    expect(source).toContain('if (!Payload->HasField(Pair.Key))');
    expect(source).toMatch(/ReadPolicy\(Record, OutDemand\.RequiredScope, OutDemand\.ConsentMode\);\s*RaiseForPayload\(Record, Payload, OutDemand\.AlsoRequiredScopes\);/u);
    expect(source).toMatch(/ReadPolicy\(\*Record, Scope, Consent\);\s*RaiseForPayload\(\*Record, Request\.Payload, Demand\.AlsoRequiredScopes\);/u);
    expect(source).toContain('FindById(Request.CapabilityId, Request.Payload, Store, Demand);');
  });

  // Membership is exact-set: replacing read with the raised write let a write-only token read motion samples.
  it('a raised scope is needed as well as the capability\'s own, never instead of it', () => {
    const source = authorization();
    expect(source).toMatch(/bool bAllowed = Principal\.IsScopeAuthorized\(Missing\);\s*for \(const EMcpCapabilityScope Also : Demand\.AlsoRequiredScopes\)/u);
    expect(source).toContain('Mutates(Demand.RequiredScope) || Demand.AlsoRequiredScopes.ContainsByPredicate(Mutates)');
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

// The REVERSE direction of Blocker 1, and the half that was actually open.
//
// The gate reading only `payload.subAction` stops a decoy `payload.action` from
// LOWERING the demand. It does nothing about the opposite: `payload.action` is
// not an unread field — domain dispatchers resolve their sub-action from it — so
// a payload carrying BOTH fields with different values could authorize one
// capability and execute another. `{subAction:"screenshot", action:
// "execute_python"}` would buy in-process code execution on a read-scoped token,
// because the raw WebSocket path applies no schema validation and no post-queue
// re-authorization between the two readers.
//
// The split is RECONCILED, not refused: alias traffic legitimately arrives with
// `action != subAction` (the gateway dispatches the canonical action and the
// handler rewrites `subAction` to the native name — `add_socket` ->
// `create_socket`), so refusing any disagreement would break those capabilities.
// The authoritative field is `subAction` (the one NormalizeAction and every
// dispatcher read first); when they disagree, `action` is OVERWRITTEN from it,
// destroying the decoy rather than trusting it.
describe('the gate and the dispatcher cannot resolve DIFFERENT actions', () => {
  it('the WebSocket authorization hop reconciles a payload whose action and subAction disagree', () => {
    const source = wsAuthority();
    expect(source, 'authorization must read both action fields').toMatch(
      /TryGetStringField\(\s*TEXT\("subAction"\)\s*,\s*PayloadSubAction\s*\)/u
    );
    expect(source, 'a disagreement must be normalized, not preferred').toMatch(
      /SetStringField\(\s*TEXT\("action"\)\s*,\s*PayloadSubAction\s*\)/u
    );
    expect(source, 'the refusals that broke alias traffic must not return').not.toContain(
      'CONFLICTING_ACTION'
    );
    expect(source).toMatch(
      /!PayloadAction\.Equals\(\s*PayloadSubAction\s*,\s*ESearchCase::IgnoreCase\s*\)/u
    );
  });

  it('the reconciliation runs BEFORE the gate resolves a demand, and before dispatch', () => {
    const authority = wsAuthority();
    const reconcileAt = authority.indexOf('SetStringField(TEXT("action"), PayloadSubAction)');
    const gateAt = authority.indexOf('McpPrequeueGate::Authorize(Request)');
    expect(reconcileAt).toBeGreaterThan(-1);
    expect(reconcileAt, 'a split payload must be reconciled before demand resolution').toBeLessThan(
      gateAt
    );

    // And the whole hop still precedes the subsystem queue.
    const messages = wsMessages();
    expect(messages.indexOf('AuthorizeAutomationRequest(Socket, RootObj)')).toBeLessThan(
      messages.indexOf('OnMessageReceived.Execute(')
    );
  });

  it('the native path stamps subAction from the SERVER-resolved action, unconditionally', () => {
    const source = nativeExecute();
    expect(source).toContain('Plan.Arguments->SetStringField(TEXT("subAction"), Plan.LegacyAction)');
    expect(
      source,
      'a caller-supplied subAction must never be preserved in preference to the resolved one'
    ).not.toMatch(/if\s*\(\s*!Plan\.Arguments->HasField\(\s*TEXT\("subAction"\)\s*\)\s*\)/u);
  });

  // Defence in depth on the two paths where the split was demonstrably
  // exploitable: system_control reaches execute_python, control_actor reaches
  // delete. Both resolve in the gate's own order (subAction first) so they
  // agree with it even if the ingest guard is ever bypassed.
  const SUBACTION_FIRST: ReadonlyArray<readonly [string, string[]]> = [
    ['system_control', ['Domains', 'SystemControl', 'McpSystemHandlers.cpp']],
    ['control_actor', ['Domains', 'ControlActor', 'McpActorDispatch.cpp']]
  ];

  it.each(SUBACTION_FIRST)('the %s dispatcher reads subAction before action', (_name, parts) => {
    const source = privateSource(...parts);
    const subActionAt = source.indexOf('TryGetStringField(TEXT("subAction"), SubAction)');
    const actionAt = source.indexOf('TryGetStringField(TEXT("action"), SubAction)');
    expect(subActionAt, 'the dispatcher must consult subAction at all').toBeGreaterThan(-1);
    expect(actionAt, 'action stays as the fallback for legacy callers').toBeGreaterThan(-1);
    expect(subActionAt, 'subAction must be tried FIRST, as NormalizeAction does').toBeLessThan(
      actionAt
    );
  });

  it('the gate file records that payload.action is read by dispatchers', () => {
    expect(
      gateDemand(),
      'the comment that claimed no handler reads payload.action was false and must not return'
    ).not.toContain('no domain handler reads that');
  });
});

describe('Task 40 Blocker 2 — path confinement canonicalizes before it checks', () => {
  it('payload path collection runs the shared canonicalizer', () => {
    expect(pathScan()).toContain('McpCanonicalizeContentPath');
  });

  it('collection no longer gates on a raw literal-prefix allowlist', () => {
    expect(pathScan()).not.toMatch(/StartsWith\(\s*TEXT\("\/Game\/"\)\s*\)/u);
  });

  // One pass over every plugin source file: 0.1 s alone, but over the 10 s default on a loaded
  // machine (the suite's other whole-tree tests carry 60 s for the same reason).
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
  }, 60_000);

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

describe('Task 40 — describe advertises the grant execute demands', () => {
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

// A consent grant on native /mcp carries a nonce this server minted for that capability (describe, or the
// 2026-07-28 consent ask) and spends it once. A made-up nonce was taken as fresh and a missing one as legacy.
describe('consent nonces are minted by the server and required on native /mcp', () => {
  it('native execute asks the gate for an issued nonce', () => {
    expect(nativeExecute()).toContain('AuthRequest.bRequireIssuedNonce = true;');
  });
  it('the gate consumes through the ledger with that requirement and refuses an unissued nonce', () => {
    const gate = gateOrder();
    expect(gate).toMatch(/Consume\(\s*Grant\.ConsentNonce, Demand\.CapabilityId, Request\.bRequireIssuedNonce\)/u);
    expect(gate).toContain('McpAuthorizationCodes::ConsentNotIssued');
  });
  it('describe and the consent ask mint their nonces in the ledger, never as a bare GUID', () => {
    const describeSource = privateSource('MCP', 'Gateway', 'McpNativeGatewayDescribe.cpp');
    expect(describeSource).toContain('FMcpConsentLedger::Get().Mint(Record.Id)');
    const ask = privateSource('MCP', 'Protocol', 'McpModernConsent.cpp');
    expect(ask).toContain('FMcpConsentLedger::Get().Mint(CapabilityId)');
    for (const source of [describeSource, ask]) expect(source).not.toMatch(/"nonce"\),\s*FGuid::NewGuid/u);
  });
  it('an unissued nonce is refused only where issuance is required', () => {
    const ledger = privateSource('Core', 'Security', 'McpConsentLedger.cpp');
    expect(ledger).toContain('if (bRequireIssued && (!IssuedFor || !IssuedFor->Equals(CapabilityId, ESearchCase::CaseSensitive)))');
  });
  // A failure is no proof of no effect: a handler can write a file or run a script no journal records.
  it('a grant is refunded only for a request refused before its handler started', () => {
    const gate = gateOrder();
    const started = gate.slice(gate.indexOf('void NoteDispatchStarted('), gate.indexOf('void RefundConsentForRequest('));
    expect(started).toContain('TakeBurnedNonce(RequestId);');
    const process = privateSource('Core', 'Requests', 'McpProcessRequest.cpp');
    expect(process.indexOf('McpPrequeueGate::NoteDispatchStarted(RequestId);')).toBeGreaterThan(-1);
    expect(process.indexOf('McpPrequeueGate::NoteDispatchStarted(RequestId);')).toBeLessThan(process.indexOf('AutomationHandlers.Find(Action)'));
  });
  // The native door answers a queue refusal itself, outside the subsystem's reply funnel that refunds.
  it('a native call the queue refuses gets its grant back', () => {
    const stream = privateSource('MCP', 'Transport', 'McpNativeTransportGatewayStream.cpp');
    expect(stream).toMatch(/if \(!bQueued\)\s*\{[^}]*McpPrequeueGate::RefundConsentForRequest\(CapturedRequestId\);/u);
  });
});
