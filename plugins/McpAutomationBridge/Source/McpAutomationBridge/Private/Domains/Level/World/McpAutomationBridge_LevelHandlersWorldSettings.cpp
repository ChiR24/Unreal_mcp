#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
namespace {
// GameModes authored as Blueprints live in the generated `_C` class, a suffix
// callers rarely spell. Accept the plain asset path as well and derive it.
UClass* ResolveGameModeClass(const FString& InPath) {
  const FString Path = InPath.TrimStartAndEnd();
  if (Path.IsEmpty()) {
    return nullptr;
  }
  if (UClass* Direct = LoadObject<UClass>(nullptr, *Path)) {
    return Direct;
  }
  if (Path.EndsWith(TEXT("_C"))) {
    return nullptr;
  }
  FString ObjectPath = Path;
  if (!Path.Contains(TEXT("."))) {
    int32 SlashIndex = INDEX_NONE;
    if (!Path.FindLastChar(TEXT('/'), SlashIndex)) {
      return nullptr;
    }
    ObjectPath = Path + TEXT(".") + Path.RightChop(SlashIndex + 1);
  }
  return LoadObject<UClass>(nullptr, *(ObjectPath + TEXT("_C")));
}
} // namespace

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

    // Everything above only validated the request. The handler used to stop
    // here and still answer "World settings updated" with settingsApplied:true,
    // so every caller — including one setting the GameMode override a level
    // needs to run its own game rules — got a success receipt for a no-op.
    AWorldSettings* Settings = World->GetWorldSettings();
    if (!Settings) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
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
      UClass* GameModeClass = ResolveGameModeClass(GameModePath);
      if (!GameModeClass || !GameModeClass->IsChildOf(AGameModeBase::StaticClass())) {
        SendAutomationResponse(
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
      Settings->bGlobalGravitySet = true;
      Settings->WorldGravityZ = static_cast<float>(NumberValue);
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
      SendAutomationResponse(
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
    Result->SetNumberField(TEXT("gravityZ"), Settings->WorldGravityZ);
    Result->SetNumberField(TEXT("timeDilation"), Settings->TimeDilation);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("World settings updated (%d applied)"), Applied.Num()), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
