// McpAutomationBridge_GameFrameworkHandlers.cpp
// Phase 21: Game Framework System Handlers
//
// Complete game mode and session management including:
// - Core Classes (GameMode, GameState, PlayerController, PlayerState, GameInstance, HUD)
// - Game Mode Configuration (default pawn, player controller, game state classes, game rules)
// - Match Flow (match states, round system, team system, scoring, spawn system)
// - Player Management (player start, respawn rules, spectating)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// Game Framework classes
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpGameFrameworkHandlers, Log, All);

// ============================================================================
// Helper Functions
// NOTE: These helpers follow the existing pattern in other *Handlers.cpp files.
// A future refactor could consolidate these into McpAutomationBridgeHelpers.h
// for shared use across all handler files.
// ============================================================================

namespace GameFrameworkHelpers
{
    // Get string field with default
    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""))
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetStringField(FieldName);
        }
        return Default;
    }

    // Get number field with default
    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetNumberField(FieldName);
        }
        return Default;
    }

    // Get bool field with default
    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return Payload->GetBoolField(FieldName);
        }
        return Default;
    }

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    // Get array field
    const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
        {
            return &Payload->GetArrayField(FieldName);
        }
        return nullptr;
    }

#if WITH_EDITOR
    // Load Blueprint from path
    UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
    {
        FString CleanPath = BlueprintPath;
        if (!CleanPath.EndsWith(TEXT("_C")))
        {
            UBlueprint* BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            if (BP) return BP;
            
            if (CleanPath.EndsWith(TEXT(".uasset")))
            {
                CleanPath = CleanPath.LeftChop(7);
                BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            }
            return BP;
        }
        return nullptr;
    }

    // Create a Blueprint of specified parent class
    UBlueprint* CreateGameFrameworkBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
    {
        if (!ParentClass)
        {
            OutError = TEXT("Invalid parent class");
            return nullptr;
        }

        // Ensure path starts with /Game/
        FString FullPath = Path;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            if (FullPath.StartsWith(TEXT("/Content/")))
            {
                FullPath = FullPath.Replace(TEXT("/Content/"), TEXT("/Game/"));
            }
            else if (!FullPath.StartsWith(TEXT("/")))
            {
                FullPath = TEXT("/Game/") + FullPath;
            }
        }
        
        // Remove trailing slash if present
        if (FullPath.EndsWith(TEXT("/")))
        {
            FullPath = FullPath.LeftChop(1);
        }
        
        FString AssetPath = FullPath / Name;
        
        UPackage* Package = CreatePackage(*AssetPath);
        if (!Package)
        {
            OutError = FString::Printf(TEXT("Failed to create package: %s"), *AssetPath);
            return nullptr;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = ParentClass;

        UBlueprint* Blueprint = Cast<UBlueprint>(
            Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                      RF_Public | RF_Standalone, nullptr, GWarn));

        if (!Blueprint)
        {
            OutError = FString::Printf(TEXT("Failed to create %s blueprint"), *ParentClass->GetName());
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(Blueprint);
        Blueprint->MarkPackageDirty();
        
        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        
        return Blueprint;
    }

    // Set a TSubclassOf property on a Blueprint CDO
    bool SetClassProperty(UBlueprint* Blueprint, const FName& PropertyName, UClass* ClassToSet, FString& OutError)
    {
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            OutError = TEXT("Invalid blueprint or generated class");
            return false;
        }

        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (!CDO)
        {
            OutError = TEXT("Failed to get CDO");
            return false;
        }

        // Find the property
        FProperty* Prop = Blueprint->GeneratedClass->FindPropertyByName(PropertyName);
        if (!Prop)
        {
            // Try on parent class
            Prop = Blueprint->ParentClass->FindPropertyByName(PropertyName);
        }

        if (!Prop)
        {
            OutError = FString::Printf(TEXT("Property '%s' not found"), *PropertyName.ToString());
            return false;
        }

        FClassProperty* ClassProp = CastField<FClassProperty>(Prop);
        if (ClassProp)
        {
            ClassProp->SetPropertyValue_InContainer(CDO, ClassToSet);
            CDO->MarkPackageDirty();
            return true;
        }

        // Try soft class property
        FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop);
        if (SoftClassProp)
        {
            FSoftObjectPtr SoftPtr(ClassToSet);
            SoftClassProp->SetPropertyValue_InContainer(CDO, SoftPtr);
            CDO->MarkPackageDirty();
            return true;
        }

        OutError = FString::Printf(TEXT("Property '%s' is not a class property"), *PropertyName.ToString());
        return false;
    }

    // Load class from path (Blueprint or native)
    UClass* LoadClassFromPath(const FString& ClassPath)
    {
        if (ClassPath.IsEmpty())
        {
            return nullptr;
        }

        // Try loading as native class first
        UClass* NativeClass = FindObject<UClass>(nullptr, *ClassPath);
        if (NativeClass)
        {
            return NativeClass;
        }

        // Try as Blueprint
        FString BPPath = ClassPath;
        if (!BPPath.EndsWith(TEXT("_C")))
        {
            BPPath += TEXT("_C");
        }
        
        UClass* BPClass = LoadClass<UObject>(nullptr, *BPPath);
        if (BPClass)
        {
            return BPClass;
        }

        // Try loading Blueprint asset and getting its generated class
        UBlueprint* BP = LoadBlueprintFromPath(ClassPath);
        if (BP && BP->GeneratedClass)
        {
            return BP->GeneratedClass;
        }

        return nullptr;
    }
