#include "Domains/Landscape/McpLandscapeCreation.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "EngineWireSubsystem.h"

bool UEngineWireSubsystem::HandleCreateLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  McpLandscapeCreation::FLandscapeCreationRequest Request;
  Request.Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  // quadsPerSection is quads per SECTION; a component holds sectionsPerComponent x
  // sectionsPerComponent of them. Storing it as quads per component built 2-section
  // components that disagreed with the imported layout, and 0 sections divided by zero.
  int32 QuadsPerSection = 63;
  if (!Payload->TryGetNumberField(TEXT("quadsPerSection"), QuadsPerSection)) {
    Payload->TryGetNumberField(TEXT("sectionSize"), QuadsPerSection);
  }
  Payload->TryGetNumberField(TEXT("sectionsPerComponent"), Request.SectionsPerComponent);
  if (QuadsPerSection < 7 || QuadsPerSection > 255 || !FMath::IsPowerOfTwo(QuadsPerSection + 1) ||
      (Request.SectionsPerComponent != 1 && Request.SectionsPerComponent != 2)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("quadsPerSection must be 7, 15, 31, 63, 127 or 255 and sectionsPerComponent 1 or 2"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Request.QuadsPerComponent = QuadsPerSection * Request.SectionsPerComponent;

  // componentCount {x, y} wins; otherwise sizeX/sizeY are sizes in quads, so the count is quads / quadsPerComponent.
  const TSharedPtr<FJsonObject> *ComponentCount = nullptr;
  const bool bHasCount = Payload->TryGetObjectField(TEXT("componentCount"), ComponentCount) && ComponentCount;
  const auto Components = [&](const TCHAR *Axis, const TCHAR *SizeField, int32 &Out) {
    double Value = 0.0;
    if (bHasCount && (*ComponentCount)->TryGetNumberField(Axis, Value)) {
      Out = FMath::Max(1, static_cast<int32>(Value));
    } else if (Payload->TryGetNumberField(SizeField, Value) && Value > 0) {
      // A size in vertices like the editor's Overall Resolution (1009 is 16 components of 63 quads) or in quads: the
      // vertex past the last quad needs no extra component, where 1009 used to build 17.
      Out = FMath::Max(1, FMath::DivideAndRoundUp(static_cast<int32>(Value) - 1, Request.QuadsPerComponent));
    }
  };
  Components(TEXT("x"), TEXT("sizeX"), Request.ComponentsX);
  Components(TEXT("y"), TEXT("sizeY"), Request.ComponentsY);

  Request.MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
  if (Request.MaterialPath.IsEmpty()) {
    Request.MaterialPath = TEXT("/Engine/EngineMaterials/WorldGridMaterial");
  } else if (!McpLandscapeCreation::LoadLandscapeMaterial(Request.MaterialPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Material not found: %s. Nothing was created."), *Request.MaterialPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  Request.Name = McpGetFirstStringField(Payload, {TEXT("name"), TEXT("landscapeName")});
  if (Request.Name.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("name or landscapeName parameter is required for create_landscape"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const bool bBadChar = Request.Name.FindLastCharByPredicate(
      [](TCHAR C) { return FCString::Strchr(TEXT("/\\:*?\"<>|"), C) != nullptr; }) != INDEX_NONE;
  if (bBadChar || Request.Name.Len() > 128) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("name must be at most 128 characters and contain none of / \\ : * ? \" < > |"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // A second landscape under one label left every later sculpt or paint by that name on whichever came first.
  if (FindActorByNameInWorldForMcp(GEditor->GetEditorWorldContext().World(), Request.Name, true)) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("An actor named %s is already in the level; pick another name. Nothing was created."), *Request.Name),
        TEXT("NAME_TAKEN"));
    return true;
  }

  McpLandscapeCreation::CreateLandscapeOnGameThread(
      *this, RequestId, RequestingSocket, Request);
  return true;
}
