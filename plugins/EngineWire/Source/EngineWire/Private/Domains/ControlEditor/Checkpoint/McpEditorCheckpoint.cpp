#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/Journal/McpChangeJournal.h"
#include "Core/Requests/McpUndoLedger.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Editor/TransBuffer.h"
#include "Editor/Transactor.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace McpEditorCheckpoint
{
namespace // inside the module namespace: a unity blob merges files, and these helper names are common
{
constexpr int32 MaxListed = 200;

struct FCheckpoint
{
    bool bOpen = false;
    FString Label;
    FGuid Marker; // the live step on top at begin; invalid when the stack was empty
    TArray<FString> Added, Removed, Saved;
    TArray<FString> AddedByMcp; // of Added, those a request through this server made
    FDelegateHandle AddedHandle, RemovedHandle, SavedHandle;
};
FCheckpoint GPoint;

UTransBuffer* Buffer()
{
    return GEditor ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
}

void Note(TArray<FString>& List, const FString& Path)
{
    if (Path.StartsWith(TEXT("/Game/")) && List.Num() < MaxListed)
    {
        List.AddUnique(Path);
    }
}

void Unbind()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        IAssetRegistry& Registry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.OnAssetAdded().Remove(GPoint.AddedHandle);
        Registry.OnAssetRemoved().Remove(GPoint.RemovedHandle);
    }
    UPackage::PackageSavedWithContextEvent.Remove(GPoint.SavedHandle);
    GPoint = FCheckpoint();
}

void Bind()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    GPoint.AddedHandle = Registry.OnAssetAdded().AddLambda([](const FAssetData& Asset) {
        Note(GPoint.Added, Asset.PackageName.ToString());
        // An asset the user made by hand, or the registry found on disk, arrives outside any request.
        if (McpChangeJournal::InRequest())
        {
            Note(GPoint.AddedByMcp, Asset.PackageName.ToString());
        }
    });
    GPoint.RemovedHandle = Registry.OnAssetRemoved().AddLambda([](const FAssetData& Asset) {
        Note(GPoint.Removed, Asset.PackageName.ToString());
    });
    GPoint.SavedHandle = UPackage::PackageSavedWithContextEvent.AddLambda(
        [](const FString&, UPackage* Package, FObjectPostSaveContext) {
            if (Package)
            {
                Note(GPoint.Saved, Package->GetName());
            }
        });
}

// The live steps above the marker, newest first. False when the marker is gone: the history was cleared since.
bool StepsSince(const UTransBuffer& Trans, TArray<const FTransaction*>& Out)
{
    for (int32 Index = Trans.GetQueueLength() - Trans.GetUndoCount() - 1; Index >= 0; --Index)
    {
        const FTransaction* Step = Trans.GetTransaction(Index);
        if (!Step || (GPoint.Marker.IsValid() && Step->GetContext().TransactionId == GPoint.Marker))
        {
            return true;
        }
        Out.Add(Step);
    }
    return !GPoint.Marker.IsValid();
}

TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FString& Value : Values)
    {
        Out.Add(MakeShared<FJsonValueString>(Value));
    }
    return Out;
}

void Refuse(UEngineWireSubsystem* Self, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
            const TCHAR* Code, const FString& Message, const TSharedPtr<FJsonObject>& Details = nullptr)
{
    SendStandardErrorResponse(Self, Socket, RequestId, Code, Message, Details);
}

void OpenCheckpoint(UEngineWireSubsystem* Self, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
           const TSharedPtr<FJsonObject>& Payload, UTransBuffer& Trans)
{
    if (GPoint.bOpen)
    {
        Refuse(Self, Socket, RequestId, TEXT("RESTORE_POINT_OPEN"),
               FString::Printf(TEXT("The restore point '%s' is open: roll back to it or end it first."), *GPoint.Label));
        return;
    }
    Unbind();
    GPoint.bOpen = true;
    GPoint.Label = GetJsonStringField(Payload, TEXT("label"), TEXT("restore point"));
    const int32 Live = Trans.GetQueueLength() - Trans.GetUndoCount();
    const FTransaction* Top = Live > 0 ? Trans.GetTransaction(Live - 1) : nullptr;
    GPoint.Marker = Top ? Top->GetContext().TransactionId : FGuid();
    Bind();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("label"), GPoint.Label);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Restore point '%s' opened."), *GPoint.Label),
                                 Result, FString());
}

