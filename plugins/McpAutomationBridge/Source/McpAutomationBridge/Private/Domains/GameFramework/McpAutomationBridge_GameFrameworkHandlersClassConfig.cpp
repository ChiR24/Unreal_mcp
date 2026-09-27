#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"

namespace McpGameFrameworkHandlers
{
static void PersistEffectiveGameFramework(FActionContext& Context, UBlueprint* GameModeBlueprint)
{
    if (!GameModeBlueprint || !GameModeBlueprint->GeneratedClass) return;
    UClass* GameModeClass = GameModeBlueprint->GeneratedClass;
    if (UGameMapsSettings* GameMapsSettings = UGameMapsSettings::GetGameMapsSettings())
    {
        GConfig->SetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"),
            *GameModeClass->GetPathName(), GEngineIni);
        GConfig->Flush(false, GEngineIni);
        GameMapsSettings->ReloadConfig();
    }
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        if (AWorldSettings* WorldSettings = GEditor->GetEditorWorldContext().World()->GetWorldSettings())
        {
            WorldSettings->DefaultGameMode = GameModeClass;
            WorldSettings->MarkPackageDirty();
        }
    }
}

static int32 SetOptionalClassCounted(UBlueprint* Blueprint, const FActionContext& Context, const FString& FieldName, const FName& PropertyName, FString& Error)
{
    const FString ClassPath = GetStringField(Context.Payload, FieldName);
    if (ClassPath.IsEmpty()) return 0;

    UClass* ClassToSet = LoadClassFromPath(ClassPath);
    if (!ClassToSet)
    {
        Error = FString::Printf(TEXT("Could not load class '%s' for %s"), *ClassPath, *FieldName);
        return 0;
    }
    if (!SetClassProperty(Blueprint, PropertyName, ClassToSet, Error))
    {
        return 0;
    }
    return 1;
}

// Not static: McpAutomationBridge_GameFrameworkHandlersCreation.cpp links against
// this helper so create_game_mode can apply the class overrides and report any
// that failed to resolve instead of silently dropping them.
int32 ApplyGameModeClassOverrides(FActionContext& Context, UBlueprint* Blueprint, FString& Error)
{
    int32 Applied = 0;
    Applied += SetOptionalClassCounted(Blueprint, Context, TEXT("defaultPawnClass"), TEXT("DefaultPawnClass"), Error);
    Applied += SetOptionalClassCounted(Blueprint, Context, TEXT("playerControllerClass"), TEXT("PlayerControllerClass"), Error);
    Applied += SetOptionalClassCounted(Blueprint, Context, TEXT("gameStateClass"), TEXT("GameStateClass"), Error);
    Applied += SetOptionalClassCounted(Blueprint, Context, TEXT("playerStateClass"), TEXT("PlayerStateClass"), Error);
    Applied += SetOptionalClassCounted(Blueprint, Context, TEXT("hudClass"), TEXT("HUDClass"), Error);
    return Applied;
}

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
    PersistEffectiveGameFramework(Context, Blueprint);
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
    // set_*_class: sub-action -> payload field (set_default_pawn_class also takes defaultPawnClass), GameMode property,
    // label for the not-found message.
    struct FClassSetter
    {
        const TCHAR* SubAction;
        const TCHAR* Field;
        const TCHAR* Property;
        const TCHAR* Label;
    };
    static const FClassSetter Setters[] = {
        {TEXT("set_default_pawn_class"), TEXT("pawnClass"), TEXT("DefaultPawnClass"), TEXT("pawn")},
        {TEXT("set_player_controller_class"), TEXT("playerControllerClass"), TEXT("PlayerControllerClass"), TEXT("PlayerController")},
        {TEXT("set_game_state_class"), TEXT("gameStateClass"), TEXT("GameStateClass"), TEXT("GameState")},
        {TEXT("set_player_state_class"), TEXT("playerStateClass"), TEXT("PlayerStateClass"), TEXT("PlayerState")},
        {TEXT("set_hud_class"), TEXT("hudClass"), TEXT("HUDClass"), TEXT("HUD")},
    };
    for (const FClassSetter& Setter : Setters)
    {
        if (Context.SubAction == Setter.SubAction)
        {
            const bool bPawn = Setter.Property == FString(TEXT("DefaultPawnClass"));
            return SetGameModeClass(
                Context,
                bPawn ? McpGetFirstStringField(Context.Payload, {TEXT("pawnClass"), TEXT("defaultPawnClass")})
                      : GetStringField(Context.Payload, Setter.Field),
                Setter.Property,
                bPawn ? FString(TEXT("Missing 'pawnClass' or 'defaultPawnClass'."))
                      : FString::Printf(TEXT("Missing '%s'."), Setter.Field),
                Setter.Label,
                Setter.Property);
        }
    }
    if (Context.SubAction == TEXT("configure_game_rules"))
    {
        return ConfigureGameRules(Context);
    }
    return false;
}
}
