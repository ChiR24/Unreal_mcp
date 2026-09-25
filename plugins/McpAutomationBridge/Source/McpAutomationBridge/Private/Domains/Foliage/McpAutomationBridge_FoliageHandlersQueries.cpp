#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleRemoveFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_foliage"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("remove_foliage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

  if (!FoliageTypePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FoliageTypePath = SafePath;
  }

  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  bool bRemoveAll = false;
  Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AInstancedFoliageActor *IFA =
      McpFoliageHandlers::GetOrCreateFoliageActorForWorldSafe(World, false);
  if (!IFA) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No foliage actor found"),
                        TEXT("FOLIAGE_ACTOR_NOT_FOUND"));
    return true;
  }

  int32 RemovedCount = 0;

  // Carving a pit or clearing a path needs only the instances inside one box
  // gone, and the only choices were a whole type or everything. Removal takes the
  // same `area` box paint does (all three axes), over every type or the named one.
  // `areas` takes several boxes under one consent: four pits were four calls.
  TArray<FBox> Boxes;
  auto AddBox = [&Boxes](const TSharedPtr<FJsonObject> &Area) {
    if (Area.IsValid() && Area->HasField(TEXT("min")) && Area->HasField(TEXT("max"))) {
      FVector AreaMin = FVector::ZeroVector, AreaMax = FVector::ZeroVector;
      ReadVectorField(Area, TEXT("min"), AreaMin, FVector::ZeroVector);
      ReadVectorField(Area, TEXT("max"), AreaMax, FVector::ZeroVector);
      Boxes.Add(FBox(AreaMin.ComponentMin(AreaMax), AreaMin.ComponentMax(AreaMax)));
    }
  };
  const TSharedPtr<FJsonObject> *AreaObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("area"), AreaObj) && AreaObj) {
    AddBox(*AreaObj);
  }
  const TArray<TSharedPtr<FJsonValue>> *AreaList = nullptr;
  if (Payload->TryGetArrayField(TEXT("areas"), AreaList)) {
    for (const TSharedPtr<FJsonValue> &Area : *AreaList) {
      if (Area.IsValid() && Area->Type == EJson::Object) {
        AddBox(Area->AsObject());
      }
    }
  }
  if (Boxes.Num() > 0) {
    UFoliageType *OnlyType = FoliageTypePath.IsEmpty()
        ? nullptr : LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (!FoliageTypePath.IsEmpty() && !OnlyType) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Foliage type not found: %s"), *FoliageTypePath),
                          TEXT("FOLIAGE_TYPE_NOT_FOUND"));
      return true;
    }
    IFA->Modify();
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      TArray<int32> Inside;
      for (int32 Index = 0; (!OnlyType || Type == OnlyType) && Index < Info.Instances.Num(); ++Index) {
        const FVector Location(Info.Instances[Index].Location);
        if (Boxes.ContainsByPredicate([&Location](const FBox &Box) { return Box.IsInsideOrOn(Location); })) {
          Inside.Add(Index);
        }
      }
      if (Inside.Num() > 0) {
        Info.RemoveInstances(Inside, true);
        RemovedCount += Inside.Num();
      }
      return true;
    });
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);
    Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
    Resp->SetBoolField(TEXT("existsAfter"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Removed %d foliage instances inside %d area(s)"),
                                           RemovedCount, Boxes.Num()),
                           Resp, FString());
    return true;
  }

  // Emptying FFoliageInfo::Instances left every rendered instance in its component,
  // out of step with the list the next add appends to; RemoveFoliageType takes the
  // instances, their components and the type out together.
  IFA->Modify();
  if (bRemoveAll) {
    TArray<UFoliageType *> Types;
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      RemovedCount += Info.Instances.Num();
      Types.Add(Type);
      return true;
    });
    IFA->RemoveFoliageType(Types.GetData(), Types.Num());
  } else if (!FoliageTypePath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
      UFoliageType *FoliageType =
          LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
      if (FoliageType) {
        if (FFoliageInfo *Info = IFA->FindInfo(FoliageType)) {
          RemovedCount = Info->Instances.Num();
          IFA->RemoveFoliageType(&FoliageType, 1);
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage removed successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("remove_foliage requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
bool UMcpAutomationBridgeSubsystem::HandleGetFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_foliage_instances"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_foliage_instances payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

  if (!FoliageTypePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FoliageTypePath = SafePath;
  }

  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AInstancedFoliageActor *IFA =
      McpFoliageHandlers::GetOrCreateFoliageActorForWorldSafe(World, false);
  if (!IFA) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No foliage actor found"), Resp, FString());
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> InstancesArray;

  if (!FoliageTypePath.IsEmpty()) {
    if (!UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Foliage type not found, 0 instances"), Resp,
                             FString());
      return true;
    }

    UFoliageType *FoliageType =
        LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (FoliageType) {
      FFoliageInfo *Info = IFA->FindInfo(FoliageType);
      if (Info) {
        for (const FFoliageInstance &Inst : Info->Instances) {
          TSharedPtr<FJsonObject> InstObj = McpHandlerUtils::CreateResultObject();
          InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
          InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
          InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
          InstObj->SetNumberField(TEXT("pitch"), Inst.Rotation.Pitch);
          InstObj->SetNumberField(TEXT("yaw"), Inst.Rotation.Yaw);
          InstObj->SetNumberField(TEXT("roll"), Inst.Rotation.Roll);
          InstObj->SetNumberField(TEXT("scale"), Inst.DrawScale3D.X);
          InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
        }
      }
    }
  } else {
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      for (const FFoliageInstance &Inst : Info.Instances) {
        TSharedPtr<FJsonObject> InstObj = McpHandlerUtils::CreateResultObject();
        InstObj->SetStringField(TEXT("foliageType"), Type->GetPathName());
        InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
        InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
        InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
        InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
      }
      return true;
    });
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), InstancesArray);
  Resp->SetNumberField(TEXT("count"), InstancesArray.Num());
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage instances retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("get_foliage_instances requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