void Rollback(UEngineWireSubsystem* Self, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
              const TArray<const FTransaction*>& Steps, const FTransaction* Foreign, const bool bIntact,
              const TSharedPtr<FJsonObject>& Result)
{
    if (GEditor->PlayWorld)
    {
        Refuse(Self, Socket, RequestId, TEXT("PIE_RUNNING"), TEXT("Stop Play before rolling back: undo does not reach the running game."));
        return;
    }
    if (!bIntact)
    {
        Refuse(Self, Socket, RequestId, TEXT("UNDO_HISTORY_CLEARED"),
               TEXT("The undo history was cleared after the restore point (a Blueprint compile or a level load clears it), so "
                    "undo cannot reach it. Nothing was undone."), Result);
        return;
    }
    if (Foreign)
    {
        Refuse(Self, Socket, RequestId, TEXT("USER_STEP_SINCE_RESTORE_POINT"),
               FString::Printf(TEXT("'%s' was not made through this server: rolling back would undo it too. Nothing was "
                                    "undone; undo it in the editor first, or end the checkpoint."),
                               *Foreign->GetContext().Title.ToString()), Result);
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Undone;
    for (const FTransaction* Step : Steps)
    {
        const FString Title = Step->GetContext().Title.ToString();
        if (!GEditor->UndoTransaction())
        {
            break;
        }
        Undone.Add(MakeShared<FJsonValueString>(Title));
    }
    Result->SetArrayField(TEXT("undone"), Undone);
    const bool bAll = Undone.Num() == Steps.Num();
    Self->SendAutomationResponse(Socket, RequestId, bAll,
        FString::Printf(TEXT("Undid %d of %d step(s) back to restore point '%s'. Assets created since stay until deleted; "
                             "saved files keep what was saved."), Undone.Num(), Steps.Num(), *GPoint.Label),
        Result, bAll ? FString() : FString(TEXT("UNDO_FAILED")));
}
} // namespace

bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Op = GetJsonStringField(Payload, TEXT("op"), TEXT("status")).ToLower();
    UTransBuffer* Trans = Buffer();
    if (!Trans)
    {
        Refuse(Self, Socket, RequestId, TEXT("UNDO_UNAVAILABLE"), TEXT("The editor has no undo buffer."));
        return true;
    }
    if (Op == TEXT("begin"))
    {
        OpenCheckpoint(Self, Socket, RequestId, Payload, *Trans);
        return true;
    }
    if (Op != TEXT("status") && Op != TEXT("rollback") && Op != TEXT("end"))
    {
        Refuse(Self, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
               FString::Printf(TEXT("Unknown op '%s': begin, status, rollback or end."), *Op));
        return true;
    }
    if (!GPoint.bOpen)
    {
        Refuse(Self, Socket, RequestId, TEXT("NO_RESTORE_POINT"), TEXT("No restore point is open: op begin opens one."));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("label"), GPoint.Label);
    if (Op == TEXT("end"))
    {
        Unbind();
        Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Restore point closed; nothing was undone."), Result, FString());
        return true;
    }
    TArray<const FTransaction*> Steps;
    const bool bIntact = StepsSince(*Trans, Steps);
    TArray<TSharedPtr<FJsonValue>> Rows;
    const FTransaction* Foreign = nullptr;
    for (const FTransaction* Step : Steps)
    {
        const bool bMcp = McpUndoLedger::IsMcpStep(Step->GetContext().TransactionId);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("title"), Step->GetContext().Title.ToString());
        Row->SetBoolField(TEXT("byMcp"), bMcp);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        Foreign = Foreign ? Foreign : (bMcp ? nullptr : Step);
    }
    Result->SetArrayField(TEXT("steps"), Rows);
    Result->SetBoolField(TEXT("historyIntact"), bIntact);
    Result->SetArrayField(TEXT("assetsCreated"), Strings(GPoint.Added));
    Result->SetArrayField(TEXT("assetsRemoved"), Strings(GPoint.Removed));
    Result->SetArrayField(TEXT("packagesSaved"), Strings(GPoint.Saved));
    // Undo deletes no asset: the call that removes what calls through this server created since, ready to send (it asks
    // for consent). Only theirs: an asset the user made is listed in assetsCreated and never offered for deletion.
    const TArray<FString> Created = GPoint.AddedByMcp.FilterByPredicate([](const FString& Path) { return !GPoint.Removed.Contains(Path); });
    if (Created.Num() > 0)
    {
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetArrayField(TEXT("assetPaths"), Strings(Created));
        Params->SetBoolField(TEXT("fixupRedirectors"), false);
        TSharedPtr<FJsonObject> Cleanup = MakeShared<FJsonObject>();
        Cleanup->SetStringField(TEXT("operation"), TEXT("execute"));
        Cleanup->SetStringField(TEXT("tool"), TEXT("manage_asset"));
        Cleanup->SetStringField(TEXT("action"), TEXT("bulk_delete"));
        Cleanup->SetObjectField(TEXT("params"), Params);
        Result->SetObjectField(TEXT("cleanupCall"), Cleanup);
    }
    if (Op == TEXT("rollback"))
    {
        Rollback(Self, Socket, RequestId, Steps, Foreign, bIntact, Result);
        return true;
    }
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%d undo step(s) since restore point '%s'."), Steps.Num(), *GPoint.Label), Result, FString());
    return true;
}

void Shutdown()
{
    Unbind();
}
} // namespace McpEditorCheckpoint
