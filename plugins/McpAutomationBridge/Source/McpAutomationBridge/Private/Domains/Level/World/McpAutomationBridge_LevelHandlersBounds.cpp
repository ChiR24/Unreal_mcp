#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/LevelBounds.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
bool HandleGetLevelBoundsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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

    // Most levels have no ALevelBounds actor at all, and the fallback was an
    // uninitialised FBox printed as "X=0 Y=0 Z=0" for BOTH min and max -- a
    // level full of geometry read back as a point at the origin, with nothing
    // in the reply saying the bounds had never been computed.
    // ALevelBounds::CalculateLevelBounds is what the engine itself uses to
    // build that actor's box, so fall back to it and say which source won.
    FBox LevelBounds(ForceInit);
    FString BoundsSource = TEXT("none");
    if (TargetLevel->LevelBoundsActor.IsValid()) {
      LevelBounds = TargetLevel->LevelBoundsActor->GetComponentsBoundingBox();
      if (LevelBounds.IsValid) {
        BoundsSource = TEXT("level_bounds_actor");
      }
    }
    if (!LevelBounds.IsValid) {
      LevelBounds = ALevelBounds::CalculateLevelBounds(TargetLevel);
      if (LevelBounds.IsValid) {
        BoundsSource = TEXT("calculated_from_actors");
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetStringField(TEXT("min"), FString::Printf(TEXT("X=%f Y=%f Z=%f"), LevelBounds.Min.X, LevelBounds.Min.Y, LevelBounds.Min.Z));
    Result->SetStringField(TEXT("max"), FString::Printf(TEXT("X=%f Y=%f Z=%f"), LevelBounds.Max.X, LevelBounds.Max.Y, LevelBounds.Max.Z));
    Result->SetBoolField(TEXT("boundsValid"), static_cast<bool>(LevelBounds.IsValid));
    Result->SetStringField(TEXT("boundsSource"), BoundsSource);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level bounds retrieved"), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#endif
} // namespace McpLevelHandlers
