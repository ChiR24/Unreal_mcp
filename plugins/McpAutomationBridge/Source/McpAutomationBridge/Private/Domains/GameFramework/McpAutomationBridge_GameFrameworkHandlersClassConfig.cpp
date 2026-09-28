#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"

namespace McpGameFrameworkHandlers
{
// Opt-in (makeDefault): setting a class never moves the project's game mode by itself. When asked,
// the game mode becomes the project default in DefaultEngine.ini, the file Project Settings writes.
// It used to happen on every call, in the per-user Saved config, and also rewrote the open level's
// World Settings override.
static bool MakeProjectDefaultGameMode(UBlueprint* GameModeBlueprint)
{
    UGameMapsSettings* Settings = UGameMapsSettings::GetGameMapsSettings();
    if (!Settings || !GameModeBlueprint || !GameModeBlueprint->GeneratedClass) return false;
    UGameMapsSettings::SetGlobalDefaultGameMode(GameModeBlueprint->GeneratedClass->GetPathName());
    Settings->SaveConfig();
    return Settings->TryUpdateDefaultConfigFile();
}

// Which game mode the open level runs in play (its World Settings override, else the project
// default), so the reply says whether the class just set is the one PIE will use.
static void ReportEffectiveGameMode(const TSharedPtr<FJsonObject>& Response, UBlueprint* GameModeBlueprint)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    UClass* Running = ResolveWorldGameMode(World, Info);
    Response->SetStringField(TEXT("openLevelGameMode"), Running ? Running->GetPathName() : FString());
    Response->SetBoolField(TEXT("effectiveInOpenLevel"), Running && Running == GameModeBlueprint->GeneratedClass);
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
    if (Context.bSave)
    {
        McpSafeAssetSave(Blueprint);
    }
    const bool bMakeDefault = GetBoolField(Context.Payload, TEXT("makeDefault"), false);
    if (bMakeDefault && !MakeProjectDefaultGameMode(Blueprint))
    {
        Context.SendError(FString::Printf(
            TEXT("Set %s to %s, but %s could not be written to DefaultEngine.ini as the project default game mode (is the file read-only?)."),
            *SuccessLabel, *ClassPath, *Blueprint->GetName()), TEXT("CONFIG_WRITE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(FString(), Blueprint);
    Response->SetBoolField(TEXT("madeDefault"), bMakeDefault);
    ReportEffectiveGameMode(Response, Blueprint);
    const bool bEffective = Response->GetBoolField(TEXT("effectiveInOpenLevel"));
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s to %s on %s.%s"), *SuccessLabel, *ClassPath,
        *Blueprint->GetName(),
        bMakeDefault ? TEXT(" It is now the project default game mode.")
        : bEffective ? TEXT("")
                     : TEXT(" The open level runs another game mode (openLevelGameMode); pass makeDefault: true, or set the level's GameMode Override, to play with it.")));
    Context.SendSuccess(Response);
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

    if (!Context.Payload->HasField(TEXT("bDelayedStart")))
    {
        Context.SendError(TEXT("Nothing to configure: pass bDelayedStart."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // bDelayedStart is an AGameMode (match-state) property; AGameModeBase has no such rule, so a
    // GameModeBase child used to answer success with nothing written.
    FBoolProperty* Prop = CastField<FBoolProperty>(Blueprint->GeneratedClass->FindPropertyByName(TEXT("bDelayedStart")));
    if (!Prop)
    {
        Context.SendError(
            FString::Printf(TEXT("%s derives from %s, which has no bDelayedStart; only GameMode (AGameMode) children have match-state rules. Create the game mode with parentClass /Script/Engine.GameMode to use them."),
                *Blueprint->GetName(), Blueprint->ParentClass ? *Blueprint->ParentClass->GetName() : TEXT("an unknown class")),
            TEXT("NOT_SUPPORTED"));
        return true;
    }
    const bool bDelayedStart = GetBoolField(Context.Payload, TEXT("bDelayedStart"));
    Prop->SetPropertyValue_InContainer(CDO, bDelayedStart);
    CDO->MarkPackageDirty();
    FinishBlueprintMutation(Blueprint, Context.bSave);

    UObject* CompiledCDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    FBoolProperty* CompiledProp = CompiledCDO ? CastField<FBoolProperty>(Blueprint->GeneratedClass->FindPropertyByName(TEXT("bDelayedStart"))) : nullptr;
    if (!CompiledProp || CompiledProp->GetPropertyValue_InContainer(CompiledCDO) != bDelayedStart)
    {
        Context.SendError(TEXT("bDelayedStart did not hold after the Blueprint compiled."), TEXT("SET_PROPERTY_FAILED"));
        return true;
    }
    Context.SendSuccess(MakeBlueprintResponse(
        FString::Printf(TEXT("Set bDelayedStart=%s"), bDelayedStart ? TEXT("true") : TEXT("false")), Blueprint));
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
