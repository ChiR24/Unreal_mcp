#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"
#include "Foundation/McpCapabilityPrincipal.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
struct FMcpGatewayExecutePlan;

// system_control run_script: a short script of capability calls run in one request, in the S-expression grammar of
// read_graph_as_code (McpGraphCodeParse). (let name (capability.id :param value ...)) calls a capability and keeps its
// result; name.field.0 reads into one; (for item list ...), (if cond ... (else ...)), (expect "assertion"),
// (return value). Checked whole before anything runs (McpScriptCheck.cpp). Each call then goes through the gateway's
// own validation, is authorized for the caller as a direct execute would be (consent aside: the script's grant covers
// the capabilities it lists), and runs on its handler in-process under a captured id, inside this request's
// change-watch, undo and error-capture brackets. A failed step or expectation rolls the script back through a restore
// point, as far as undo reaches.
namespace McpScript
{
using FNames = TMap<FString, TSharedPtr<FJsonValue>>;

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

    /** Runs the forms; false with Error/ErrorCode at the first failure. Returned holds (return value). */
    bool Run(const TArray<McpGraphCode::FSExpr>& Forms);

    TArray<FStep> Steps;
    TSharedPtr<FJsonValue> Returned;
    TArray<FString> Warnings; // the steps' own warnings, by line
    FString Error;
    FString ErrorCode;
    bool bDeferred = false; // a step's handler answers later: what it does is past any rollback

private:
    UEngineWireSubsystem& Subsystem;
    FString RequestId;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FMcpCapabilityPrincipal Principal;
    TSet<FString> Allowed;
    FRunLimits Limits;
    bool bReturned = false;

    bool Fail(int32 Line, const FString& Code, const FString& Message);
    bool Stmts(const TArray<McpGraphCode::FSExpr>& Body, int32 From, FNames Names);
    bool Stmt(const McpGraphCode::FSExpr& Form, FNames& Names);
    bool Eval(const McpGraphCode::FSExpr& Expr, const FNames& Names, TSharedPtr<FJsonValue>& Out);
    bool Pairs(const McpGraphCode::FSExpr& Form, const FNames& Names, TSharedPtr<FJsonObject>& Out);
    // McpScriptCall.cpp: one capability call, validated, authorized and run in-process.
    bool Call(const McpGraphCode::FSExpr& Form, const FNames& Names, TSharedPtr<FJsonValue>& Out);
    bool Dispatch(int32 Line, const FMcpGatewayExecutePlan& Plan, const FString& StepId, TSharedPtr<FJsonValue>& Out);
};

// McpScriptHandler.cpp. OutRan (run_recipe): whether the script ran to its end, every step and expectation holding.
bool HandleRunScript(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, bool* OutRan = nullptr);
} // namespace McpScript
