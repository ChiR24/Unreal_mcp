#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeCreation.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_landscape"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_landscape payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  double X = 0.0, Y = 0.0, Z = 0.0;
  if (!Payload->TryGetNumberField(TEXT("x"), X) ||
      !Payload->TryGetNumberField(TEXT("y"), Y) ||
      !Payload->TryGetNumberField(TEXT("z"), Z)) {
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Z);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *LocArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
          LocArr->Num() >= 3) {
        X = (*LocArr)[0]->AsNumber();
        Y = (*LocArr)[1]->AsNumber();
        Z = (*LocArr)[2]->AsNumber();
      }
    }
  }

  McpLandscapeCreation::FLandscapeCreationRequest Request;
  bool bHasCX = Payload->TryGetNumberField(TEXT("componentsX"),
                                           Request.ComponentsX);
  bool bHasCY = Payload->TryGetNumberField(TEXT("componentsY"),
                                           Request.ComponentsY);

  int32 ComponentCount = 0;
  const TSharedPtr<FJsonObject> *ComponentCountObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("componentCount"), ComponentCountObj) &&
      ComponentCountObj) {
    double ComponentCountX = 0.0, ComponentCountY = 0.0;
    if (!bHasCX &&
        (*ComponentCountObj)->TryGetNumberField(TEXT("x"), ComponentCountX)) {
      Request.ComponentsX =
          FMath::Max(1, static_cast<int32>(ComponentCountX));
      bHasCX = true;
    }
    if (!bHasCY &&
        (*ComponentCountObj)->TryGetNumberField(TEXT("y"), ComponentCountY)) {
      Request.ComponentsY =
          FMath::Max(1, static_cast<int32>(ComponentCountY));
      bHasCY = true;
    }
  } else {
    Payload->TryGetNumberField(TEXT("componentCount"), ComponentCount);
  }
  if (!bHasCX && ComponentCount > 0) {
    Request.ComponentsX = ComponentCount;
  }
  if (!bHasCY && ComponentCount > 0) {
    Request.ComponentsY = ComponentCount;
  }

  double SizeXUnits = 0.0, SizeYUnits = 0.0;
  if (Payload->TryGetNumberField(TEXT("sizeX"), SizeXUnits) && SizeXUnits > 0 &&
      !bHasCX) {
    Request.ComponentsX =
        FMath::Max(1, static_cast<int32>(FMath::Floor(SizeXUnits / 1000.0)));
  }
  if (Payload->TryGetNumberField(TEXT("sizeY"), SizeYUnits) && SizeYUnits > 0 &&
      !bHasCY) {
    Request.ComponentsY =
        FMath::Max(1, static_cast<int32>(FMath::Floor(SizeYUnits / 1000.0)));
  }

  if (!Payload->TryGetNumberField(TEXT("quadsPerComponent"),
                                  Request.QuadsPerComponent)) {
    if (!Payload->TryGetNumberField(TEXT("quadsPerSection"),
                                    Request.QuadsPerComponent)) {
      Payload->TryGetNumberField(TEXT("sectionSize"),
                                 Request.QuadsPerComponent);
    }
  }
  Payload->TryGetNumberField(TEXT("sectionsPerComponent"),
                             Request.SectionsPerComponent);

  Payload->TryGetStringField(TEXT("materialPath"), Request.MaterialPath);
  if (Request.MaterialPath.IsEmpty()) {
    Request.MaterialPath = TEXT("/Engine/EngineMaterials/WorldGridMaterial");
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  if (!Payload->TryGetStringField(TEXT("name"), Request.Name) ||
      Request.Name.IsEmpty()) {
    Payload->TryGetStringField(TEXT("landscapeName"), Request.Name);
  }
  if (Request.Name.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("name or landscapeName parameter is required for create_landscape"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Request.Name.Contains(TEXT("/")) || Request.Name.Contains(TEXT("\\")) ||
      Request.Name.Contains(TEXT(":")) || Request.Name.Contains(TEXT("*")) ||
      Request.Name.Contains(TEXT("?")) || Request.Name.Contains(TEXT("\"")) ||
      Request.Name.Contains(TEXT("<")) || Request.Name.Contains(TEXT(">")) ||
      Request.Name.Contains(TEXT("|"))) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("name contains invalid characters (/, \\, :, *, ?, \", <, >, |)"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Request.Name.Len() > 128) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("name exceeds maximum length of 128 characters"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Request.Location = FVector(X, Y, Z);
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, RequestId, RequestingSocket, Request]() {
              UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
              if (!Subsystem)
                return;
              McpLandscapeCreation::CreateLandscapeOnGameThread(
                  *Subsystem, RequestId, RequestingSocket, Request);
            });
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_landscape requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
