#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "EditorBuildUtils.h"
#include "RenderingThread.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleBuildLightingAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    // FEditorBuildUtils::EditorBuild is async — returns immediately while
    // the lighting build runs in the background, avoiding 30s timeout.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("buildStarted"), true);
    if (Payload.IsValid()) {
      FString Q;
      if (Payload->TryGetStringField(TEXT("quality"), Q) && !Q.IsEmpty())
        Result->SetStringField(TEXT("quality"), Q);
    }
    FlushRenderingCommands();
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildLighting);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Lighting build started (runs in background)"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Lighting build requested (no active world)"), Result);
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
