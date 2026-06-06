#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
static bool ConfigureSpawnSystem(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const FString SpawnMethod = GetStringField(Context.Payload, TEXT("spawnSelectionMethod"), TEXT("Random"));
    const double RespawnDelay = GetNumberField(Context.Payload, TEXT("respawnDelay"), 5.0);
    const bool bUsePlayerStarts = GetBoolField(Context.Payload, TEXT("usePlayerStarts"), true);
    const bool bCanRespawn = GetBoolField(Context.Payload, TEXT("canRespawn"), true);
    const int32 MaxRespawns = static_cast<int32>(GetNumberField(Context.Payload, TEXT("maxRespawns"), -1));
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring spawn system: method=%s, delay=%.1f, usePlayerStarts=%d"), *SpawnMethod, RespawnDelay, bUsePlayerStarts);

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("SpawnSelectionMethod"), MakeNamePinType(), TEXT("Spawn System"), SpawnMethod);
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("RespawnDelay"), MakeFloatPinType(), TEXT("Spawn System"), FString::SanitizeFloat(RespawnDelay));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bUsePlayerStarts"), MakeBoolPinType(), TEXT("Spawn System"), bUsePlayerStarts ? TEXT("true") : TEXT("false"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bCanRespawn"), MakeBoolPinType(), TEXT("Spawn System"), bCanRespawn ? TEXT("true") : TEXT("false"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("MaxRespawns"), MakeIntPinType(), TEXT("Spawn System"), FString::FromInt(MaxRespawns));

    if (Blueprint->GeneratedClass)
    {
        AGameMode* GameModeCDO = Cast<AGameMode>(Blueprint->GeneratedClass->GetDefaultObject());
        if (GameModeCDO)
        {
            GameModeCDO->MinRespawnDelay = static_cast<float>(RespawnDelay);
            GameModeCDO->MarkPackageDirty();
        }
        else
        {
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Blueprint is not derived from AGameMode. MinRespawnDelay not set."));
        }
    }

    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(FString::Printf(TEXT("Added %d spawn system variables to Blueprint"), VarsAdded), Blueprint);
    Response->SetNumberField(TEXT("variablesAdded"), VarsAdded);
    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetStringField(TEXT("spawnSelectionMethod"), SpawnMethod);
    ConfigObj->SetNumberField(TEXT("respawnDelay"), RespawnDelay);
    ConfigObj->SetBoolField(TEXT("usePlayerStarts"), bUsePlayerStarts);
    ConfigObj->SetBoolField(TEXT("canRespawn"), bCanRespawn);
    ConfigObj->SetNumberField(TEXT("maxRespawns"), MaxRespawns);
    Response->SetObjectField(TEXT("configuration"), ConfigObj);
    Context.SendSuccess(Response);
    return true;
}

