#include "Core/Requests/McpUndoLedger.h"

#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Editor/Transactor.h"
#include "Misc/ITransaction.h"

namespace McpUndoLedger
{
namespace // inside the module namespace: a unity blob merges files, and these helper names are common
{
struct FStep
{
    FGuid Id;
    FString Title;
    bool bDropped = false; // recorded no change (or only play-world objects): the buffer pops it as it closes
};

FString GRequestId;
bool GRecording = false;
TArray<FStep> GSteps;
FGuid GTopAtBegin; // the live step on top when the request began; gone afterwards = the history was cleared
TWeakObjectPtr<UTransBuffer> GBound;
FDelegateHandle GStateHandle;
// Every step a request through this server left, this editor session: what a restore point may undo.
constexpr int32 MaxOwned = 4096; // the plugin-side cap of the execute idempotency ledger
TArray<FGuid> GOwned;            // oldest first, so the cap drops the oldest
TSet<FGuid> GOwnedSet;
TFunction<void()> GHeld; // the request's reply, sent inside its handler's open transaction, waiting for it to close
bool GFlushing = false;

UTransBuffer* Buffer()
{
    return GEditor ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
}

FGuid IdAt(const UTransBuffer& Trans, const int32 Index)
{
    const FTransaction* Transaction = Trans.GetTransaction(Index);
    return Transaction ? Transaction->GetContext().TransactionId : FGuid();
}

void OnTransactionState(const FTransactionContext& Context, const ETransactionStateEventType Event)
{
    UTransBuffer* Trans = GBound.Get();
    if (!GRecording || !Trans || Event != ETransactionStateEventType::TransactionFinalized)
    {
        return;
    }
    // Broadcast with the closing transaction on top, before End pops a transient one, so the verdict is known here.
    FStep Step;
    Step.Id = Context.TransactionId;
    Step.Title = Context.Title.ToString();
    const FTransaction* Top = Trans->GetQueueLength() > 0 ? Trans->GetTransaction(Trans->GetQueueLength() - 1) : nullptr;
    Step.bDropped = Top && Top->GetContext().TransactionId == Context.TransactionId && Top->IsTransient();
    GSteps.Add(MoveTemp(Step));
}

// Whether Id is in the buffer and not undone.
bool OnStack(const UTransBuffer& Trans, const FGuid& Id)
{
    for (int32 Index = Trans.GetQueueLength() - Trans.GetUndoCount() - 1; Index >= 0; --Index)
    {
        if (IdAt(Trans, Index) == Id)
        {
            return true;
        }
    }
    return false;
}

bool InBuffer(const UTransBuffer& Trans, const FGuid& Id)
{
    for (int32 Index = Trans.GetQueueLength() - 1; Index >= 0; --Index)
    {
        if (IdAt(Trans, Index) == Id)
        {
            return true;
        }
    }
    return false;
}

void WriteBlock(const TSharedPtr<FJsonObject>& Result, const TArray<const FStep*>& Kept, const bool bCleared)
{
    TSharedPtr<FJsonObject> Undo = MakeShared<FJsonObject>();
    Undo->SetBoolField(TEXT("undoable"), Kept.Num() > 0);
    if (Kept.Num() > 0)
    {
        Undo->SetStringField(TEXT("transactionScope"), Kept.Last()->Title);
        Undo->SetNumberField(TEXT("steps"), Kept.Num());
    }
    else if (bCleared)
    {
        Undo->SetStringField(TEXT("reasonCode"), TEXT("UNDO_HISTORY_CLEARED"));
        Undo->SetStringField(TEXT("reason"), TEXT("Something this call ran (a Blueprint compile, a level load) cleared the "
            "editor's undo history, so undo cannot go back to before it."));
    }
    else if (GEditor && GEditor->PlayWorld)
    {
        // The running game's objects are transient, so a transaction keeps none of them: an agent took the change as
        // made in the level and lost it when Play stopped.
        Undo->SetStringField(TEXT("reasonCode"), TEXT("PLAY_WORLD_ONLY"));
        Undo->SetStringField(TEXT("reason"), TEXT("Play is running and the editor kept no undo step for this call: it "
            "changed only the running game (or nothing), which nothing saves or undoes and which ends when Play stops."));
    }
    else
    {
        Undo->SetStringField(TEXT("reasonCode"), TEXT("NOTHING_CHANGED"));
        Undo->SetStringField(TEXT("reason"), TEXT("The editor kept no undo step for this call: what it set matched what "
            "was there, or it changed only the running game."));
    }
    Result->SetObjectField(TEXT("undo"), Undo);
}
} // namespace

void Begin(const FString& RequestId)
{
    End();
    UTransBuffer* Trans = Buffer();
    GRequestId = RequestId;
    GSteps.Reset();
    GBound = Trans;
    // The top live step: an undone one above it is discarded by the next transaction, which is no clearing.
    const int32 Live = Trans ? Trans->GetQueueLength() - Trans->GetUndoCount() : 0;
    GTopAtBegin = Live > 0 ? IdAt(*Trans, Live - 1) : FGuid();
    if (Trans)
    {
        GStateHandle = Trans->OnTransactionStateChanged().AddStatic(&OnTransactionState);
        GRecording = true;
    }
}

void End()
{
    if (UTransBuffer* Trans = GBound.Get(); Trans && GStateHandle.IsValid())
    {
        Trans->OnTransactionStateChanged().Remove(GStateHandle);
        for (const FStep& Step : GSteps)
        {
            if (!Step.bDropped && !GOwnedSet.Contains(Step.Id))
            {
                GOwned.Add(Step.Id);
                GOwnedSet.Add(Step.Id);
                if (GOwned.Num() > MaxOwned)
                {
                    GOwnedSet.Remove(GOwned[0]);
                    GOwned.RemoveAt(0);
                }
            }
        }
    }
    GStateHandle.Reset();
    GRecording = false;
}

bool IsMcpStep(const FGuid& TransactionId)
{
    // Or a step the request still running left: a script rolls its own steps back before its request ends.
    return GOwnedSet.Contains(TransactionId) ||
           (GRecording && GSteps.ContainsByPredicate([&TransactionId](const FStep& Step)
                                                     { return !Step.bDropped && Step.Id == TransactionId; }));
}

bool Hold(const FString& RequestId, TFunction<void()> Send)
{
    UTransBuffer* Trans = GBound.Get();
    if (!GRecording || GFlushing || RequestId != GRequestId || !Trans || !Trans->IsActive())
    {
        return false;
    }
    if (!GHeld) // a second reply to the same request is dropped, as the first one sent would have won
    {
        GHeld = MoveTemp(Send);
    }
    return true;
}

void Flush()
{
    if (!GHeld)
    {
        return;
    }
    TFunction<void()> Send = MoveTemp(GHeld);
    GHeld = nullptr;
    TGuardValue<bool> Flushing(GFlushing, true);
    Send();
}

void Describe(const FString& RequestId, const TSharedPtr<FJsonObject>& Result)
{
    UTransBuffer* Trans = GBound.Get();
    // A transaction still open records until it closes, and a reply sent after its handler returned (deferred) is
    // past End, unless it runs inside RunLatentReply: what such a call leaves on the stack is not known here.
    if (!GRecording || !Trans || !Result.IsValid() || RequestId != GRequestId || Trans->IsActive())
    {
        return;
    }
    TArray<const FStep*> Kept;
    bool bCleared = GTopAtBegin.IsValid() && !InBuffer(*Trans, GTopAtBegin);
    for (const FStep& Step : GSteps)
    {
        if (OnStack(*Trans, Step.Id))
        {
            Kept.Add(&Step);
        }
        else if (!Step.bDropped)
        {
            bCleared |= !InBuffer(*Trans, Step.Id); // kept when it closed, gone now: cleared (an undone one is still in it)
        }
    }
    const TSharedPtr<FJsonObject>* Own = nullptr;
    if (Result->TryGetObjectField(TEXT("undo"), Own) && Own)
    {
        bool bClaimed = false;
        // The handler's word stands unless it names a step the stack does not hold.
        if (!(*Own)->TryGetBoolField(TEXT("undoable"), bClaimed) || !bClaimed || Kept.Num() > 0)
        {
            return;
        }
    }
    else if (Kept.Num() == 0 && !bCleared)
    {
        return; // nothing recorded and nothing claimed
    }
    WriteBlock(Result, Kept, bCleared);
}
} // namespace McpUndoLedger
