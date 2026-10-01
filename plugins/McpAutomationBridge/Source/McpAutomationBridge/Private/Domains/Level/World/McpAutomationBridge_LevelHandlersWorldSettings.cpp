#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"

namespace McpLevelHandlers {
bool HandleSetLevelWorldSettingsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString RequestedLevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), RequestedLevelPath);
    }

    if (!RequestedLevelPath.IsEmpty()) {
      const FString RequestedLevelPathAsGiven = RequestedLevelPath;
      RequestedLevelPath = SanitizeProjectRelativePath(RequestedLevelPath);
      if (RequestedLevelPath.IsEmpty()) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               McpPathRefusalMessage(TEXT("levelPath"), RequestedLevelPathAsGiven), nullptr,
                               TEXT("SECURITY_VIOLATION"));
        return true;
      }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* TargetLevel = World->GetCurrentLevel();
    if (!TargetLevel) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }

    FString CurrentLevelPath = TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT("");

    if (!RequestedLevelPath.IsEmpty()) {
      if (!CurrentLevelPath.Equals(RequestedLevelPath, ESearchCase::IgnoreCase)) {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Requested level '%s' is not loaded (current: %s)"),
                           *RequestedLevelPath, *CurrentLevelPath),
            nullptr, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
    }

    // Everything above only validated the request. The handler used to stop
    // here and still answer "World settings updated" with settingsApplied:true,
    // so every caller — including one setting the GameMode override a level
    // needs to run its own game rules — got a success receipt for a no-op.
    AWorldSettings* Settings = World->GetWorldSettings();
    if (!Settings) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level has no WorldSettings actor"), nullptr,
                             TEXT("NO_WORLD_SETTINGS"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> Applied;
    Settings->Modify();

    FString GameModePath;
    Payload->TryGetStringField(TEXT("gameMode"), GameModePath);
    if (GameModePath.IsEmpty()) Payload->TryGetStringField(TEXT("gameModeOverride"), GameModePath);
    if (!GameModePath.TrimStartAndEnd().IsEmpty()) {
      UClass* GameModeClass = ResolveClassByName(GameModePath.TrimStartAndEnd());
      if (!GameModeClass || !GameModeClass->IsChildOf(AGameModeBase::StaticClass())) {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Could not resolve '%s' to a GameModeBase class"), *GameModePath),
            nullptr, TEXT("GAME_MODE_NOT_FOUND"));
        return true;
      }
      Settings->DefaultGameMode = GameModeClass;
      Applied.Add(MakeShared<FJsonValueString>(TEXT("gameMode")));
    }

    double NumberValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("killZ"), NumberValue)) {
      Settings->KillZ = static_cast<float>(NumberValue);
      Applied.Add(MakeShared<FJsonValueString>(TEXT("killZ")));
    }
    if (Payload->TryGetNumberField(TEXT("gravityZ"), NumberValue)) {
      // GlobalGravityZ is the saved EditAnywhere property; WorldGravityZ is a transient cache
      // that AWorldSettings::GetGravityZ() re-derives from it whenever bWorldGravitySet is
      // false. Writing only the cache left GlobalGravityZ at 0, so the next GetGravityZ() threw
      // the requested value away and — because bGlobalGravitySet was now on — pinned world
      // gravity to 0. Write the saved property and let the cache rebuild from it.
      Settings->bGlobalGravitySet = true;
      Settings->GlobalGravityZ = static_cast<float>(NumberValue);
      Settings->bWorldGravitySet = false;
      Settings->GetGravityZ();
      Applied.Add(MakeShared<FJsonValueString>(TEXT("gravityZ")));
    }
    if (Payload->TryGetNumberField(TEXT("timeDilation"), NumberValue)) {
      Settings->TimeDilation = static_cast<float>(NumberValue);
      Applied.Add(MakeShared<FJsonValueString>(TEXT("timeDilation")));
    }
    bool bBoolValue = false;
    if (Payload->TryGetBoolField(TEXT("enableWorldBoundsChecks"), bBoolValue)) {
      Settings->bEnableWorldBoundsChecks = bBoolValue;
      Applied.Add(MakeShared<FJsonValueString>(TEXT("enableWorldBoundsChecks")));
    }

    if (Applied.Num() == 0) {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("No world settings supplied; expected one of gameMode, killZ, "
               "gravityZ, timeDilation, enableWorldBoundsChecks"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    Settings->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), CurrentLevelPath);
    Result->SetBoolField(TEXT("settingsApplied"), true);
    Result->SetArrayField(TEXT("appliedSettings"), Applied);
    Result->SetStringField(TEXT("gameMode"),
        Settings->DefaultGameMode ? Settings->DefaultGameMode->GetPathName() : TEXT(""));
    Result->SetNumberField(TEXT("killZ"), Settings->KillZ);
    Result->SetNumberField(TEXT("gravityZ"), Settings->GetGravityZ());
    Result->SetNumberField(TEXT("timeDilation"), Settings->TimeDilation);

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("World settings updated (%d applied)"), Applied.Num()), Result);
    return true;
}
} // namespace McpLevelHandlers