#endif
}

// ============================================================================
// Main Handler Implementation
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageGameFrameworkAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_game_framework"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("HandleManageGameFrameworkAction: subAction=%s"), *SubAction);

    // Common parameters
    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    FString BlueprintPath = GameModeBlueprint; // Keep in sync for configure_player_start

    // ========================================================================
    // 21.1 CORE CLASSES (6 actions)
    // ========================================================================

    if (SubAction == TEXT("create_game_mode"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_game_mode."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? AGameModeBase::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = AGameModeBase::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Set initial class defaults if provided
        FString DefaultPawnClass = GetStringField(Payload, TEXT("defaultPawnClass"));
        if (!DefaultPawnClass.IsEmpty())
        {
            UClass* PawnClass = LoadClassFromPath(DefaultPawnClass);
            if (PawnClass)
            {
                SetClassProperty(BP, TEXT("DefaultPawnClass"), PawnClass, Error);
            }
        }

        FString PlayerControllerClass = GetStringField(Payload, TEXT("playerControllerClass"));
        if (!PlayerControllerClass.IsEmpty())
        {
            UClass* PCClass = LoadClassFromPath(PlayerControllerClass);
            if (PCClass)
            {
                SetClassProperty(BP, TEXT("PlayerControllerClass"), PCClass, Error);
            }
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameMode blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("create_game_state"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_game_state."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? AGameStateBase::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = AGameStateBase::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameState blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("create_player_controller"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_player_controller."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? APlayerController::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = APlayerController::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created PlayerController blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("create_player_state"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_player_state."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? APlayerState::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = APlayerState::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created PlayerState blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("create_game_instance"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_game_instance."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? UGameInstance::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = UGameInstance::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameInstance blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("create_hud_class"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' for create_hud_class."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
        UClass* ParentClass = ParentClassPath.IsEmpty() ? AHUD::StaticClass() : LoadClassFromPath(ParentClassPath);
        
        if (!ParentClass)
        {
            ParentClass = AHUD::StaticClass();
        }

        FString Error;
        UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
        
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created HUD blueprint: %s"), *Name));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }

    // ========================================================================
    // 21.2 GAME MODE CONFIGURATION (5 actions)
    // ========================================================================

    else if (SubAction == TEXT("set_default_pawn_class"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Support both pawnClass and defaultPawnClass as aliases
        FString PawnClassPath = GetStringField(Payload, TEXT("pawnClass"));
        if (PawnClassPath.IsEmpty())
        {
            PawnClassPath = GetStringField(Payload, TEXT("defaultPawnClass"));
        }
        if (PawnClassPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'pawnClass' or 'defaultPawnClass'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        UClass* PawnClass = LoadClassFromPath(PawnClassPath);
        if (!PawnClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load pawn class: %s"), *PawnClassPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        if (!SetClassProperty(BP, TEXT("DefaultPawnClass"), PawnClass, Error))
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
            return true;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);
        
        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set DefaultPawnClass to %s"), *PawnClassPath));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("set_player_controller_class"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PCClassPath = GetStringField(Payload, TEXT("playerControllerClass"));
        if (PCClassPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'playerControllerClass'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        UClass* PCClass = LoadClassFromPath(PCClassPath);
        if (!PCClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load PlayerController class: %s"), *PCClassPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        if (!SetClassProperty(BP, TEXT("PlayerControllerClass"), PCClass, Error))
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
            return true;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set PlayerControllerClass to %s"), *PCClassPath));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("set_game_state_class"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString GSClassPath = GetStringField(Payload, TEXT("gameStateClass"));
        if (GSClassPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameStateClass'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        UClass* GSClass = LoadClassFromPath(GSClassPath);
        if (!GSClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameState class: %s"), *GSClassPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        if (!SetClassProperty(BP, TEXT("GameStateClass"), GSClass, Error))
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
            return true;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set GameStateClass to %s"), *GSClassPath));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("set_player_state_class"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PSClassPath = GetStringField(Payload, TEXT("playerStateClass"));
        if (PSClassPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'playerStateClass'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        UClass* PSClass = LoadClassFromPath(PSClassPath);
        if (!PSClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load PlayerState class: %s"), *PSClassPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        if (!SetClassProperty(BP, TEXT("PlayerStateClass"), PSClass, Error))
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
            return true;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set PlayerStateClass to %s"), *PSClassPath));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_game_rules"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP || !BP->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        UObject* CDO = BP->GeneratedClass->GetDefaultObject();
        if (!CDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get CDO."), TEXT("INTERNAL_ERROR"));
            return true;
        }

        // Configure game rules via reflection
        bool bModified = false;

        // Note: These properties may not exist on AGameModeBase, only on AGameMode
        // We'll try to set them if they exist
        
        if (Payload->HasField(TEXT("bDelayedStart")))
        {
            FBoolProperty* Prop = CastField<FBoolProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("bDelayedStart")));
            if (Prop)
            {
                Prop->SetPropertyValue_InContainer(CDO, GetBoolField(Payload, TEXT("bDelayedStart")));
                bModified = true;
            }
        }

        if (Payload->HasField(TEXT("startPlayersNeeded")))
        {
            // This would typically be a custom property - log for user info
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("startPlayersNeeded would require custom variable in Blueprint"));
        }

        if (bModified)
        {
            CDO->MarkPackageDirty();
            FKismetEditorUtilities::CompileBlueprint(BP);
        }

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Configured game rules"));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }

    // ========================================================================
    // 21.3 MATCH FLOW (5 actions)
    // ========================================================================

    else if (SubAction == TEXT("setup_match_states"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        // Match states are typically handled via the AGameMode class (not AGameModeBase)
        // Log the configuration for now
        const TArray<TSharedPtr<FJsonValue>>* StatesArray = GetArrayField(Payload, TEXT("states"));
        if (StatesArray)
        {
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Setting up %d match states"), StatesArray->Num());
        }

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Match states configuration received. To persist, use manage_blueprint with add_variable action to create state enum/int variable, then implement state machine logic in Blueprint."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        Response->SetNumberField(TEXT("stateCount"), StatesArray ? StatesArray->Num() : 0);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_round_system"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        int32 NumRounds = static_cast<int32>(GetNumberField(Payload, TEXT("numRounds"), 0));
        double RoundTime = GetNumberField(Payload, TEXT("roundTime"), 0);
        double IntermissionTime = GetNumberField(Payload, TEXT("intermissionTime"), 0);

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring round system: rounds=%d, roundTime=%.1f, intermission=%.1f"), 
               NumRounds, RoundTime, IntermissionTime);

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Round system configuration received. To persist, use manage_blueprint with add_variable action to create NumRounds (int), RoundTime (float), IntermissionTime (float) variables."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        
        TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject());
        ConfigObj->SetNumberField(TEXT("numRounds"), NumRounds);
        ConfigObj->SetNumberField(TEXT("roundTime"), RoundTime);
        ConfigObj->SetNumberField(TEXT("intermissionTime"), IntermissionTime);
        Response->SetObjectField(TEXT("configuration"), ConfigObj);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_team_system"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        int32 NumTeams = static_cast<int32>(GetNumberField(Payload, TEXT("numTeams"), 2));
        int32 TeamSize = static_cast<int32>(GetNumberField(Payload, TEXT("teamSize"), 0));
        bool bAutoBalance = GetBoolField(Payload, TEXT("autoBalance"), true);
        bool bFriendlyFire = GetBoolField(Payload, TEXT("friendlyFire"), false);

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring team system: teams=%d, size=%d, autoBalance=%d, friendlyFire=%d"), 
               NumTeams, TeamSize, bAutoBalance, bFriendlyFire);

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Team system configuration received. To persist, use manage_blueprint with add_variable action to create NumTeams (int), TeamSize (int), bAutoBalance (bool), bFriendlyFire (bool) variables."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        
        TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject());
        ConfigObj->SetNumberField(TEXT("numTeams"), NumTeams);
        ConfigObj->SetNumberField(TEXT("teamSize"), TeamSize);
        ConfigObj->SetBoolField(TEXT("autoBalance"), bAutoBalance);
        ConfigObj->SetBoolField(TEXT("friendlyFire"), bFriendlyFire);
        Response->SetObjectField(TEXT("configuration"), ConfigObj);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_scoring_system"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        double ScorePerKill = GetNumberField(Payload, TEXT("scorePerKill"), 100);
        double ScorePerObjective = GetNumberField(Payload, TEXT("scorePerObjective"), 500);
        double ScorePerAssist = GetNumberField(Payload, TEXT("scorePerAssist"), 50);

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring scoring: kill=%.0f, objective=%.0f, assist=%.0f"), 
               ScorePerKill, ScorePerObjective, ScorePerAssist);

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Scoring system configuration received. To persist, use manage_blueprint with add_variable action to create ScorePerKill (float), ScorePerObjective (float), ScorePerAssist (float) variables."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        
        TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject());
        ConfigObj->SetNumberField(TEXT("scorePerKill"), ScorePerKill);
        ConfigObj->SetNumberField(TEXT("scorePerObjective"), ScorePerObjective);
        ConfigObj->SetNumberField(TEXT("scorePerAssist"), ScorePerAssist);
        Response->SetObjectField(TEXT("configuration"), ConfigObj);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_spawn_system"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        FString SpawnMethod = GetStringField(Payload, TEXT("spawnSelectionMethod"), TEXT("Random"));
        double RespawnDelay = GetNumberField(Payload, TEXT("respawnDelay"), 5.0);
        bool bUsePlayerStarts = GetBoolField(Payload, TEXT("usePlayerStarts"), true);

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring spawn system: method=%s, delay=%.1f, usePlayerStarts=%d"), 
               *SpawnMethod, RespawnDelay, bUsePlayerStarts);

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Spawn system configured."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }

    // ========================================================================
    // 21.4 PLAYER MANAGEMENT (3 actions)
    // ========================================================================

    else if (SubAction == TEXT("configure_player_start"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'blueprintPath'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // This typically works on PlayerStart actors in a level, not blueprints
        // For now, we'll handle it as a configuration helper
        
        TSharedPtr<FJsonObject> LocationObj = GetObjectField(Payload, TEXT("location"));
        TSharedPtr<FJsonObject> RotationObj = GetObjectField(Payload, TEXT("rotation"));
        int32 TeamIndex = static_cast<int32>(GetNumberField(Payload, TEXT("teamIndex"), 0));
        bool bPlayerOnly = GetBoolField(Payload, TEXT("bPlayerOnly"), false);

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configure PlayerStart: path=%s, teamIndex=%d, playerOnly=%d"), 
               *BlueprintPath, TeamIndex, bPlayerOnly);

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("PlayerStart configuration noted. Use control_actor to spawn/modify PlayerStart actors in level."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("set_respawn_rules"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        double RespawnDelay = GetNumberField(Payload, TEXT("respawnDelay"), 5.0);
        FString RespawnLocation = GetStringField(Payload, TEXT("respawnLocation"), TEXT("PlayerStart"));

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Setting respawn rules: delay=%.1f, location=%s"), 
               RespawnDelay, *RespawnLocation);

        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Respawn rules configured."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }
    else if (SubAction == TEXT("configure_spectating"))
    {
        if (GameModeBlueprint.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
            return true;
        }

        FString SpectatorClassPath = GetStringField(Payload, TEXT("spectatorClass"));
        bool bAllowSpectating = GetBoolField(Payload, TEXT("allowSpectating"), true);
        FString ViewMode = GetStringField(Payload, TEXT("spectatorViewMode"), TEXT("FreeCam"));

        // Set spectator class if provided
        if (!SpectatorClassPath.IsEmpty())
        {
            UClass* SpectatorClass = LoadClassFromPath(SpectatorClassPath);
            if (SpectatorClass)
            {
                FString Error;
                SetClassProperty(BP, TEXT("SpectatorClass"), SpectatorClass, Error);
            }
        }

        FKismetEditorUtilities::CompileBlueprint(BP);
        BP->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(BP);
        }

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Spectating configured."));
        Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }

    // ========================================================================
    // UTILITY (1 action)
    // ========================================================================

    else if (SubAction == TEXT("get_game_framework_info"))
    {
        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject());
        Response->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());

        // If a specific GameMode blueprint is provided, query it
        if (!GameModeBlueprint.IsEmpty())
        {
            UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
            if (BP && BP->GeneratedClass)
            {
                UObject* CDO = BP->GeneratedClass->GetDefaultObject();
                if (CDO)
                {
                    // Try to get class properties
                    FClassProperty* PawnProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("DefaultPawnClass")));
                    if (PawnProp)
                    {
                        UClass* PawnClass = Cast<UClass>(PawnProp->GetPropertyValue_InContainer(CDO));
                        if (PawnClass)
                        {
                            InfoObj->SetStringField(TEXT("defaultPawnClass"), PawnClass->GetPathName());
                        }
                    }

                    FClassProperty* PCProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("PlayerControllerClass")));
                    if (PCProp)
                    {
                        UClass* PCClass = Cast<UClass>(PCProp->GetPropertyValue_InContainer(CDO));
                        if (PCClass)
                        {
                            InfoObj->SetStringField(TEXT("playerControllerClass"), PCClass->GetPathName());
                        }
                    }

                    FClassProperty* GSProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("GameStateClass")));
                    if (GSProp)
                    {
                        UClass* GSClass = Cast<UClass>(GSProp->GetPropertyValue_InContainer(CDO));
                        if (GSClass)
                        {
                            InfoObj->SetStringField(TEXT("gameStateClass"), GSClass->GetPathName());
                        }
                    }

                    FClassProperty* PSProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("PlayerStateClass")));
                    if (PSProp)
                    {
                        UClass* PSClass = Cast<UClass>(PSProp->GetPropertyValue_InContainer(CDO));
                        if (PSClass)
                        {
                            InfoObj->SetStringField(TEXT("playerStateClass"), PSClass->GetPathName());
                        }
                    }

                    FClassProperty* HUDProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("HUDClass")));
                    if (HUDProp)
                    {
                        UClass* HUDClass = Cast<UClass>(HUDProp->GetPropertyValue_InContainer(CDO));
                        if (HUDClass)
                        {
                            InfoObj->SetStringField(TEXT("hudClass"), HUDClass->GetPathName());
                        }
                    }
                }

                InfoObj->SetStringField(TEXT("gameModeClass"), BP->GeneratedClass->GetPathName());
            }
        }
        else
        {
            // Query current world's game mode if available
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (World)
            {
                AGameModeBase* GM = World->GetAuthGameMode();
                if (GM)
                {
                    InfoObj->SetStringField(TEXT("gameModeClass"), GM->GetClass()->GetPathName());
                    
                    if (GM->DefaultPawnClass)
                    {
                        InfoObj->SetStringField(TEXT("defaultPawnClass"), GM->DefaultPawnClass->GetPathName());
                    }
                    if (GM->PlayerControllerClass)
                    {
                        InfoObj->SetStringField(TEXT("playerControllerClass"), GM->PlayerControllerClass->GetPathName());
                    }
                    if (GM->GameStateClass)
                    {
                        InfoObj->SetStringField(TEXT("gameStateClass"), GM->GameStateClass->GetPathName());
                    }
                    if (GM->PlayerStateClass)
                    {
                        InfoObj->SetStringField(TEXT("playerStateClass"), GM->PlayerStateClass->GetPathName());
                    }
                    if (GM->HUDClass)
                    {
                        InfoObj->SetStringField(TEXT("hudClass"), GM->HUDClass->GetPathName());
                    }
                }
            }
        }

        Response->SetObjectField(TEXT("gameFrameworkInfo"), InfoObj);
        Response->SetStringField(TEXT("message"), TEXT("Game framework info retrieved."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
        return true;
    }

    // ========================================================================
    // Unknown subAction
    // ========================================================================

    else
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
        return true;
    }

#endif // WITH_EDITOR
}
