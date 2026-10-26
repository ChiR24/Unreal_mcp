// system_control run_script: capability calls run in one request (plugin Domains/Scripts). Each step is held to what a
// direct execute is held to, for the same caller, and a failure rolls the script back; these are read from the C++ as
// text, and the live cases in tests/mcp-tools/core/run-script.test.mjs run them.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const SCRIPTS = join(PRIVATE, 'Domains', 'Scripts');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (path: string): string => readFileSync(path, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('run_script steps', () => {
  const call = code(join(SCRIPTS, 'McpScriptCall.cpp'));

  it('go through the gateway validation, the caller principal gate and the resolved action, in that order', () => {
    const validated = call.indexOf('ValidateAndResolveGatewayExecute(');
    const refused = call.indexOf('StepRefusal(Plan.CapabilityId');
    const gated = call.indexOf('McpPrequeueGate::AuthorizeStep(Auth)');
    const stamped = call.indexOf('Plan.Arguments->SetStringField(TEXT("subAction"), Plan.LegacyAction)');
    const dispatched = call.indexOf('return Dispatch(');
    for (const at of [validated, refused, gated, stamped, dispatched]) {
      expect(at).toBeGreaterThan(-1);
    }
    expect(validated < refused && refused < gated && gated < stamped && stamped < dispatched).toBe(true);
    expect(call).toContain('Auth.Principal = &Principal;');
  });

  it('bind what a direct execute returns, and fail on an engine error the call logged', () => {
    expect(call).toContain('McpProjectCanonicalOutput(');
    expect(call).toMatch(/Step\.bSuccess = Reply\.bSuccess && NewErrors <= 0;/u);
  });

  it('keep a reply that comes later from ever reaching a transport', () => {
    expect(call).toMatch(/if \(!Reply\.bCaptured\)[\s\S]*?if \(bTaken\)\s*\{\s*Capture\.Begin\(StepId, false\);/u);
  });

  it('are captured as no batch step, so each handler keeps its own undo step and recompile as a direct call does', () => {
    expect(call).toMatch(/Capture\.Begin\(StepId, false\);\s*const bool bTaken = Subsystem\.RunHandlerInProcess\(/u);
    // Where a handler leaves its undo step or recompile to a batch, it asks IsBatchStep, never IsCapturing: a captured
    // run_script step is no batch, and a spawn in a script that kept no undo step stayed after the script rolled back.
    const domains = join(PRIVATE, 'Domains');
    for (const file of [
      join('ControlActor', 'McpActorAdvanced.cpp'), join('ControlActor', 'McpActorMats.cpp'),
      join('MaterialAuthoring', 'McpMatPrivate.h'), join('MaterialAuthoring', 'Nodes', 'McpMatUpdateCustomExpr.cpp'),
      join('Volume', 'McpVolumeHandlers.cpp'),
    ]) {
      const text = code(join(domains, file));
      expect(text, file).toContain('IsBatchStep(RequestId)');
      expect(text, file).not.toContain('IsCapturing(RequestId)');
    }
    // Placement is the exception: an actor described in a reply counts as reported, and no client reads a captured
    // reply, so a script step leaves it to its request's change watch, as a batch item does.
    const support = code(join(domains, 'ControlActor', 'McpActorSupport.h'));
    expect(support).toMatch(/McpBeginSpawnTransaction\(const FString &RequestId\) \{\s*if \(FMcpResponseCaptureRegistry::Get\(\)\.IsBatchStep\(RequestId\)\)/u);
    expect(support).toMatch(/McpDescribeActorPlacement\(const FString &RequestId, AActor \*Actor, const TSharedPtr<FJsonObject> &Data\) \{\s*if \(!FMcpResponseCaptureRegistry::Get\(\)\.IsCapturing\(RequestId\)\)/u);
  });

  it('are read with a nesting bound, so no text recurses past the game thread\'s stack', () => {
    const parse = code(join(PRIVATE, 'Domains', 'BlueprintGraph', 'Code', 'McpGraphCodeParse.cpp'));
    expect(parse).toMatch(/constexpr int32 MaxDepth = \d+;/u);
    expect(parse).toContain('Depth >= MaxDepth');
    expect(parse).toContain('OutError, Depth + 1)');
  });

  it('refuse a nested script and the undo history the rollback runs through', () => {
    const policy = code(join(SCRIPTS, 'McpScriptPolicy.cpp'));
    expect(policy).toContain('CapabilityId == TEXT("system_control.run_script")');
    expect(policy).toContain('CapabilityId == TEXT("control_editor.undo")');
    // A capability the gate cannot resolve asks for consent: fail closed.
    expect(policy).toContain('Demand.CapabilityId.IsEmpty() ||');
  });
});

describe('run_script consent and principal', () => {
  it('a step skips only consent, which the listed capabilities and the run\'s grant cover', () => {
    const gate = code(join(PRIVATE, 'Core', 'Security', 'McpPrequeueGate.cpp'));
    expect(gate).toMatch(/FMcpAuthorizationDecision AuthorizeStep\(const FMcpPrequeueRequest& Request\)\s*\{\s*const FMcpAuthorizationDecision Bound = RequirePrincipal\(Request\);[\s\S]*?Demand\.ConsentMode = TEXT\("none"\);\s*return AuthorizeWithDemand\(Request, Demand\);/u);
    expect(gate).toMatch(/RememberRequestPrincipal\(Request\.RequestId, Principal\);\s*return FMcpAuthorizationDecision::Allow\(\);/u);
  });

  it('the principal lives from the gate to the reply', () => {
    const funnel = code(join(PRIVATE, 'Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp'));
    expect(funnel).toContain('McpPrequeueGate::ForgetRequestPrincipal(RequestId);');
    const handler = code(join(SCRIPTS, 'McpScriptHandler.cpp'));
    expect(handler).toMatch(/if \(!McpPrequeueGate::FindRequestPrincipal\(RequestId, Principal\)\)\s*\{\s*return Refuse\(TEXT\("SCRIPT_NO_PRINCIPAL"\)/u);
  });

  it('nothing runs before the whole script is checked and a restore point is open', () => {
    const handler = code(join(SCRIPTS, 'McpScriptHandler.cpp'));
    const checked = handler.indexOf('if (!Check(Forms, Allowed, Error, Code))');
    const opened = handler.indexOf('RestorePoint(Subsystem, RequestId, TEXT("begin"))');
    const ran = handler.indexOf('Runner.Run(Forms)');
    expect(checked).toBeGreaterThan(-1);
    expect(checked < opened && opened < ran).toBe(true);
    expect(handler).toContain('RestorePoint(Subsystem, RequestId, TEXT("rollback"))');
  });

  it('a restore point opened inside a request rolls back that request\'s own steps', () => {
    const ledger = code(join(PRIVATE, 'Core', 'Requests', 'McpUndoLedger.cpp'));
    expect(ledger).toMatch(/return GOwnedSet\.Contains\(TransactionId\) \|\|\s*\(GRecording && GSteps\.ContainsByPredicate/u);
  });

  // A script of reads that ran past its deadline said "What the script changed was undone" over "Undid 0 of 0 step(s)".
  it('a failed script says it undid changes only when the rollback undid a step', () => {
    const handler = code(join(SCRIPTS, 'McpScriptHandler.cpp'));
    expect(handler).toContain('Undone.Result->TryGetArrayField(TEXT("undone"), UndoneSteps)');
    expect(handler).toMatch(/: bUndid \? TEXT\(" What the script changed was undone\."\)/u);
  });
});
