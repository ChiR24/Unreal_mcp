#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleGetCurrentLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* CurrentLevel = EditorWorld->GetCurrentLevel();
    if (!CurrentLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level available"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }

    UPackage* WorldPackage = EditorWorld->GetOutermost();
    UPackage* LevelPackage = CurrentLevel->GetOutermost();

    auto WorldTypeToString = [](EWorldType::Type WorldType) -> FString {
      switch (WorldType) {
      case EWorldType::Game:
        return TEXT("Game");
      case EWorldType::Editor:
        return TEXT("Editor");
      case EWorldType::PIE:
        return TEXT("PIE");
      case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
      case EWorldType::GamePreview:
        return TEXT("GamePreview");
      case EWorldType::GameRPC:
        return TEXT("GameRPC");
      case EWorldType::Inactive:
        return TEXT("Inactive");
      case EWorldType::None:
      default:
        return TEXT("None");
      }
    };

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("mapName"), EditorWorld->GetMapName());
    Result->SetStringField(TEXT("mapPath"), WorldPackage ? WorldPackage->GetName() : TEXT(""));
    Result->SetStringField(TEXT("levelName"), CurrentLevel->GetName());
    Result->SetStringField(TEXT("levelPath"), LevelPackage ? LevelPackage->GetName() : TEXT(""));
    // Include editor-world identity separately from the map package so agents
    // can distinguish persistent map state from transient PIE/editor worlds.
    Result->SetStringField(TEXT("editorWorldName"), EditorWorld->GetName());
    Result->SetStringField(TEXT("editorWorldPath"), WorldPackage ? WorldPackage->GetPathName() : TEXT(""));
    Result->SetStringField(TEXT("worldType"), WorldTypeToString(EditorWorld->WorldType));
    Result->SetNumberField(TEXT("actorCount"), CurrentLevel->Actors.Num());
    Result->SetBoolField(TEXT("isPersistentLevel"), CurrentLevel == EditorWorld->PersistentLevel);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Current level retrieved"), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
