#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
bool HandleRemoveLevelFromWorldAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }

    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required"), nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    LevelPath = SanitizeProjectRelativePath(LevelPath);
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Invalid levelPath"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    ULevel* TargetLevel = nullptr;
    for (ULevel* Level : Levels) {
      if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
        TargetLevel = Level;
        break;
      }
    }

    if (TargetLevel) {
      bool bRemoved = UEditorLevelUtils::RemoveLevelFromWorld(TargetLevel);
      if (bRemoved) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("levelPath"), LevelPath);
        Result->SetBoolField(TEXT("removed"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level removed from world"), Result);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Failed to remove level"), nullptr, TEXT("REMOVE_FAILED"));
      }
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#endif
} // namespace McpLevelHandlers
