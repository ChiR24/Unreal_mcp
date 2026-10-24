// Copyright (c) 2024 EngineWire Contributors
//
// The assertions a call carries, held from dispatch to its reply: requirements judged when the call is admitted to its
// handler, expectations when it answers. Judging is McpContractsJudge.cpp.

#include "Domains/Debug/Contracts/McpContracts.h"

#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Solvers/McpPratt.h"
#include "Misc/ScopeLock.h"

namespace McpContracts
{
namespace
{
constexpr int32 MaxHeld = 256; // a request that never answers must not keep its assertions for the session

struct FHeld
{
    TArray<FString> Expect;
    TArray<FString> Require;
};

FCriticalSection GMutex;
TMap<FString, FHeld> GHeld;

UWorld* JudgedWorld()
{
    return GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
}

// Each assertion as a row {assertion, verdict, actual, nextCall}; OutFailed names the failed ones, and with bStrict the
// unknown ones too (a requirement that cannot be observed is not met).
TArray<TSharedPtr<FJsonValue>> JudgeAll(const TArray<FString>& Texts, const TArray<FString>* Warnings, bool bStrict,
                                        TArray<FString>& OutFailed)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    UWorld* World = JudgedWorld();
    for (const FString& Text : Texts)
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Error;
        FVerdict Verdict;
        Verdict.Verdict = TEXT("unknown");
        if (!IsInGameThread())
        {
            Verdict.Actual = TEXT("the reply came off the game thread, where the editor cannot be read");
        }
        else if (!McpPratt::Parse(Text, Root, Error))
        {
            Verdict.Actual = Error; // the door refuses these; kept for a request that arrived another way
        }
        else
        {
            Verdict = Judge(Root, World, Warnings);
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("assertion"), Text);
        Row->SetStringField(TEXT("verdict"), Verdict.Verdict);
        if (!Verdict.Actual.IsEmpty())
        {
            Row->SetStringField(TEXT("actual"), Verdict.Actual);
        }
        if (Verdict.NextCall.IsValid())
        {
            Row->SetObjectField(TEXT("nextCall"), Verdict.NextCall);
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        if (Verdict.Verdict == TEXT("failed") || (bStrict && Verdict.Verdict == TEXT("unknown")))
        {
            OutFailed.Add(FString::Printf(TEXT("'%s' %s: %s"), *Text, *Verdict.Verdict, *Verdict.Actual));
        }
    }
    return Rows;
}
} // namespace

void Register(const FString& RequestId, const TArray<FString>& Expect, const TArray<FString>& Require)
{
    if (RequestId.IsEmpty() || Expect.Num() + Require.Num() == 0)
    {
        return;
    }
    FScopeLock Lock(&GMutex);
    if (GHeld.Num() >= MaxHeld)
    {
        // ponytail: TMap refills freed slots, so the first entry is only roughly the oldest; a FIFO if this cap ever bites.
        auto Oldest = GHeld.CreateIterator();
        Oldest.RemoveCurrent();
    }
    GHeld.Add(RequestId, FHeld{Expect, Require});
}

bool Admit(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TArray<FString> Require;
    {
        FScopeLock Lock(&GMutex);
        if (FHeld* Held = GHeld.Find(RequestId))
        {
            Require = MoveTemp(Held->Require);
        }
    }
    if (Require.Num() == 0)
    {
        return true;
    }
    TArray<FString> Unmet;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("requirements"), JudgeAll(Require, nullptr, true, Unmet));
    if (Unmet.Num() == 0)
    {
        return true;
    }
    Forget(RequestId); // nothing runs, so nothing is expected of it either
    Bridge.SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Nothing ran: %d of %d requirements are not met: %s"), Unmet.Num(), Require.Num(),
                        *FString::Join(Unmet, TEXT("; "))), Result, TEXT("PRECONDITION_FAILED"));
    return false;
}

void AddExpectations(const FString& RequestId, TSharedPtr<FJsonObject>& Result)
{
    FHeld Held;
    {
        FScopeLock Lock(&GMutex);
        GHeld.RemoveAndCopyValue(RequestId, Held);
    }
    if (Held.Expect.Num() == 0)
    {
        return;
    }
    if (!Result.IsValid())
    {
        Result = MakeShared<FJsonObject>();
    }
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing))
    {
        Warnings = *Existing;
    }
    TArray<FString> Said;
    FString Text;
    for (const TSharedPtr<FJsonValue>& Warning : Warnings)
    {
        if (Warning.IsValid() && Warning->TryGetString(Text))
        {
            Said.Add(Text);
        }
    }
    // A placement the handler judged itself stays in its own field (the change watch does not repeat it): a spawn's
    // placementWarning, a batch's results[].placementWarning.
    TArray<TSharedPtr<FJsonObject>> Placed = {Result};
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (Result->TryGetArrayField(TEXT("results"), Rows))
    {
        for (const TSharedPtr<FJsonValue>& Row : *Rows)
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            if (Row.IsValid() && Row->TryGetObject(Object))
            {
                Placed.Add(*Object);
            }
        }
    }
    for (const TSharedPtr<FJsonObject>& Object : Placed)
    {
        if (Object.IsValid() && Object->TryGetStringField(TEXT("placementWarning"), Text) && !Text.IsEmpty())
        {
            Said.Add(Text);
        }
    }
    TArray<FString> Failed;
    Result->SetArrayField(TEXT("expectations"), JudgeAll(Held.Expect, &Said, false, Failed));
    if (Failed.Num() > 0)
    {
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
            TEXT("%d of %d expectations failed; the call itself succeeded (expectations[] says what was found)."), Failed.Num(), Held.Expect.Num())));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
}

void Forget(const FString& RequestId)
{
    FScopeLock Lock(&GMutex);
    GHeld.Remove(RequestId);
}
} // namespace McpContracts
