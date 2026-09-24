#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
    FMcpResponseCaptureRegistry &Capture = FMcpResponseCaptureRegistry::Get();
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failures;
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
      if (!Reply.bSuccess) {
        Entry->SetStringField(TEXT("error"), Reply.Message);
        Failures.Add(FString::Printf(TEXT("%s: %s"), *Name, *Reply.Message));
      }
      Results.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const int32 Applied = Results.Num() - Failures.Num();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("results"), Results);
    Data->SetNumberField(TEXT("applied"), Applied);
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

  TArray<TSharedPtr<FJsonValue>> AppliedComponents;
  for (UPrimitiveComponent *Component : TargetComponents) {
    if (!Component) {
      continue;
    }

    const int32 MaterialCount = Component->GetNumMaterials();
    if (MaterialSlot >= MaterialCount) {
      continue;
    }

    Component->Modify();
    Component->SetMaterial(MaterialSlot, Material);
    // BB-022 contingency (UE 5.7): UDynamicMeshComponent::SetMaterial routes
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

    if (!bAllComponents) {
      break;
    }
  }

  if (AppliedComponents.Num() == 0) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("MATERIAL_SLOT_NOT_FOUND"),
        FString::Printf(TEXT("No primitive components expose material slot %d"), MaterialSlot),
        nullptr);
    return true;
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Found->GetPathName());
  Data->SetStringField(TEXT("materialPath"), Material->GetPathName());
  Data->SetStringField(TEXT("resolvedMaterialPath"), ResolvedMaterialPath);
  Data->SetNumberField(TEXT("materialSlot"), MaterialSlot);
  Data->SetArrayField(TEXT("components"), AppliedComponents);
  McpHandlerUtils::AddVerification(Data, Found);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor material set"), Data);
  return true;
#else
  return false;
#endif
}
