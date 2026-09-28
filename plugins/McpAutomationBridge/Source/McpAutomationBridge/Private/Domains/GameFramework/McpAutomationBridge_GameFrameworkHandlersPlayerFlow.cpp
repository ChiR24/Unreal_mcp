#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{

static bool SetRespawnRules(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    if (!Context.Payload->HasField(TEXT("respawnDelay")))
    {
        Context.SendError(TEXT("Missing 'respawnDelay' (seconds)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const double RespawnDelay = GetNumberField(Context.Payload, TEXT("respawnDelay"), 0.0);
    // MinRespawnDelay lives on AGameMode; a GameModeBase child has no respawn rule to set.
    AGameMode* GameModeCDO = Blueprint->GeneratedClass ? Cast<AGameMode>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!GameModeCDO)
    {
        Context.SendError(
            FString::Printf(TEXT("%s is not a GameMode (AGameMode) child, so it has no MinRespawnDelay. Create the game mode with parentClass /Script/Engine.GameMode to use respawn rules."),
                *Blueprint->GetName()),
            TEXT("NOT_SUPPORTED"));
        return true;
    }
    GameModeCDO->MinRespawnDelay = static_cast<float>(RespawnDelay);
    GameModeCDO->MarkPackageDirty();
    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
        FString::Printf(TEXT("Set MinRespawnDelay=%.1f"), RespawnDelay),
        Blueprint);
    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("respawnDelay"), RespawnDelay);
    Response->SetObjectField(TEXT("configuration"), ConfigObj);
    Context.SendSuccess(Response);
    return true;
}

static bool ConfigureSpectating(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    const FString SpectatorClassPath = GetStringField(Context.Payload, TEXT("spectatorClass"));
    if (SpectatorClassPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'spectatorClass' (a SpectatorPawn class path)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    UClass* SpectatorClass = LoadClassFromPath(SpectatorClassPath);
    if (!SpectatorClass)
    {
        Context.SendError(FString::Printf(TEXT("Failed to load spectator class: %s"), *SpectatorClassPath), TEXT("NOT_FOUND"));
        return true;
    }
    FString Error;
    if (!SetClassProperty(Blueprint, TEXT("SpectatorClass"), SpectatorClass, Error))
    {
        Context.SendError(Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    FinishBlueprintMutation(Blueprint, Context.bSave);
    Context.SendSuccess(MakeBlueprintResponse(
        FString::Printf(TEXT("Set SpectatorClass to %s"), *SpectatorClass->GetPathName()), Blueprint));
    return true;
}

bool HandlePlayerFlowAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("set_respawn_rules")) return SetRespawnRules(Context);
    if (Context.SubAction == TEXT("configure_spectating")) return ConfigureSpectating(Context);
    return false;
}
}
