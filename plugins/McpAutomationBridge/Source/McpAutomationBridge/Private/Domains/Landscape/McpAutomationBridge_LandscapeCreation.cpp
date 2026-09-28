#include "Domains/Landscape/McpAutomationBridge_LandscapeCreation.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateLandscape(
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
      Out = FMath::Max(1, FMath::DivideAndRoundUp(static_cast<int32>(Value), Request.QuadsPerComponent));
    }
  };
  Components(TEXT("x"), TEXT("sizeX"), Request.ComponentsX);
  Components(TEXT("y"), TEXT("sizeY"), Request.ComponentsY);

  Request.MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
  if (Request.MaterialPath.IsEmpty()) {
    Request.MaterialPath = TEXT("/Engine/EngineMaterials/WorldGridMaterial");
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

  McpLandscapeCreation::CreateLandscapeOnGameThread(
      *this, RequestId, RequestingSocket, Request);
  return true;
}
