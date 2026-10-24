// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;
namespace McpPratt
{
struct FNode;
}

// Action contracts: what an execute states about the editor (options.require before it runs, options.expect after),
// judged on the game thread against the editor itself. A verdict is ok, failed or unknown (not observable yet, with what
// to wait for); a failed one says what was found and, when known, the call that fixes it. A requirement not met stops
// the call before its handler runs (PRECONDITION_FAILED: nothing changed, a consent grant is handed back). An
// expectation never fails the call it judges: that call already changed the editor, and treating it as failed would
// hand back a single-use grant and make a retry repeat the change; its verdicts and a warning ride on the reply.
namespace McpContracts
{
/** One assertion's verdict: ok, failed or unknown; what was found; and the call that fixes a failure, when known. */
struct FVerdict
{
    FString Verdict = TEXT("ok");
    FString Actual;
    TSharedPtr<FJsonObject> NextCall;
};

/** Holds a call's assertions from dispatch (any thread) until it is admitted and answers. */
void Register(const FString& RequestId, const TArray<FString>& Expect, const TArray<FString>& Require);

/** Before the handler runs, on the game thread: false once it answered PRECONDITION_FAILED for a requirement not met. */
bool Admit(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket);

/** In a successful reply: expectations[] with each verdict, and one warning naming the failures (Result is made when
 *  the reply had none). */
void AddExpectations(const FString& RequestId, TSharedPtr<FJsonObject>& Result);

/** A failed reply judges nothing: its assertions are dropped. */
void Forget(const FString& RequestId);

/** One parsed assertion against the world; Warnings are the reply's own, for noWarnings (null before the call runs). */
FVerdict Judge(const TSharedPtr<McpPratt::FNode>& Node, UWorld* World, const TArray<FString>* Warnings);
} // namespace McpContracts
