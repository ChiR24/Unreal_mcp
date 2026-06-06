#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
static bool SetGameModeClass(
    FActionContext& Context,
    const FString& ClassPath,
    const FName& PropertyName,
    const FString& MissingMessage,
    const FString& NotFoundLabel,
    const FString& SuccessLabel)
{
    if (!RequireGameModePath(Context)) return true;
    if (ClassPath.IsEmpty())
    {
        Context.SendError(MissingMessage, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    UClass* ClassToSet = LoadClassFromPath(ClassPath);
    if (!ClassToSet)
    {
        Context.SendError(FString::Printf(TEXT("Failed to load %s class: %s"), *NotFoundLabel, *ClassPath), TEXT("NOT_FOUND"));
        return true;
    }

    FString Error;
    if (!SetClassProperty(Blueprint, PropertyName, ClassToSet, Error))
    {
        Context.SendError(Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(Blueprint);
    if (Context.bSave)
    {
        McpSafeAssetSave(Blueprint);
    }

    Context.SendSuccess(MakeBlueprintResponse(FString::Printf(TEXT("Set %s to %s"), *SuccessLabel, *ClassPath), Blueprint));
    return true;
}

static bool ConfigureGameRules(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;

    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;
    if (!Blueprint->GeneratedClass)
    {
        Context.SendError(
            FString::Printf(TEXT("Failed to load GameMode: %s"), *Context.GameModeBlueprint),
            TEXT("NOT_FOUND"));
        return true;
    }

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        Context.SendError(TEXT("Failed to get CDO."), TEXT("INTERNAL_ERROR"));
        return true;
    }

    bool bModified = false;
    if (Context.Payload->HasField(TEXT("bDelayedStart")))
    {
        FBoolProperty* Prop = CastField<FBoolProperty>(Blueprint->GeneratedClass->FindPropertyByName(TEXT("bDelayedStart")));
        if (Prop)
        {
            Prop->SetPropertyValue_InContainer(CDO, GetBoolField(Context.Payload, TEXT("bDelayedStart")));
            bModified = true;
        }
    }

    if (Context.Payload->HasField(TEXT("startPlayersNeeded")))
    {
        Context.SendError(
            TEXT("startPlayersNeeded is not a native GameMode property and is not implemented as a generated Blueprint variable."),
            TEXT("UNSUPPORTED_FIELD"));
        return true;
    }

    if (bModified)
    {
        CDO->MarkPackageDirty();
        McpSafeCompileBlueprint(Blueprint);
    }
    if (Context.bSave)
    {
        McpSafeAssetSave(Blueprint);
    }

    Context.SendSuccess(MakeBlueprintResponse(TEXT("Configured game rules"), Blueprint));
    return true;
}

bool HandleGameModeConfigAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("set_default_pawn_class"))
    {
        FString PawnClassPath = GetStringField(Context.Payload, TEXT("pawnClass"));
        if (PawnClassPath.IsEmpty()) PawnClassPath = GetStringField(Context.Payload, TEXT("defaultPawnClass"));
        return SetGameModeClass(
            Context,
            PawnClassPath,
            TEXT("DefaultPawnClass"),
            TEXT("Missing 'pawnClass' or 'defaultPawnClass'."),
            TEXT("pawn"),
            TEXT("DefaultPawnClass"));
    }
    if (Context.SubAction == TEXT("set_player_controller_class"))
    {
        return SetGameModeClass(
            Context,
            GetStringField(Context.Payload, TEXT("playerControllerClass")),
            TEXT("PlayerControllerClass"),
            TEXT("Missing 'playerControllerClass'."),
            TEXT("PlayerController"),
            TEXT("PlayerControllerClass"));
    }
    if (Context.SubAction == TEXT("set_game_state_class"))
    {
        return SetGameModeClass(
            Context,
            GetStringField(Context.Payload, TEXT("gameStateClass")),
            TEXT("GameStateClass"),
            TEXT("Missing 'gameStateClass'."),
            TEXT("GameState"),
            TEXT("GameStateClass"));
    }
    if (Context.SubAction == TEXT("set_player_state_class"))
    {
        return SetGameModeClass(
            Context,
            GetStringField(Context.Payload, TEXT("playerStateClass")),
            TEXT("PlayerStateClass"),
            TEXT("Missing 'playerStateClass'."),
            TEXT("PlayerState"),
            TEXT("PlayerStateClass"));
    }
    if (Context.SubAction == TEXT("configure_game_rules"))
    {
        return ConfigureGameRules(Context);
    }
    return false;
}
#endif
}
