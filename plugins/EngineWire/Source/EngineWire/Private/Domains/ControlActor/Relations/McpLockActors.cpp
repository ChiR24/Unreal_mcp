// control_actor lock_actors: Lock Actor Movement on or off for the actors named, as the editor's Transform menu sets
// it. A locked actor cannot be dragged in the viewport, and fix_placement, arrange_by_relations and settle_physics leave
// it where it is (McpPlacementFix::PinnedReason); set_transform still moves it. Every name is found before anything
// changes, so a refusal changes nothing; the actors whose lock changes are one undo step (none when every actor already
// is as asked). The flag lives on the actor in every 5.x; 5.8's editor data storage reads it back when the actor is
// modified, and the viewport gathers its gizmo targets again so a selected actor's gizmo follows at once.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "LevelEditorViewport.h"

namespace McpRelations
{
bool HandleLockActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    const bool bLock = GetJsonBoolField(Payload, TEXT("locked"), true);
    TArray<AActor*> Actors;
    for (const FString& Name : Names)
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            Bridge->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("No actor is labelled or named %s. Nothing was changed."), *Name), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Actors.AddUnique(Actor);
    }
    TArray<UObject*> Changing;
    TArray<bool> Was;
    for (AActor* Actor : Actors)
    {
        Was.Add(Actor->IsLockLocation());
        if (Was.Last() != bLock)
        {
            Changing.Add(Actor);
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // The undo step covers only actors whose lock changes: one already as asked would make a step that changes nothing.
    if (Changing.Num() > 0)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(bLock ? TEXT("Lock Actor Movement") : TEXT("Unlock Actor Movement")),
            EMcpMutationDurability::EditorStateOnly, Changing);
        for (UObject* Object : Changing)
        {
            AActor* Actor = CastChecked<AActor>(Object);
            Actor->Modify();
            Actor->SetLockLocation(bLock);
        }
        Transaction.DescribeInto(Result);
        if (GCurrentLevelEditingViewportClient)
        {
            GCurrentLevelEditingViewportClient->GetElementsToManipulate(true);
        }
        GEditor->RedrawLevelEditingViewports(false);
    }
    // Read back: an actor inside a level instance that is not being edited reads locked whatever its own flag says.
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> NotAsAsked;
    int32 Changed = 0;
    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        const bool bNow = Actors[Index]->IsLockLocation();
        Changed += bNow != Was[Index] ? 1 : 0;
        if (bNow != bLock)
        {
            NotAsAsked.Add(Actors[Index]->GetActorLabel());
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Actors[Index]->GetActorLabel());
        Row->SetBoolField(TEXT("locked"), bNow);
        Row->SetBoolField(TEXT("changed"), bNow != Was[Index]);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("actors"), Rows);
    Result->SetBoolField(TEXT("locked"), bLock);
    Result->SetNumberField(TEXT("changed"), Changed);
    const TCHAR* State = bLock ? TEXT("locked") : TEXT("unlocked");
    FString Message = Changed > 0 ? FString::Printf(TEXT("%d of %d actor(s) %s."), Changed, Actors.Num(), State)
        : NotAsAsked.Num() == 0 ? FString::Printf(TEXT("Nothing changed: the %d actor(s) already were %s."), Actors.Num(), State)
        : FString(TEXT("Nothing changed."));
    if (NotAsAsked.Num() > 0)
    {
        Result->SetArrayField(TEXT("notAsAsked"), McpHandlerUtils::ToJsonStringArray(NotAsAsked));
        Message += FString::Printf(TEXT(" %d still read %s: an actor inside a level instance that is not being edited stays locked."),
            NotAsAsked.Num(), bLock ? TEXT("unlocked") : TEXT("locked"));
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
} // namespace McpRelations
