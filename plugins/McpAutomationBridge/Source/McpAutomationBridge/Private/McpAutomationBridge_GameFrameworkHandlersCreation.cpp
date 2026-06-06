#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
static void SetOptionalClass(UBlueprint* Blueprint, const FActionContext& Context, const FString& FieldName, const FName& PropertyName, FString& Error)
{
    const FString ClassPath = GetStringField(Context.Payload, FieldName);
    if (ClassPath.IsEmpty()) return;

    if (UClass* ClassToSet = LoadClassFromPath(ClassPath))
    {
        SetClassProperty(Blueprint, PropertyName, ClassToSet, Error);
    }
}

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
        SetOptionalClass(Blueprint, Context, TEXT("defaultPawnClass"), TEXT("DefaultPawnClass"), Error);
        SetOptionalClass(Blueprint, Context, TEXT("playerControllerClass"), TEXT("PlayerControllerClass"), Error);
        SetOptionalClass(Blueprint, Context, TEXT("gameStateClass"), TEXT("GameStateClass"), Error);
        SetOptionalClass(Blueprint, Context, TEXT("playerStateClass"), TEXT("PlayerStateClass"), Error);
        SetOptionalClass(Blueprint, Context, TEXT("hudClass"), TEXT("HUDClass"), Error);
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
