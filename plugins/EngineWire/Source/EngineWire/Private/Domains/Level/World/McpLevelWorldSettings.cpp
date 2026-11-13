#include "Domains/Level/McpLevelActions.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"

namespace McpLevelHandlers {
bool HandleSetLevelWorldSettingsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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

    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("No editor world available"), TEXT("NO_WORLD"));
      return true;
    }

    // The open level: its settings are the ones written (World->GetWorldSettings()), and the current level can be a
    // sublevel, which the comparison and the reply named.
    const FString CurrentLevelPath = World->GetOutermost()->GetName();
    if (!RequestedLevelPath.IsEmpty() && !CurrentLevelPath.Equals(RequestedLevelPath, ESearchCase::IgnoreCase)) {
      Subsystem.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Requested level '%s' is not loaded (current: %s)"),
                         *RequestedLevelPath, *CurrentLevelPath),
          TEXT("LEVEL_NOT_LOADED"));
      return true;
    }

    AWorldSettings* Settings = World->GetWorldSettings();
    if (!Settings) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("Level has no WorldSettings actor"),
                             TEXT("NO_WORLD_SETTINGS"));
      return true;
    }

    // Resolved and checked before the step opens: Modify ran first with no transaction open, so nothing was recorded
    // for undo and a refused call had already left the level unsaved.
    FString GameModePath;
    Payload->TryGetStringField(TEXT("gameMode"), GameModePath);
    GameModePath.TrimStartAndEndInline();
    UClass* GameModeClass = nullptr;
    if (!GameModePath.IsEmpty()) {
      GameModeClass = ResolveClassByName(GameModePath);
      if (!GameModeClass || !GameModeClass->IsChildOf(AGameModeBase::StaticClass())) {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not resolve '%s' to a GameModeBase class"), *GameModePath),
            TEXT("GAME_MODE_NOT_FOUND"));
        return true;
      }
    }
    double KillZ = 0.0;
    double GravityZ = 0.0;
    double TimeDilation = 0.0;
    bool bBoundsChecks = false;
    const bool bHasKillZ = Payload->TryGetNumberField(TEXT("killZ"), KillZ);
    const bool bHasGravityZ = Payload->TryGetNumberField(TEXT("gravityZ"), GravityZ);
    const bool bHasTimeDilation = Payload->TryGetNumberField(TEXT("timeDilation"), TimeDilation);
    const bool bHasBoundsChecks = Payload->TryGetBoolField(TEXT("enableWorldBoundsChecks"), bBoundsChecks);
    if (!GameModeClass && !bHasKillZ && !bHasGravityZ && !bHasTimeDilation && !bHasBoundsChecks) {
      Subsystem.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("No world settings supplied; expected one of gameMode, killZ, "
               "gravityZ, timeDilation, enableWorldBoundsChecks"),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set World Settings")),
                                            EMcpMutationDurability::EditorStateOnly, {Settings});
    TArray<TSharedPtr<FJsonValue>> Applied;
    if (GameModeClass) {
      Settings->DefaultGameMode = GameModeClass;
      Applied.Add(MakeShared<FJsonValueString>(TEXT("gameMode")));
    }
    if (bHasKillZ) {
      Settings->KillZ = static_cast<float>(KillZ);
      Applied.Add(MakeShared<FJsonValueString>(TEXT("killZ")));
    }
    if (bHasGravityZ) {
      // GlobalGravityZ is the saved EditAnywhere property; WorldGravityZ is a transient cache
      // that AWorldSettings::GetGravityZ() re-derives from it whenever bWorldGravitySet is
      // false. Writing only the cache left GlobalGravityZ at 0, so the next GetGravityZ() threw
      // the requested value away and — because bGlobalGravitySet was now on — pinned world
      // gravity to 0. Write the saved property and let the cache rebuild from it.
      Settings->bGlobalGravitySet = true;
      Settings->GlobalGravityZ = static_cast<float>(GravityZ);
      Settings->bWorldGravitySet = false;
      Settings->GetGravityZ();
      Applied.Add(MakeShared<FJsonValueString>(TEXT("gravityZ")));
    }
    if (bHasTimeDilation) {
      Settings->TimeDilation = static_cast<float>(TimeDilation);
      Applied.Add(MakeShared<FJsonValueString>(TEXT("timeDilation")));
    }
    if (bHasBoundsChecks) {
      Settings->bEnableWorldBoundsChecks = bBoundsChecks;
      Applied.Add(MakeShared<FJsonValueString>(TEXT("enableWorldBoundsChecks")));
    }
    Settings->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), CurrentLevelPath);
    Result->SetArrayField(TEXT("appliedSettings"), Applied);
    Result->SetStringField(TEXT("gameMode"),
        Settings->DefaultGameMode ? Settings->DefaultGameMode->GetPathName() : TEXT(""));
    Result->SetNumberField(TEXT("killZ"), Settings->KillZ);
    Result->SetNumberField(TEXT("gravityZ"), Settings->GetGravityZ());
    Result->SetNumberField(TEXT("timeDilation"), Settings->TimeDilation);
    Result->SetBoolField(TEXT("enableWorldBoundsChecks"), Settings->bEnableWorldBoundsChecks);
    Transaction.DescribeInto(Result);

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("World settings updated (%d applied)"), Applied.Num()), Result);
    return true;
}
} // namespace McpLevelHandlers
