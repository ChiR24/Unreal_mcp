#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleAddFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString FoliageTypePath;
  if (!McpFoliageHandlers::ReadFoliageTypePath(*this, RequestId, RequestingSocket, Payload, FoliageTypePath)) {
    return true;
  }
  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("foliageType or foliageTypePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  struct FFoliageTransformData {
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
  };
  TArray<FFoliageTransformData> ParsedTransforms;

  const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
  if (Payload->TryGetArrayField(TEXT("transforms"), Transforms) && Transforms) {
    for (const TSharedPtr<FJsonValue> &V : *Transforms) {
      if (!V.IsValid() || V->Type != EJson::Object)
        continue;
      const TSharedPtr<FJsonObject> *TObj = nullptr;
      if (!V->TryGetObject(TObj) || !TObj)
        continue;

      // A transform without a location places nothing; scale falls back to uniformScale, then 1.
      if (!(*TObj)->HasField(TEXT("location")))
        continue;
      FFoliageTransformData TransformData;
      TransformData.Location = ExtractVectorField(*TObj, TEXT("location"), FVector::ZeroVector);
      TransformData.Rotation = ExtractRotatorField(*TObj, TEXT("rotation"), FRotator::ZeroRotator);
      TransformData.Scale = ExtractVectorField(*TObj, TEXT("scale"),
                                               FVector(GetJsonNumberField(*TObj, TEXT("uniformScale"), 1.0)));
      ParsedTransforms.Add(TransformData);
    }
  }

  if (ParsedTransforms.Num() == 0) {
    // A bare location has no scale or rotation of its own, so the advertised
    // minScale/maxScale/randomYaw vary it. They used to be ignored here: every
    // instance came out at scale 1 facing +X, a row of identical clones.
    double MinScale = 1.0, MaxScale = 1.0;
    Payload->TryGetNumberField(TEXT("minScale"), MinScale);
    Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);
    bool bRandomYaw = false;
    Payload->TryGetBoolField(TEXT("randomYaw"), bRandomYaw);
    const TArray<TSharedPtr<FJsonValue>> *LocationsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("locations"), LocationsArray) &&
        LocationsArray) {
      for (const TSharedPtr<FJsonValue> &Val : *LocationsArray) {
        FFoliageTransformData TransformData;
        TransformData.Location = ReadJsonVector(Val, FVector::ZeroVector);
        TransformData.Scale = FVector(FMath::FRandRange(FMath::Min(MinScale, MaxScale), FMath::Max(MinScale, MaxScale)));
        if (bRandomYaw) {
          TransformData.Rotation.Yaw = FMath::FRandRange(0.0, 360.0);
        }
        ParsedTransforms.Add(TransformData);
      }
    }
  }

  if (ParsedTransforms.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("transforms or locations must contain at least one valid location"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UFoliageType *FoliageType = McpFoliageHandlers::ResolveFoliageTypeOrMesh(*this, RequestId, RequestingSocket, FoliageTypePath);
  AInstancedFoliageActor *IFA = FoliageType ? McpFoliageHandlers::RequireFoliageActor(*this, RequestId, RequestingSocket) : nullptr;
  if (!IFA) {
    return true;
  }
  for (const FFoliageTransformData &TransformData : ParsedTransforms) {
    FFoliageInstance Instance;
    Instance.Location = TransformData.Location;
    Instance.Rotation = TransformData.Rotation;
    Instance.DrawScale3D = FVector3f(TransformData.Scale);
    McpFoliageHandlers::AddFoliageInstance(IFA, FoliageType, Instance);
  }
  const int32 Added = ParsedTransforms.Num();
  IFA->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instancesPlaced"), Added);
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetBoolField(TEXT("existsAfter"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage instances added"), Resp, FString());
  return true;
}
