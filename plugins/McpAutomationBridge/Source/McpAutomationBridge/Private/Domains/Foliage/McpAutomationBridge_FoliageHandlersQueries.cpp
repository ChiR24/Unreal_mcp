#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleRemoveFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString FoliageTypePath;
  if (!McpFoliageHandlers::ReadFoliageTypePath(*this, RequestId, RequestingSocket, Payload, FoliageTypePath)) {
    return true;
  }

  bool bRemoveAll = false;
  Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
  const bool bHasArea = Payload->HasField(TEXT("area")) || Payload->HasField(TEXT("areas"));
  // With nothing to remove named, or a type that does not exist, this used to
  // remove nothing and still report success.
  if (FoliageTypePath.IsEmpty() && !bRemoveAll && !bHasArea) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Name what to remove: foliageType (or foliageTypePath), area, areas, or removeAll true"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!FoliageTypePath.IsEmpty() && !LoadObject<UFoliageType>(nullptr, *FoliageTypePath, nullptr, LOAD_NoWarn)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Foliage type not found: %s"), *FoliageTypePath),
                        TEXT("FOLIAGE_TYPE_NOT_FOUND"));
    return true;
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
      const FVector AreaMin = ExtractVectorField(Area, TEXT("min"), FVector::ZeroVector);
      const FVector AreaMax = ExtractVectorField(Area, TEXT("max"), FVector::ZeroVector);
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
  } else if (bRemoveAll) {
    // Emptying FFoliageInfo::Instances left every rendered instance in its component,
    // out of step with the list the next add appends to; RemoveFoliageType takes the
    // instances, their components and the type out together.
    IFA->Modify();
    TArray<UFoliageType *> Types;
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      RemovedCount += Info.Instances.Num();
      Types.Add(Type);
      return true;
    });
    IFA->RemoveFoliageType(Types.GetData(), Types.Num());
  } else if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Each area needs both min and max"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  } else {
    UFoliageType *FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (FFoliageInfo *Info = FoliageType ? IFA->FindInfo(FoliageType) : nullptr) {
      IFA->Modify();
      RemovedCount = Info->Instances.Num();
      IFA->RemoveFoliageType(&FoliageType, 1);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         Boxes.Num() > 0 ? FString::Printf(TEXT("Removed %d foliage instances inside %d area(s)"), RemovedCount, Boxes.Num())
                                         : FString(TEXT("Foliage removed successfully")),
                         Resp, FString());
  return true;
}
