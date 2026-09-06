#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
bool HandleGetLevelActorsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }

    if (!LevelPath.IsEmpty()) {
      LevelPath = SanitizeProjectRelativePath(LevelPath);
      if (LevelPath.IsEmpty()) {
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

    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
      for (ULevel* Level : Levels) {
        if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
          TargetLevel = Level;
          break;
        }
      }
    } else {
      TargetLevel = World->GetCurrentLevel();
    }

    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ActorsArray;
    for (AActor* Actor : TargetLevel->Actors) {
      if (Actor) {
        ActorsArray.Add(MakeShared<FJsonValueString>(Actor->GetName()));
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetNumberField(TEXT("count"), ActorsArray.Num());
    Result->SetArrayField(TEXT("actors"), ActorsArray);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level actors retrieved"), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#endif
} // namespace McpLevelHandlers
