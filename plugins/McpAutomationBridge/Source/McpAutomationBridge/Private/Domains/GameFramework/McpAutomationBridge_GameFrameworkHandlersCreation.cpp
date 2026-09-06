#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
// Declared in ClassConfig.cpp: applies defaultPawnClass/playerControllerClass/
// gameStateClass/playerStateClass/hudClass overrides and returns how many were
// applied, so create_game_mode can report silently-dropped fields instead of
// answering success while ignoring them.
int32 ApplyGameModeClassOverrides(FActionContext& Context, UBlueprint* Blueprint, FString& Error);

static bool CreateFrameworkClass(FActionContext& Context, UClass* DefaultParent, const FString& ActionName, const FString& Label, bool bConfigureGameMode)
{
    if (Context.Name.IsEmpty())
    {
        Context.SendError(FString::Printf(TEXT("Missing 'name' for %s."), *ActionName), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString ParentClassPath = GetStringField(Context.Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? DefaultParent : LoadClassFromPath(ParentClassPath);
    if (!ParentClass)
    {
        ParentClass = DefaultParent;
    }

    FString Error;
    UBlueprint* Blueprint = CreateGameFrameworkBlueprint(Context.Path, Context.Name, ParentClass, Error);
    if (!Blueprint)
    {
        Context.SendError(Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bConfigureGameMode)
    {
        FString OverrideError;
        const int32 Applied = ApplyGameModeClassOverrides(Context, Blueprint, OverrideError);
        if (Applied > 0)
        {
            McpSafeCompileBlueprint(Blueprint);
        }
        if (!OverrideError.IsEmpty())
        {
            // A requested override was dropped (bad class path or property) —
            // say so instead of reporting an unconditional success.
            TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
                FString::Printf(TEXT("Created %s blueprint: %s (%d class override(s) applied, some failed: %s)"),
                    *Label, *Context.Name, Applied, *OverrideError),
                Blueprint);
            McpHandlerUtils::AddVerification(Response, Blueprint);
            Context.SendSuccess(Response);
            return true;
        }
    }

    if (Context.bSave)
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
        FString::Printf(TEXT("Created %s blueprint: %s"), *Label, *Context.Name),
        Blueprint);
    McpHandlerUtils::AddVerification(Response, Blueprint);
    Context.SendSuccess(Response);
    return true;
}

bool HandleCoreClassAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("create_game_mode"))
    {
        return CreateFrameworkClass(Context, AGameModeBase::StaticClass(), TEXT("create_game_mode"), TEXT("GameMode"), true);
    }
    if (Context.SubAction == TEXT("create_game_state"))
    {
        return CreateFrameworkClass(Context, AGameStateBase::StaticClass(), TEXT("create_game_state"), TEXT("GameState"), false);
    }
    if (Context.SubAction == TEXT("create_player_controller"))
    {
        return CreateFrameworkClass(Context, APlayerController::StaticClass(), TEXT("create_player_controller"), TEXT("PlayerController"), false);
    }
    if (Context.SubAction == TEXT("create_player_state"))
    {
        return CreateFrameworkClass(Context, APlayerState::StaticClass(), TEXT("create_player_state"), TEXT("PlayerState"), false);
    }
    if (Context.SubAction == TEXT("create_game_instance"))
    {
        return CreateFrameworkClass(Context, UGameInstance::StaticClass(), TEXT("create_game_instance"), TEXT("GameInstance"), false);
    }
    if (Context.SubAction == TEXT("create_hud_class"))
    {
        return CreateFrameworkClass(Context, AHUD::StaticClass(), TEXT("create_hud_class"), TEXT("HUD"), false);
    }
    return false;
}
#endif
}
