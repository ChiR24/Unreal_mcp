#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{

static bool SetRespawnRules(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const double RespawnDelay = GetNumberField(Context.Payload, TEXT("respawnDelay"), 5.0);

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
    if (Context.SubAction == TEXT("set_respawn_rules")) return SetRespawnRules(Context);
    if (Context.SubAction == TEXT("configure_spectating")) return ConfigureSpectating(Context);
    return false;
}
}
