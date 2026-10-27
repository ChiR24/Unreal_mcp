#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpFoliagePrivate.h"

// get_foliage_instances. With no type it listed every instance of every type, so a
// meadow of 573 instances came back as ~70 KB to answer "what foliage is here". The
// reply now always counts per type (byType); summary drops the instance list and
// limit caps it (truncated says so; count stays the total).
bool UEngineWireSubsystem::HandleGetFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString FoliageTypePath;
  if (!McpFoliageHandlers::ReadFoliageTypePath(*this, RequestId, RequestingSocket, Payload, FoliageTypePath)) {
    return true;
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
  const TArray<AInstancedFoliageActor *> FoliageActors = McpFoliageHandlers::AllFoliageActors(World);
  UFoliageType *OnlyType = FoliageTypePath.IsEmpty()
      ? nullptr : LoadObject<UFoliageType>(nullptr, *FoliageTypePath, nullptr, LOAD_NoWarn);
  // A typo'd type read the same as a real one with no instances; remove_foliage refuses it.
  if (!FoliageTypePath.IsEmpty() && !OnlyType) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Foliage type not found: %s"), *FoliageTypePath),
                        TEXT("FOLIAGE_TYPE_NOT_FOUND"));
    return true;
  }
  TArray<TSharedPtr<FJsonValue>> InstancesArray;
  // Counts per type across every foliage actor (one per loaded level or World Partition cell), in first-seen order.
  TArray<UFoliageType *> TypeOrder;
  TMap<UFoliageType *, int32> TypeCounts;
  int32 Total = 0;
  const auto AddInstance = [&](UFoliageType *Type, const FFoliageInstance &Inst) {
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
      // The whole scale: one number (the X axis) hid a non-uniform scale (a tall thin tree read as uniform).
      TSharedPtr<FJsonObject> Scale = MakeShared<FJsonObject>();
      Scale->SetNumberField(TEXT("x"), Inst.DrawScale3D.X);
      Scale->SetNumberField(TEXT("y"), Inst.DrawScale3D.Y);
      Scale->SetNumberField(TEXT("z"), Inst.DrawScale3D.Z);
      InstObj->SetObjectField(TEXT("scale"), Scale);
    }
    InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
  };
  for (AInstancedFoliageActor *IFA : FoliageActors) {
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      if (OnlyType && Type != OnlyType) {
        return true;
      }
      if (!TypeCounts.Contains(Type)) {
        TypeOrder.Add(Type);
      }
      TypeCounts.FindOrAdd(Type) += Info.Instances.Num();
      for (const FFoliageInstance &Inst : Info.Instances) {
        ++Total;
        if (!bSummary && InstancesArray.Num() < Limit) {
          AddInstance(Type, Inst);
        }
      }
      return true;
    });
  }
  TArray<TSharedPtr<FJsonValue>> ByType;
  for (UFoliageType *Type : TypeOrder) {
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("foliageType"), Type->GetPathName());
    Row->SetNumberField(TEXT("count"), TypeCounts[Type]);
    ByType.Add(MakeShared<FJsonValueObject>(Row));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  if (!bSummary) {
    Resp->SetArrayField(TEXT("instances"), InstancesArray);
    Resp->SetBoolField(TEXT("truncated"), InstancesArray.Num() < Total);
  }
  Resp->SetNumberField(TEXT("count"), Total);
  Resp->SetArrayField(TEXT("byType"), ByType);
  Resp->SetNumberField(TEXT("foliageActors"), FoliageActors.Num());
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("%d foliage instance(s) across %d type(s)"), Total, ByType.Num()),
                         Resp, FString());
  return true;
}
