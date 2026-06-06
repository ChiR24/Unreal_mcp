#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleSetLevelWorldSettingsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString RequestedLevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), RequestedLevelPath);
      if (RequestedLevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), RequestedLevelPath);
    }

    if (!RequestedLevelPath.IsEmpty()) {
      RequestedLevelPath = SanitizeProjectRelativePath(RequestedLevelPath);
      if (RequestedLevelPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Invalid levelPath"), nullptr,
                               TEXT("SECURITY_VIOLATION"));
        return true;
      }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* TargetLevel = World->GetCurrentLevel();
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }

    FString CurrentLevelPath = TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT("");

    // If a specific level path was requested, validate it matches the current level
    if (!RequestedLevelPath.IsEmpty()) {
      if (CurrentLevelPath.ToLower() != RequestedLevelPath.ToLower()) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Requested level '%s' is not loaded (current: %s)"),
                           *RequestedLevelPath, *CurrentLevelPath),
            nullptr, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), CurrentLevelPath);
    Result->SetBoolField(TEXT("settingsApplied"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("World settings updated"), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
