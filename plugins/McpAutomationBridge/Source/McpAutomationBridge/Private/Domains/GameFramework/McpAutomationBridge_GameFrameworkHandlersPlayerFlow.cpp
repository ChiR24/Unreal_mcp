#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"

#include "ScopedTransaction.h"

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

// Tags one PlayerStart in the open editor level. The engine's FindPlayerStart picks a start whose
// PlayerStartTag equals the travel URL's Portal, and team spawning matches TeamN, so the tag is the
// level half of team spawns. An undoable level edit, not saved: the level stays modified.
static bool ConfigurePlayerStart(FActionContext& Context)
{
    const FString StartName = GetStringField(Context.Payload, TEXT("playerStartName")).TrimStartAndEnd();
    FString Tag = GetStringField(Context.Payload, TEXT("playerStartTag")).TrimStartAndEnd();
    if (Context.Payload->HasField(TEXT("teamIndex")))
    {
        const double TeamIndex = GetNumberField(Context.Payload, TEXT("teamIndex"));
        if (!Tag.IsEmpty() || TeamIndex < 1.0 || TeamIndex != FMath::FloorToDouble(TeamIndex))
        {
            Context.SendError(TEXT("teamIndex is a whole number from 1 (it sets the tag Team1, Team2, ...); pass it or playerStartTag, not both. Nothing was changed."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Tag = FString::Printf(TEXT("Team%d"), static_cast<int32>(TeamIndex));
    }
    if (Tag.IsEmpty())
    {
        Context.SendError(TEXT("Pass playerStartTag (any tag) or teamIndex (1 or more, sets TeamN)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Context.SendError(TEXT("No editor level is open."), TEXT("NO_WORLD"));
        return true;
    }
    TArray<FString> Labels;
    APlayerStart* Target = nullptr;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        Labels.Add(It->GetActorLabel());
        Target = Labels.Num() == 1 ? *It : nullptr;
    }
    if (!StartName.IsEmpty())
    {
        Target = FindActorOfClassForMcp<APlayerStart>(World, StartName);
    }
    if (!Target)
    {
        const FString Names = FString::Join(Labels, TEXT(", "));
        if (Labels.Num() == 0)
        {
            Context.SendError(TEXT("The open level has no PlayerStart. Place one first (control_actor spawn with classPath /Script/Engine.PlayerStart)."), TEXT("NOT_FOUND"));
        }
        else if (StartName.IsEmpty())
        {
            Context.SendError(FString::Printf(TEXT("The level has %d PlayerStarts; pass playerStartName, one of: %s."), Labels.Num(), *Names), TEXT("AMBIGUOUS_TARGET"));
        }
        else
        {
            Context.SendError(FString::Printf(TEXT("No PlayerStart named '%s'. The level has: %s."), *StartName, *Names), TEXT("NOT_FOUND"));
        }
        return true;
    }

    const FName PreviousTag = Target->PlayerStartTag;
    {
        const FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "SetPlayerStartTag", "Set PlayerStart Tag"));
        Target->Modify();
        Target->PlayerStartTag = FName(*Tag);
    }
    Target->MarkPackageDirty();

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("playerStart"), Target->GetActorLabel());
    Response->SetStringField(TEXT("playerStartTag"), Tag);
    Response->SetStringField(TEXT("previousTag"), PreviousTag.ToString());
    Response->SetStringField(TEXT("message"), FString::Printf(
        TEXT("PlayerStart '%s' tag set to '%s' (was '%s') in level %s. The level is modified but not saved: save it to keep the tag. Undo reverts it."),
        *Target->GetActorLabel(), *Tag, *PreviousTag.ToString(), *World->GetMapName()));
    Context.SendSuccess(Response);
    return true;
}

bool HandlePlayerFlowAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("configure_player_start")) return ConfigurePlayerStart(Context);
    if (Context.SubAction == TEXT("set_respawn_rules")) return SetRespawnRules(Context);
    if (Context.SubAction == TEXT("configure_spectating")) return ConfigureSpectating(Context);
    return false;
}
}
