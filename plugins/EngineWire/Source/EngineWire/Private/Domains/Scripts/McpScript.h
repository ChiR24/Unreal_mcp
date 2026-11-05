#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"
#include "Foundation/McpCapabilityPrincipal.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
struct FMcpCapturedResponse;
struct FMcpGatewayExecutePlan;

// system_control run_script: a short script of capability calls run in one request, in the S-expression grammar of
// read_graph_as_code (McpGraphCodeParse). (let name (capability.id :param value ...)) calls a capability and keeps its
// result; name.field.0 reads into one; (for item list ...), (if cond ... (else ...)) and, as a value, (if cond a b),
// (expect "assertion"), (return value). Checked whole before anything runs (McpScriptCheck.cpp). Each call then goes
// through the gateway's own validation, is authorized for the caller as a direct execute would be (consent aside: the
// script's grant covers the capabilities it lists), and runs on its handler in-process under a captured id, inside this
// request's change-watch, undo and error-capture brackets. A call that answers later (an import lands on a later tick)
// is awaited within the deadline (McpScriptAwait.cpp): the run goes on in the request's brackets again from its first
// form, every call and expectation an earlier pass reached returning what it did then. A failed step or expectation
// rolls the script back through a restore point, as far as undo reaches.
namespace McpScript
{
using FNames = TMap<FString, TSharedPtr<FJsonValue>>;

constexpr int32 MaxWarnings = 50; // the warnings a script's reply keeps

// One call the script made, for the reply.
struct FStep
{
    int32 Line = 0;
    FString Capability;
    bool bSuccess = false;
    FString ErrorCode;
    FString Message;
};

struct FRunLimits
{
    int32 MaxSteps = 200;
    double DeadlineSeconds = 0.0; // FPlatformTime::Seconds() past which no further step starts
};

// McpScriptValues.cpp
/** Form is a list whose head is the symbol Word. */
bool HeadIs(const McpGraphCode::FSExpr& Form, const TCHAR* Word);
/** A head that names a capability: a dot inside, no |, not a leading dot or a :keyword. */
bool IsCall(const McpGraphCode::FSExpr& Form);
/** == != < <= > >= + - * / */
bool IsOperator(const McpGraphCode::FSExpr& Form);
/** A literal, null, or a name with a dotted path into it (name.field.0.field), as JSON. */
bool EvalAtom(const McpGraphCode::FSExpr& Expr, const FNames& Names, TSharedPtr<FJsonValue>& Out, FString& OutError);
/** Op on two values: == and != on anything (by value), the rest on numbers; null when it does not apply. */
TSharedPtr<FJsonValue> Apply(const FString& Op, const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B);
/** Truth of a value: false, 0, "", null, an empty array or object are false. */
bool IsTrue(const TSharedPtr<FJsonValue>& Value);

// McpScriptCheck.cpp
/** The canonical id of a capability named by id or by a unique alias; empty when none. */
FString ResolveCapability(const FString& Name);
/** Whether the capability asks for consent (its policy's consent is not none). */
bool AsksConsent(const FString& CapabilityId);
/** Why a script may not call the capability (itself, the undo history, or consent it did not list), with OutCode;
 *  empty when it may. */
FString StepRefusal(const FString& CapabilityId, const TSet<FString>& Allowed, FString& OutCode);
/** Before anything runs: every call names a capability a script may run, with only parameters its contract declares
 *  and each one it requires; each capability that asks for consent is in Allowed (canonical ids); every name is bound
 *  before use. False with OutError (and its line) and OutCode. */
bool Check(const TArray<McpGraphCode::FSExpr>& Forms, const TSet<FString>& Allowed, FString& OutError, FString& OutCode);

// McpScriptRun.cpp
class FRunner
{
public:
    FRunner(UEngineWireSubsystem& InSubsystem, const FString& InRequestId, const TSharedPtr<FMcpBridgeWebSocket>& InSocket,
            const FMcpCapabilityPrincipal& InPrincipal, const TSet<FString>& InAllowed, const FRunLimits& InLimits)
        : Subsystem(InSubsystem), RequestId(InRequestId), Socket(InSocket), Principal(InPrincipal), Allowed(InAllowed),
          Limits(InLimits) {}

    /** One pass over the forms from the first: false with Error/ErrorCode at the first failure, or with Waiting set when
     *  a step answers later. Returned holds (return value). */
    bool Run(const TArray<McpGraphCode::FSExpr>& Forms);
    /** McpScriptCall.cpp: the reply of the step the run waited on (none, bCaptured false, when the deadline came first),
     *  taken as a step answering at once is; false when the run ends there. Run goes on from the first form. */
    bool Resume(const FMcpCapturedResponse& Reply);
    double Deadline() const { return Limits.DeadlineSeconds; }

