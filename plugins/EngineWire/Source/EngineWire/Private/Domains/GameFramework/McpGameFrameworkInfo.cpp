#include "Domains/GameFramework/McpGameFrameworkContext.h"

namespace McpGameFrameworkHandlers
{
// A role the game mode spawns nothing for (a default pawn set to None) reads "None", as configure_game_mode names it;
// left out, it read as a value nobody had looked up.
static FString ClassPathOrNone(const UClass* Class)
{
    return Class ? Class->GetPathName() : FString(TEXT("None"));
}

// The class slots of the game mode whose class default CDO is, each under its payload field's name.
static void AddGameModeDefaults(const UObject* CDO, TSharedPtr<FJsonObject>& InfoObj)
{
    for (const FGameModeClassSlot& Slot : GameModeClassSlots)
    {
        if (const FClassProperty* Property = CastField<FClassProperty>(CDO->GetClass()->FindPropertyByName(Slot.Property)))
        {
            InfoObj->SetStringField(Slot.Field, ClassPathOrNone(Cast<UClass>(Property->GetPropertyValue_InContainer(CDO))));
        }
    }
}

static bool AddBlueprintInfo(FActionContext& Context, TSharedPtr<FJsonObject>& InfoObj)
{
    UBlueprint* Blueprint = LoadBlueprintFromPath(Context.GameModeBlueprint);
    if (!Blueprint || !Blueprint->GeneratedClass) return false;

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (CDO)
    {
        AddGameModeDefaults(CDO, InfoObj);
    }
    InfoObj->SetStringField(TEXT("gameModeClass"), Blueprint->GeneratedClass->GetPathName());
    return true;
}

UClass* ResolveWorldGameMode(UWorld* World, TSharedPtr<FJsonObject>& InfoObj)
{
    AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
    if (GameMode)
    {
        InfoObj->SetStringField(TEXT("source"), TEXT("live"));
        return GameMode->GetClass();
    }

    if (World)
    {
        if (AWorldSettings* WorldSettings = World->GetWorldSettings())
        {
            if (UClass* LevelGameMode = WorldSettings->DefaultGameMode.Get())
            {
                InfoObj->SetStringField(TEXT("source"), TEXT("levelDefault"));
                return LevelGameMode;
            }
        }

        const FString DefaultGameModeStr = UGameMapsSettings::GetGlobalDefaultGameMode();
        if (!DefaultGameModeStr.IsEmpty())
        {
            FSoftClassPath DefaultGameModePath(DefaultGameModeStr);
            if (UClass* ProjectGameMode = DefaultGameModePath.TryLoadClass<AGameModeBase>())
            {
                InfoObj->SetStringField(TEXT("source"), TEXT("projectDefault"));
                return ProjectGameMode;
            }
        }
    }
    return nullptr;
}

bool HandleInfoAction(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_game_framework_info"))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);

    TSharedPtr<FJsonObject> InfoObj = McpHandlerUtils::CreateResultObject();
    if (!Context.GameModeBlueprint.IsEmpty())
    {
        if (!AddBlueprintInfo(Context, InfoObj))
        {
            Context.SendError(
                FString::Printf(TEXT("No GameMode Blueprint at %s."), *Context.GameModeBlueprint),
                TEXT("NOT_FOUND"));
            return true;
        }
    }
    else
    {
        UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld();

        UClass* ResolvedGameModeClass = ResolveWorldGameMode(World, InfoObj);
        if (ResolvedGameModeClass)
        {
            InfoObj->SetStringField(TEXT("gameModeClass"), ResolvedGameModeClass->GetPathName());
            if (const AGameModeBase* CDO = ResolvedGameModeClass->GetDefaultObject<AGameModeBase>())
            {
                AddGameModeDefaults(CDO, InfoObj);
            }
        }
        if (World)
        {
            InfoObj->SetStringField(TEXT("worldName"), World->GetName());
            InfoObj->SetBoolField(TEXT("isPlayInEditor"), World->IsPlayInEditor());
        }
    }

    Response->SetObjectField(TEXT("gameFrameworkInfo"), InfoObj);
    Response->SetStringField(TEXT("message"), TEXT("Game framework info retrieved."));
    Context.SendSuccess(Response);
    return true;
}
}
