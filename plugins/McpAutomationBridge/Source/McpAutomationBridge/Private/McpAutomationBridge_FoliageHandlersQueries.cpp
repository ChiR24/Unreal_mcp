#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_FoliageHandlersPrivate.h"

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

  if (bRemoveAll) {
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      RemovedCount += Info.Instances.Num();
      Info.Instances.Empty();
      return true;
    });
    IFA->Modify();
  } else if (!FoliageTypePath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
      UFoliageType *FoliageType =
          LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
      if (FoliageType) {
        FFoliageInfo *Info = IFA->FindInfo(FoliageType);
        if (Info) {
          RemovedCount = Info->Instances.Num();
          Info->Instances.Empty();
          IFA->Modify();
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
