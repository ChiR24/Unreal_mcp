#include "Domains/GameFramework/McpGameFrameworkContext.h"

namespace McpGameFrameworkHandlers
{
static bool CreateFrameworkClass(FActionContext& Context, UClass* DefaultParent, const FString& Label, bool bConfigureGameMode)
{
    if (Context.Name.IsEmpty())
    {
        Context.SendError(FString::Printf(TEXT("Missing 'name' for %s."), *Context.SubAction), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // An unloadable or unrelated parentClass used to fall back to the default parent and still
    // answer success; refuse it before anything is created.
    const FString ParentClassPath = GetJsonStringField(Context.Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? DefaultParent : LoadClassFromPath(ParentClassPath);
    if (!ParentClass)
    {
        Context.SendError(FString::Printf(TEXT("Failed to load parentClass: %s"), *ParentClassPath), TEXT("NOT_FOUND"));
        return true;
    }
    if (!ParentClass->IsChildOf(DefaultParent))
    {
        Context.SendError(FString::Printf(TEXT("parentClass %s is not a %s."), *ParentClass->GetPathName(), *DefaultParent->GetName()),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (bConfigureGameMode)
    {
        // Each override must load and be the kind its GameMode slot holds (a Pawn for the pawn), so a
        // bad one is refused before the asset exists rather than after.
        for (const FGameModeClassSlot& Override : GameModeClassSlots)
        {
            const FString ClassPath = GetJsonStringField(Context.Payload, Override.Field);
            if (ClassPath.IsEmpty()) continue;
            UClass* OverrideClass = LoadClassFromPath(ClassPath);
            const FClassProperty* Slot = CastField<FClassProperty>(ParentClass->FindPropertyByName(Override.Property));
            if (!OverrideClass || (Slot && Slot->MetaClass && !OverrideClass->IsChildOf(Slot->MetaClass)))
            {
                Context.SendError(OverrideClass
                        ? FString::Printf(TEXT("%s for %s is not a %s; nothing was created."), *ClassPath, Override.Field, *Slot->MetaClass->GetName())
                        : FString::Printf(TEXT("Could not load class '%s' for %s; nothing was created."), *ClassPath, Override.Field),
                    OverrideClass ? TEXT("INVALID_ARGUMENT") : TEXT("NOT_FOUND"));
                return true;
            }
        }
    }

    FString Error;
    UBlueprint* Blueprint = CreateGameFrameworkBlueprint(Context.Path, Context.Name, ParentClass, Error);
    if (!Blueprint)
    {
        Context.SendError(Error, TEXT("CREATION_FAILED"));
        return true;
    }

    FString OverrideError;
    if (bConfigureGameMode && ApplyGameModeClassOverrides(Context, Blueprint, OverrideError) > 0)
    {
        McpSafeCompileBlueprint(Blueprint);
    }
    // A failed save answered success all the same.
    const bool bSaved = Context.bSave && McpSafeAssetSave(Blueprint);
    if (!OverrideError.IsEmpty())
    {
        // The classes all loaded, so this is a class of the wrong kind (a non-Pawn as the pawn).
        Context.SendError(FString::Printf(TEXT("Created %s, but a class override was refused: %s"), *Blueprint->GetPathName(), *OverrideError),
            TEXT("CLASS_OVERRIDE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
        FString::Printf(TEXT("Created %s blueprint: %s"), *Label, *Context.Name),
        Blueprint);
    Response->SetStringField(TEXT("parentClass"), ParentClass->GetPathName());
    Response->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Response, Blueprint);
    Context.SendSuccess(Response);
    return true;
}
bool HandleCoreClassAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("create_game_mode"))
    {
        return CreateFrameworkClass(Context, AGameModeBase::StaticClass(), TEXT("GameMode"), true);
    }
    if (Context.SubAction == TEXT("create_game_state"))
    {
        return CreateFrameworkClass(Context, AGameStateBase::StaticClass(), TEXT("GameState"), false);
    }
    if (Context.SubAction == TEXT("create_player_controller"))
    {
        return CreateFrameworkClass(Context, APlayerController::StaticClass(), TEXT("PlayerController"), false);
    }
    if (Context.SubAction == TEXT("create_player_state"))
    {
        return CreateFrameworkClass(Context, APlayerState::StaticClass(), TEXT("PlayerState"), false);
    }
    if (Context.SubAction == TEXT("create_game_instance"))
    {
        return CreateFrameworkClass(Context, UGameInstance::StaticClass(), TEXT("GameInstance"), false);
    }
    if (Context.SubAction == TEXT("create_hud_class"))
    {
        return CreateFrameworkClass(Context, AHUD::StaticClass(), TEXT("HUD"), false);
    }
    return false;
}
}
