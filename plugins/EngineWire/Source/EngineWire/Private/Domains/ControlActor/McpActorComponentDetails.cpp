// Per-component detail enrichment for get_components entries, and the many-actor form of
// set_component_properties.
//
// The component list handler stays under the pure-line ceiling by delegating
// the inspection fields to this sibling file. Everything added here is
// additive: existing readers keep reading name/class/path/relative* untouched,
// and the new fields (class identity, attach parent, visibility/active state)
// only widen the entry.

#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Properties/McpComponentLookup.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "UObject/Class.h"

void McpAppendComponentDetailFields(UActorComponent *Component,
                                    TSharedPtr<FJsonObject> &Entry) {
  if (!Component || !Entry.IsValid()) {
    return;
  }

  // Class identity: display name and full path side by side, since the legacy
  // `class` field only carries the path name.
  UClass *ComponentClass = Component->GetClass();
  if (ComponentClass) {
    Entry->SetStringField(TEXT("className"), ComponentClass->GetName());
    Entry->SetStringField(TEXT("classPath"), ComponentClass->GetPathName());
  }

  Entry->SetBoolField(TEXT("isActive"), Component->IsActive());

  if (USceneComponent *SceneComp = Cast<USceneComponent>(Component)) {
    Entry->SetBoolField(TEXT("isSceneComponent"), true);
    Entry->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
    if (USceneComponent *AttachParent = SceneComp->GetAttachParent()) {
      Entry->SetStringField(TEXT("attachParent"), AttachParent->GetName());
    }
  } else {
    Entry->SetBoolField(TEXT("isSceneComponent"), false);
  }
  // A world actor's mesh and materials took an inspect_object call per actor to find.
  // No property list rides along: the first ten names a class declares, with no values, were most of a
  // 36-component pawn's 30 KB answer; get_component_property and get_component_details read properties.
  McpHandlerUtils::AddMeshAssetFields(Component, Entry);
}

// set_component_properties with actorNames: the same properties on the same component of each actor (a dozen fire
// lights took a call apiece), in one undo step as set_visibility's. Names that matched no actor come back in missing;
// a results row only for an actor whose component did not take everything, which fails the call.
void McpSetComponentPropertiesOnActors(UEngineWireSubsystem &Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                       const FString &RequestId, const TArray<AActor *> &Actors,
                                       const TArray<FString> &Missing, const FString &ComponentName,
                                       const TSharedPtr<FJsonObject> &Properties) {
  if (Actors.Num() == 0) {
    McpSendNoActorNamesFound(&Bridge, Socket, RequestId, Missing);
    return;
  }
  TArray<UActorComponent *> Components;
  TArray<UObject *> Undoable;
  for (AActor *Actor : Actors) {
    UActorComponent *Component = FindComponentByName(Actor, ComponentName);
    Components.Add(Component);
    if (Component) {
      Component->SetFlags(RF_Transactional);
      Undoable.Add(Component);
    }
  }
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Component Properties")),
                                          EMcpMutationDurability::EditorStateOnly, Undoable);
  TArray<TSharedPtr<FJsonValue>> Rows;
  TArray<FString> Updated;
  for (int32 Index = 0; Index < Actors.Num(); ++Index) {
    UActorComponent *Component = Components[Index];
    TArray<FString> Applied, Warnings;
    if (Component) {
      Component->Modify();
      McpApplyComponentProperties(Component, Properties, Applied, Warnings);
      McpRefreshComponentAfterEdit(Component);
      Component->MarkPackageDirty();
    }
    if (Component && Warnings.Num() == 0) {
      Updated.Add(Actors[Index]->GetActorLabel());
      continue;
    }
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    Row->SetStringField(TEXT("actorName"), Actors[Index]->GetActorLabel());
    Row->SetStringField(TEXT("error"), Component ? FString::Join(Warnings, TEXT("; "))
                                                 : FString::Printf(TEXT("no component %s"), *ComponentName));
    Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("updatedActors"), Updated.Num());
  Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Updated));
  if (Missing.Num() > 0) {
    Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
  }
  if (Rows.Num() > 0) {
    Data->SetArrayField(TEXT("results"), Rows);
  }
  Transaction.DescribeInto(Data);
  Bridge.SendAutomationResponse(
      Socket, RequestId, Rows.Num() == 0,
      FString::Printf(TEXT("Component properties updated on %d of %d actors"), Updated.Num(), Actors.Num()), Data,
      Rows.Num() == 0 ? FString() : FString(TEXT("PARTIAL_FAILURE")));
}
