// control_editor set_selected_actors: which actors the editor has selected, as clicking them in the viewport or the Outliner
// sets it (the Details panel follows), by label, name or path. mode replace (the default) makes the named actors the
// selection, add adds them, remove takes them off; replace with no names clears it. Every name is found before the
// selection changes, so a refusal changes nothing. A change is one undo step, as a click in the viewport is (the
// editor's selection is transactional); a call that leaves the selection as it was opens none.

#include "Domains/ControlEditor/Selection/McpEditorSelection.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"

namespace McpEditorSelection
{
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    const FString Mode = GetJsonStringField(Payload, TEXT("mode"), TEXT("replace")).ToLower();
    const bool bReplace = Mode == TEXT("replace");
    const bool bSelect = Mode != TEXT("remove");
    if (!(bReplace || Mode == TEXT("add") || Mode == TEXT("remove")) || (!bReplace && Names.Num() == 0))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("mode is replace, add or remove; add and remove name the actors in actorNames. "
            "The selection is as it was."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    TArray<AActor*> Actors;
    for (const FString& Name : Names)
    {
        AActor* Actor = FindActorByNameInWorldForMcp(World, Name, true);
        if (!Actor)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor is labelled or named %s. The selection is as it "
                "was."), *Name), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Actors.AddUnique(Actor);
    }
    USelection* Selection = GEditor->GetSelectedActors();
    TArray<AActor*> Before;
    Selection->GetSelectedObjects(Before);
    // What the selection becomes: a call that leaves it as it is opens no undo step.
    TSet<AActor*> Wanted;
    for (AActor* Actor : Before)
    {
        if (!bReplace)
        {
            Wanted.Add(Actor);
        }
    }
    for (AActor* Actor : Actors)
    {
        if (bSelect)
        {
            Wanted.Add(Actor);
        }
        else
        {
            Wanted.Remove(Actor);
        }
    }
    const bool bChanges = Wanted.Num() != Before.Num() || Before.ContainsByPredicate([&Wanted](AActor* Actor) { return !Wanted.Contains(Actor); });
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Declined;
    if (bChanges)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Select Actors")), EMcpMutationDurability::EditorStateOnly,
            TArray<UObject*>{Selection});
        if (bReplace)
        {
            // Actors only: deselecting BSP surfaces too would modify each level's model and mark the level unsaved.
            GEditor->SelectNone(false, false, false);
        }
        for (AActor* Actor : Actors)
        {
            GEditor->SelectActor(Actor, bSelect, false, true);
            if (Selection->IsSelected(Actor) != bSelect)
            {
                Declined.Add(MakeShared<FJsonValueString>(Actor->GetActorLabel()));
            }
        }
        GEditor->NoteSelectionChange();
        Transaction.DescribeInto(Result);
    }
    TArray<AActor*> After;
    Selection->GetSelectedObjects(After);
    TArray<TSharedPtr<FJsonValue>> Labels;
    for (AActor* Actor : After)
    {
        if (Actor && Labels.Num() < 100)
        {
            Labels.Add(MakeShared<FJsonValueString>(Actor->GetActorLabel()));
        }
    }
    Result->SetArrayField(TEXT("selected"), Labels);
    Result->SetNumberField(TEXT("selectedCount"), After.Num());
    if (Declined.Num() > 0)
    {
        // The editor keeps some out of its selection: an actor in a locked level, or one it selects through its parent.
        Result->SetArrayField(TEXT("notSelected"), Declined);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, bChanges ? FString::Printf(TEXT("%d actor(s) selected."), After.Num())
        : FString::Printf(TEXT("The selection already was that: %d actor(s) selected."), After.Num()), Result);
    return true;
}
} // namespace McpEditorSelection
