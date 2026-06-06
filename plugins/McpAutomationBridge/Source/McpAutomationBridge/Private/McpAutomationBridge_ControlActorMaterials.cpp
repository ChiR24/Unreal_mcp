#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
