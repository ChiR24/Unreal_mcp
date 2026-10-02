#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"
#include "Foundation/McpScopedEditorTransaction.h"

// The slots a component can take a material in. A dynamic mesh names each triangle's slot by its material id, but
// its component starts with one slot however many ids an SDF or set_material_id gave it, and its SetMaterial grows
// the list on demand: so the ids its mesh uses count as slots it has (an SDF head's teeth in slot 2).
static int32 McpTakeableSlots(UPrimitiveComponent *Component) {
  int32 Slots = Component->GetNumMaterials();
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
  if (UDynamicMeshComponent *Dynamic = Cast<UDynamicMeshComponent>(Component)) {
    Slots = FMath::Max(Slots, McpGeometryHandlers::ConversionSlotCount(Dynamic->GetDynamicMesh()));
  }
#endif
  return Slots;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // actorNames: one material onto many actors in one call (every step of a
  // staircase was a call of its own). Each actor runs through this handler under
  // a captured id, so it behaves exactly like a single set_material.
  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), Names) && Names->Num() > 0) {
    constexpr int32 MaxActors = 500;
    if (Names->Num() > MaxActors) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                FString::Printf(TEXT("actorNames takes at most %d actors"), MaxActors), nullptr);
      return true;
    }
    // One undo step for the whole list, as set_visibility's: the actors that resolve and their primitive
    // components go into one transaction, and each actor's run below (captured) leaves its own to this one.
    TArray<UObject *> Undoable;
    for (const TSharedPtr<FJsonValue> &Named : *Names) {
      if (AActor *Actor = Named.IsValid() ? FindActorByName(Named->AsString()) : nullptr) {
        McpAddActorUndoSet(Actor, Undoable);
      }
    }
    TUniquePtr<FMcpScopedEditorTransaction> Transaction;
    if (Undoable.Num() > 0) {
      Transaction = MakeUnique<FMcpScopedEditorTransaction>(
          FText::FromString(TEXT("Set Actor Material")), EMcpMutationDurability::EditorStateOnly, Undoable);
    }
    FMcpResponseCaptureRegistry &Capture = FMcpResponseCaptureRegistry::Get();
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failures;
    // The actors that took the material, each by the name its one-actor run replied with (the label unless
    // another actor shares it), once each: what the receipt lists as changes and gives an actor handle apiece.
    TArray<FString> Affected;
    for (int32 Index = 0; Index < Names->Num(); ++Index) {
      const FString Name = (*Names)[Index].IsValid() ? (*Names)[Index]->AsString() : FString();
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      One->Values = Payload->Values;
      One->RemoveField(TEXT("actorNames"));
      One->SetStringField(TEXT("actorName"), Name);
      const FString ItemId = FString::Printf(TEXT("%s#%d"), *RequestId, Index);
      Capture.Begin(ItemId);
      HandleControlActorSetMaterial(ItemId, One, Socket);
      const FMcpCapturedResponse Reply = Capture.End(ItemId);
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("actorName"), Name);
      Entry->SetBoolField(TEXT("applied"), Reply.bSuccess);
      if (Reply.bSuccess) {
        FString Changed;
        if (!Reply.Result.IsValid() || !Reply.Result->TryGetStringField(TEXT("actorName"), Changed) || Changed.IsEmpty()) {
          Changed = Name;
        }
        Affected.AddUnique(Changed);
      } else {
        Entry->SetStringField(TEXT("error"), Reply.Message);
        Failures.Add(FString::Printf(TEXT("%s: %s"), *Name, *Reply.Message));
      }
      Results.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const int32 Applied = Results.Num() - Failures.Num();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("results"), Results);
    Data->SetNumberField(TEXT("applied"), Applied);
    Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));
    if (Transaction) {
      Transaction->DescribeInto(Data);
    }
    if (Failures.Num() > 0) {
      SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Material set on %d of %d actors; %s"), Applied,
                                             Results.Num(), *FString::Join(Failures, TEXT("; "))),
                             Data, TEXT("MATERIAL_BATCH_INCOMPLETE"));
    } else {
      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Material set on %d actors"), Applied), Data);
    }
    return true;
  }

  // materials: entry i into slot i ("" skips one), so an SDF part with six slots is one call, not six.
  // Each slot runs through this handler under a captured id, as the actorNames form does.
  const TArray<TSharedPtr<FJsonValue>> *SlotMaterials = nullptr;
  if (Payload->TryGetArrayField(TEXT("materials"), SlotMaterials) && SlotMaterials->Num() > 0) {
    TArray<UObject *> Undoable; // one undo step for every slot; a captured run (actorNames) leaves it to its caller
    AActor *Owner = FindActorByName(GetJsonStringField(Payload, TEXT("actorName")));
    if (Owner && !FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) McpAddActorUndoSet(Owner, Undoable);
    TUniquePtr<FMcpScopedEditorTransaction> Transaction(Undoable.Num() == 0 ? nullptr : new FMcpScopedEditorTransaction(
        FText::FromString(TEXT("Set Actor Material")), EMcpMutationDurability::EditorStateOnly, Undoable));
    TArray<TSharedPtr<FJsonValue>> Slots;
    TArray<FString> Failures;
    for (int32 Slot = 0; Slot < SlotMaterials->Num(); ++Slot) {
      const FString Path = (*SlotMaterials)[Slot].IsValid() ? (*SlotMaterials)[Slot]->AsString() : FString();
      if (Path.IsEmpty()) continue;
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      One->Values = Payload->Values;
      One->RemoveField(TEXT("materials"));
      One->SetStringField(TEXT("materialPath"), Path);
      One->SetNumberField(TEXT("materialSlot"), Slot);
      const FString ItemId = FString::Printf(TEXT("%s#slot%d"), *RequestId, Slot);
      FMcpResponseCaptureRegistry::Get().Begin(ItemId);
      HandleControlActorSetMaterial(ItemId, One, Socket);
      const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(ItemId);
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetNumberField(TEXT("materialSlot"), Slot);
      Entry->SetStringField(TEXT("materialPath"), Path);
      Entry->SetBoolField(TEXT("applied"), Reply.bSuccess);
      if (!Reply.bSuccess) Failures.Add(FString::Printf(TEXT("slot %d: %s"), Slot, *Reply.Message));
      Slots.Add(MakeShared<FJsonValueObject>(Entry));
    }
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("slots"), Slots);
    if (Owner) Data->SetStringField(TEXT("actorName"), McpActorRef(Owner));
    if (Transaction) Transaction->DescribeInto(Data);
    SendAutomationResponse(Socket, RequestId, Failures.Num() == 0, Failures.Num() == 0
        ? FString::Printf(TEXT("Materials set on %d slots"), Slots.Num()) : FString::Join(Failures, TEXT("; ")),
        Data, Failures.Num() == 0 ? FString() : FString(TEXT("MATERIAL_BATCH_INCOMPLETE")));
    return true;
  }

  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), TargetName);
  }
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString MaterialPath;
  Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
  if (MaterialPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), MaterialPath);
  }
  if (MaterialPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("material"), MaterialPath);
  }
  if (MaterialPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("materialPath required"), nullptr);
    return true;
  }

  double SlotNumber = 0.0;
  if (!Payload->TryGetNumberField(TEXT("materialSlot"), SlotNumber)) {
    if (!Payload->TryGetNumberField(TEXT("materialIndex"), SlotNumber)) {
      Payload->TryGetNumberField(TEXT("slotIndex"), SlotNumber);
    }
  }
  if (SlotNumber < 0.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("materialSlot must be non-negative"), nullptr);
    return true;
  }
  const int32 MaterialSlot = static_cast<int32>(SlotNumber);

  FString ResolvedMaterialPath;
  FString LoadError;
  UMaterialInterface *Material =
      LoadMaterialForMcp(MaterialPath, ResolvedMaterialPath, LoadError);
  if (!Material) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("MATERIAL_NOT_FOUND"),
                              LoadError, nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  TArray<UPrimitiveComponent *> TargetComponents;
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (!ComponentName.IsEmpty()) {
    UActorComponent *Component = FindComponentByName(Found, ComponentName);
    UPrimitiveComponent *PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
    if (!PrimitiveComponent) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                                TEXT("Primitive component not found"), nullptr);
      return true;
    }
    TargetComponents.Add(PrimitiveComponent);
  } else {
    Found->GetComponents<UPrimitiveComponent>(TargetComponents);
  }

  if (TargetComponents.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Actor has no primitive components"), nullptr);
    return true;
  }

  bool bAllComponents = false;
  Payload->TryGetBoolField(TEXT("allComponents"), bAllComponents);

  // The components that take the slot (the first one unless allComponents), settled before anything changes,
  // so a call that changes nothing opens no undo step.
  TArray<UPrimitiveComponent *> Takers;
  for (UPrimitiveComponent *Component : TargetComponents) {
    if (Component && MaterialSlot < McpTakeableSlots(Component)) {
      Takers.Add(Component);
      if (!bAllComponents) {
        break;
      }
    }
  }
  if (Takers.Num() == 0) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("MATERIAL_SLOT_NOT_FOUND"),
        FString::Printf(TEXT("No primitive components expose material slot %d"), MaterialSlot),
        nullptr);
    return true;
  }

  // One undo step for the assignment, as set_visibility's: the actor and every primitive component, each flagged
  // RF_Transactional first. A run inside a batch (captured) leaves the step to its caller: the actorNames form
  // holds one for every actor, and spawn_batch's material was never undoable.
  TUniquePtr<FMcpScopedEditorTransaction> Transaction;
  if (!FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) {
    TArray<UObject *> Undoable;
    McpAddActorUndoSet(Found, Undoable);
    Transaction = MakeUnique<FMcpScopedEditorTransaction>(
        FText::FromString(TEXT("Set Actor Material")), EMcpMutationDurability::EditorStateOnly, Undoable);
  }

  TArray<TSharedPtr<FJsonValue>> AppliedComponents;
  for (UPrimitiveComponent *Component : Takers) {
    const int32 MaterialCount = McpTakeableSlots(Component);
    Component->Modify();
    Component->SetMaterial(MaterialSlot, Material);
    // Contingency (UE 5.7): UDynamicMeshComponent::SetMaterial routes
    // into mesh material attributes, not OverrideMaterials. Mirror the
    // assignment so get_component_property(OverrideMaterials) read-back agrees.
    if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Component)) {
      if (MeshComp->OverrideMaterials.IsValidIndex(MaterialSlot)) {
        MeshComp->OverrideMaterials[MaterialSlot] = Material;
      } else {
        const int32 NewSize = FMath::Max(MeshComp->OverrideMaterials.Num(), MaterialSlot + 1);
        MeshComp->OverrideMaterials.SetNumZeroed(NewSize);
        MeshComp->OverrideMaterials[MaterialSlot] = Material;
      }
    }
    Component->MarkRenderStateDirty();
    Component->MarkPackageDirty();

    TSharedPtr<FJsonObject> ComponentObj = McpHandlerUtils::CreateResultObject();
    ComponentObj->SetStringField(TEXT("name"), Component->GetName());
    ComponentObj->SetStringField(TEXT("path"), Component->GetPathName());
    ComponentObj->SetNumberField(TEXT("materialSlots"), MaterialCount);
    AppliedComponents.Add(MakeShared<FJsonValueObject>(ComponentObj));
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Data->SetStringField(TEXT("actorPath"), Found->GetPathName());
  Data->SetStringField(TEXT("materialPath"), Material->GetPathName());
  Data->SetStringField(TEXT("resolvedMaterialPath"), ResolvedMaterialPath);
  Data->SetNumberField(TEXT("materialSlot"), MaterialSlot);
  Data->SetArrayField(TEXT("components"), AppliedComponents);
  if (Transaction) {
    Transaction->DescribeInto(Data);
  }
  McpHandlerUtils::AddVerification(Data, Found);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor material set"), Data);
  return true;
}