    TArray<FStep> Steps;
    TSharedPtr<FJsonValue> Returned;
    TArray<FString> Warnings; // the steps' own warnings, by line
    FString Error;
    FString ErrorCode;
    FString Waiting;        // while the run waits: the captured id of the step that answers later
    bool bDeferred = false; // that step still ran at the deadline: what it does is past any rollback
    bool bOutside = false;  // a step answered later outside any request's brackets: undo may not reach what it made

private:
    UEngineWireSubsystem& Subsystem;
    FString RequestId;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FMcpCapabilityPrincipal Principal;
    TSet<FString> Allowed;
    FRunLimits Limits;
    bool bReturned = false;
    // Each list or object's node count, a value held twice counted twice (SizeOf): what a list or obj may hold.
    TMap<TSharedPtr<FJsonValue>, int64> Sizes;
    int64 SizeOf(const TSharedPtr<FJsonValue>& Value);
    // What each call and (expect ...) met so far returned, in order (an expectation that held keeps null): a pass after a
    // wait meets them again and takes these, nothing run, validated or judged twice. Next is the one met next.
    TArray<TSharedPtr<FJsonValue>> Memo;
    int32 Next = 0;
    TSharedPtr<FJsonObject> WaitSchema; // the waiting step's declared output, to project its reply by
    bool Recall(TSharedPtr<FJsonValue>& Out);         // true with what an earlier pass met here
    bool Remember(const TSharedPtr<FJsonValue>& Out); // a new outcome; true

    bool Fail(int32 Line, const FString& Code, const FString& Message);
    bool Stmts(const TArray<McpGraphCode::FSExpr>& Body, int32 From, FNames Names);
    bool Stmt(const McpGraphCode::FSExpr& Form, FNames& Names);
    bool Eval(const McpGraphCode::FSExpr& Expr, const FNames& Names, TSharedPtr<FJsonValue>& Out);
    // McpScriptCall.cpp: one capability call, its :param value pairs evaluated, validated, authorized and run in-process.
    bool Pairs(const McpGraphCode::FSExpr& Form, const FNames& Names, TSharedPtr<FJsonObject>& Out);
    bool Call(const McpGraphCode::FSExpr& Form, const FNames& Names, TSharedPtr<FJsonValue>& Out);
    bool Dispatch(int32 Line, const FMcpGatewayExecutePlan& Plan, const FString& StepId, TSharedPtr<FJsonValue>& Out);
    bool Settle(const FMcpCapturedResponse& Reply, int32 NewErrors, const TArray<FString>& Logged,
                const TSharedPtr<FJsonObject>& OutputSchema, TSharedPtr<FJsonValue>& Out);
};

// A run from its first pass to its reply; kept between ticks while it waits on a step (McpScriptAwait.cpp).
struct FScriptRun
{
    FScriptRun(UEngineWireSubsystem& Subsystem, const FString& InRequestId, const TSharedPtr<FMcpBridgeWebSocket>& InSocket,
               const FMcpCapabilityPrincipal& Principal, const TSet<FString>& Allowed, const FRunLimits& Limits)
        : Runner(Subsystem, InRequestId, InSocket, Principal, Allowed, Limits), RequestId(InRequestId), Socket(InSocket) {}

    FRunner Runner;
    TArray<McpGraphCode::FSExpr> Forms;
    FString RequestId;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FString Label;                 // the call as the change journal names it, in a later bracket too
    TFunction<void(bool)> OnDone;  // told at the end whether the script ran to its end (run_recipe)
};

// McpScriptHandler.cpp. OnDone (run_recipe): told, once the script has ended (now, or on a later tick after a step that
// answers later), whether it ran to its end, every step and expectation holding.
bool HandleRunScript(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, TFunction<void(bool)> OnDone = nullptr);
/** One pass of the run, inside the request's brackets: it ends the run (Finish), or waits on a step (Wait). */
void Pass(UEngineWireSubsystem& Subsystem, const TSharedRef<FScriptRun>& Run);
/** The end of the run: the restore point ended or rolled back, the reply, then OnDone. */
void Finish(UEngineWireSubsystem& Subsystem, FScriptRun& Run, bool bRan);
// McpScriptAwait.cpp
/** Keeps what this bracket's reply would have said of its steps, then awaits the step's reply each tick until the
 *  deadline; the run goes on inside the request's brackets again (UEngineWireSubsystem::RunLatentReply). */
void Wait(UEngineWireSubsystem& Subsystem, const TSharedRef<FScriptRun>& Run);
} // namespace McpScript