static bool ConfigurePlayerStart(FActionContext& Context)
{
    if (Context.BlueprintPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'blueprintPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const int32 TeamIndex = static_cast<int32>(GetNumberField(Context.Payload, TEXT("teamIndex"), 0));
    const bool bPlayerOnly = GetBoolField(Context.Payload, TEXT("bPlayerOnly"), false);
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configure PlayerStart: path=%s, teamIndex=%d, playerOnly=%d"), *Context.BlueprintPath, TeamIndex, bPlayerOnly);

    FString PlayerStartName = GetStringField(Context.Payload, TEXT("playerStartName"));
    if (PlayerStartName.IsEmpty()) PlayerStartName = GetStringField(Context.Payload, TEXT("actorName"));

    FString PlayerStartTag = GetStringField(Context.Payload, TEXT("playerStartTag"));
    if (PlayerStartTag.IsEmpty() && TeamIndex > 0)
    {
        PlayerStartTag = FString::Printf(TEXT("Team%d"), TeamIndex);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Context.SendError(TEXT("No world available."), TEXT("NO_WORLD"));
        return true;
    }

    int32 ConfiguredCount = 0;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        APlayerStart* PlayerStart = *It;
        if (!PlayerStart) continue;
        if (!PlayerStartName.IsEmpty() &&
            !PlayerStart->GetActorLabel().Equals(PlayerStartName, ESearchCase::IgnoreCase) &&
            !PlayerStart->GetName().Equals(PlayerStartName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!PlayerStartTag.IsEmpty())
        {
            PlayerStart->PlayerStartTag = FName(*PlayerStartTag);
        }
        PlayerStart->MarkPackageDirty();
        ConfiguredCount++;
        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configured PlayerStart: %s with tag=%s"), *PlayerStart->GetName(), *PlayerStartTag);
    }

    if (ConfiguredCount == 0 && !PlayerStartName.IsEmpty())
    {
        Context.SendError(FString::Printf(TEXT("PlayerStart '%s' not found in level."), *PlayerStartName), TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured %d PlayerStart actor(s)"), ConfiguredCount));
    Response->SetNumberField(TEXT("configuredCount"), ConfiguredCount);
    if (!PlayerStartTag.IsEmpty()) Response->SetStringField(TEXT("playerStartTag"), PlayerStartTag);
    Response->SetNumberField(TEXT("teamIndex"), TeamIndex);
    Context.SendSuccess(Response);
    return true;
}

static bool SetRespawnRules(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const double RespawnDelay = GetNumberField(Context.Payload, TEXT("respawnDelay"), 5.0);
    const FString RespawnLocation = GetStringField(Context.Payload, TEXT("respawnLocation"), TEXT("PlayerStart"));
    const bool bForceRespawn = GetBoolField(Context.Payload, TEXT("forceRespawn"), true);
    const int32 RespawnLives = static_cast<int32>(GetNumberField(Context.Payload, TEXT("respawnLives"), -1));
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Setting respawn rules: delay=%.1f, location=%s, force=%d, lives=%d"), RespawnDelay, *RespawnLocation, bForceRespawn, RespawnLives);

    if (Blueprint->GeneratedClass)
    {
        if (AGameMode* GameModeCDO = Cast<AGameMode>(Blueprint->GeneratedClass->GetDefaultObject()))
        {
            GameModeCDO->MinRespawnDelay = static_cast<float>(RespawnDelay);
            GameModeCDO->MarkPackageDirty();
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Set MinRespawnDelay=%.1f on CDO"), RespawnDelay);
        }
        else
        {
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Blueprint is not derived from AGameMode. MinRespawnDelay not set."));
        }
    }

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("RespawnLocation"), MakeNamePinType(), TEXT("Respawn Rules"), RespawnLocation);
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bForceRespawn"), MakeBoolPinType(), TEXT("Respawn Rules"), bForceRespawn ? TEXT("true") : TEXT("false"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("RespawnLives"), MakeIntPinType(), TEXT("Respawn Rules"), FString::FromInt(RespawnLives));
    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
        FString::Printf(TEXT("Set respawn rules (MinRespawnDelay=%.1f, added %d variables)"), RespawnDelay, VarsAdded),
        Blueprint);
    Response->SetNumberField(TEXT("variablesAdded"), VarsAdded);
    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("respawnDelay"), RespawnDelay);
    ConfigObj->SetStringField(TEXT("respawnLocation"), RespawnLocation);
    ConfigObj->SetBoolField(TEXT("forceRespawn"), bForceRespawn);
    ConfigObj->SetNumberField(TEXT("respawnLives"), RespawnLives);
    Response->SetObjectField(TEXT("configuration"), ConfigObj);
    Context.SendSuccess(Response);
    return true;
}

static bool ConfigureSpectating(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const FString SpectatorClassPath = GetStringField(Context.Payload, TEXT("spectatorClass"));
    if (!SpectatorClassPath.IsEmpty())
    {
        if (UClass* SpectatorClass = LoadClassFromPath(SpectatorClassPath))
        {
            FString Error;
            SetClassProperty(Blueprint, TEXT("SpectatorClass"), SpectatorClass, Error);
        }
    }

    FinishBlueprintMutation(Blueprint, Context.bSave);
    Context.SendSuccess(MakeBlueprintResponse(TEXT("Spectating configured."), Blueprint));
    return true;
}

bool HandlePlayerFlowAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("configure_spawn_system")) return ConfigureSpawnSystem(Context);
    if (Context.SubAction == TEXT("configure_player_start")) return ConfigurePlayerStart(Context);
    if (Context.SubAction == TEXT("set_respawn_rules")) return SetRespawnRules(Context);
    if (Context.SubAction == TEXT("configure_spectating")) return ConfigureSpectating(Context);
    return false;
}
#endif
}
