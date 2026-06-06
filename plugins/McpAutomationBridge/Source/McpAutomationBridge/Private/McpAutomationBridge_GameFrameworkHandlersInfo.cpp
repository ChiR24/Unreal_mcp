#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
static void AddGeneratedClassProperty(UClass* Class, UObject* CDO, const FName& PropertyName, const FString& ResponseName, TSharedPtr<FJsonObject>& InfoObj)
{
    FClassProperty* Property = CastField<FClassProperty>(Class->FindPropertyByName(PropertyName));
    if (!Property) return;

    UClass* Value = Cast<UClass>(Property->GetPropertyValue_InContainer(CDO));
    if (Value)
    {
        InfoObj->SetStringField(ResponseName, Value->GetPathName());
    }
}

static void AddGameModeDefaults(const AGameModeBase* CDO, TSharedPtr<FJsonObject>& InfoObj)
{
    if (CDO->DefaultPawnClass) InfoObj->SetStringField(TEXT("defaultPawnClass"), CDO->DefaultPawnClass->GetPathName());
    if (CDO->PlayerControllerClass) InfoObj->SetStringField(TEXT("playerControllerClass"), CDO->PlayerControllerClass->GetPathName());
    if (CDO->GameStateClass) InfoObj->SetStringField(TEXT("gameStateClass"), CDO->GameStateClass->GetPathName());
    if (CDO->PlayerStateClass) InfoObj->SetStringField(TEXT("playerStateClass"), CDO->PlayerStateClass->GetPathName());
    if (CDO->HUDClass) InfoObj->SetStringField(TEXT("hudClass"), CDO->HUDClass->GetPathName());
}

static void AddBlueprintInfo(FActionContext& Context, TSharedPtr<FJsonObject>& InfoObj)
{
    UBlueprint* Blueprint = LoadBlueprintFromPath(Context.GameModeBlueprint);
    if (!Blueprint || !Blueprint->GeneratedClass) return;

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (CDO)
    {
        AddGeneratedClassProperty(Blueprint->GeneratedClass, CDO, TEXT("DefaultPawnClass"), TEXT("defaultPawnClass"), InfoObj);
        AddGeneratedClassProperty(Blueprint->GeneratedClass, CDO, TEXT("PlayerControllerClass"), TEXT("playerControllerClass"), InfoObj);
        AddGeneratedClassProperty(Blueprint->GeneratedClass, CDO, TEXT("GameStateClass"), TEXT("gameStateClass"), InfoObj);
        AddGeneratedClassProperty(Blueprint->GeneratedClass, CDO, TEXT("PlayerStateClass"), TEXT("playerStateClass"), InfoObj);
        AddGeneratedClassProperty(Blueprint->GeneratedClass, CDO, TEXT("HUDClass"), TEXT("hudClass"), InfoObj);
    }
    InfoObj->SetStringField(TEXT("gameModeClass"), Blueprint->GeneratedClass->GetPathName());
}

static UClass* ResolveWorldGameMode(UWorld* World, TSharedPtr<FJsonObject>& InfoObj)
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
        AddBlueprintInfo(Context, InfoObj);
    }
    else
    {
        UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
        if (!World && GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }

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
#endif
}
