#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

// get_foliage_instances. With no type it listed every instance of every type, so a
// meadow of 573 instances came back as ~70 KB to answer "what foliage is here". The
// reply now always counts per type (byType); summary drops the instance list and
// limit caps it (truncated says so; count stays the total).
bool UMcpAutomationBridgeSubsystem::HandleGetFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Action.Equals(TEXT("get_foliage_instances"), ESearchCase::IgnoreCase)) {
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
    const FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FoliageTypePath = FPaths::GetPath(SafePath).IsEmpty()
        ? FString::Printf(TEXT("/Game/Foliage/%s"), *SafePath) : SafePath;
  }
  bool bSummary = false;
  Payload->TryGetBoolField(TEXT("summary"), bSummary);
  double LimitValue = -1.0;
  const int32 Limit = Payload->TryGetNumberField(TEXT("limit"), LimitValue) && LimitValue >= 0.0
      ? static_cast<int32>(LimitValue) : MAX_int32;

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UWorld *World = GEditor->GetEditorWorldContext().World();
  AInstancedFoliageActor *IFA =
      McpFoliageHandlers::GetOrCreateFoliageActorForWorldSafe(World, false);
  UFoliageType *OnlyType = nullptr;
  if (IFA && !FoliageTypePath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
    OnlyType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
  }
  if (!IFA || (!FoliageTypePath.IsEmpty() && !OnlyType)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
    Resp->SetNumberField(TEXT("count"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           IFA ? TEXT("Foliage type not found, 0 instances") : TEXT("No foliage actor found"),
                           Resp, FString());
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> InstancesArray;
  TArray<TSharedPtr<FJsonValue>> ByType;
  int32 Total = 0;
  IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
    if (OnlyType && Type != OnlyType) {
      return true;
    }
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("foliageType"), Type->GetPathName());
    Row->SetNumberField(TEXT("count"), Info.Instances.Num());
    ByType.Add(MakeShared<FJsonValueObject>(Row));
    for (const FFoliageInstance &Inst : Info.Instances) {
      ++Total;
      if (bSummary || InstancesArray.Num() >= Limit) {
        continue;
      }
      TSharedPtr<FJsonObject> InstObj = McpHandlerUtils::CreateResultObject();
      if (!OnlyType) {
        InstObj->SetStringField(TEXT("foliageType"), Type->GetPathName());
      }
      InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
      InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
      InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
      if (OnlyType) {
        InstObj->SetNumberField(TEXT("pitch"), Inst.Rotation.Pitch);
        InstObj->SetNumberField(TEXT("yaw"), Inst.Rotation.Yaw);
        InstObj->SetNumberField(TEXT("roll"), Inst.Rotation.Roll);
        InstObj->SetNumberField(TEXT("scale"), Inst.DrawScale3D.X);
      }
      InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
    }
    return true;
  });

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  if (!bSummary) {
    Resp->SetArrayField(TEXT("instances"), InstancesArray);
    Resp->SetBoolField(TEXT("truncated"), InstancesArray.Num() < Total);
  }
  Resp->SetNumberField(TEXT("count"), Total);
  Resp->SetArrayField(TEXT("byType"), ByType);
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("%d foliage instance(s) across %d type(s)"), Total, ByType.Num()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("get_foliage_instances requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
