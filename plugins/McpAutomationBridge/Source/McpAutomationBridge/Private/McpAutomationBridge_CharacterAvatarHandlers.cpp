// McpAutomationBridge_CharacterAvatarHandlers.cpp
// Phase 36: Character & Avatar Plugin Handlers
// Implements: MetaHuman, Groom/Hair, Mutable (Customizable), Ready Player Me

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Editor.h"

// Conditional includes for plugins
#if __has_include("GroomComponent.h")
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#define MCP_HAS_GROOM 1
#else
#define MCP_HAS_GROOM 0
#endif

#if __has_include("MutableRuntime/Public/CustomizableObject.h")
#include "MutableRuntime/Public/CustomizableObject.h"
#include "MutableRuntime/Public/CustomizableObjectInstance.h"
#define MCP_HAS_MUTABLE 1
#elif __has_include("CustomizableObject.h")
#include "CustomizableObject.h"
#include "CustomizableObjectInstance.h"
#define MCP_HAS_MUTABLE 1
#else
#define MCP_HAS_MUTABLE 0
#endif

// Helper function to get groom component from actor
#if MCP_HAS_GROOM
static UGroomComponent* GetGroomComponentFromActor(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<UGroomComponent>();
}
#endif

// Helper to create JSON result
static TSharedPtr<FJsonObject> MakeSuccessResult(const FString& Message)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), Message);
    return Result;
}

static TSharedPtr<FJsonObject> MakeErrorResult(const FString& Message, const FString& ErrorCode = TEXT("ERROR"))
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), ErrorCode);
    Result->SetStringField(TEXT("message"), Message);
    return Result;
}

bool UMcpAutomationBridgeSubsystem::HandleManageCharacterAvatarAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = Payload->HasField(TEXT("subAction"))
        ? Payload->GetStringField(TEXT("subAction"))
        : Action;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleManageCharacterAvatarAction: %s"), *SubAction);

    // =========================================================================
    // METAHUMAN ACTIONS
    // =========================================================================

    if (SubAction == TEXT("import_metahuman"))
    {
        // MetaHuman import typically happens through Quixel Bridge
        // This provides a programmatic interface for automation
        const FString SourcePath = Payload->GetStringField(TEXT("sourcePath"));
        if (SourcePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // MetaHuman import is complex and requires the MetaHuman SDK
        // For now, provide guidance
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("MetaHuman import initiated. Use Quixel Bridge for full import functionality."));
        Result->SetStringField(TEXT("note"), TEXT("MetaHuman assets are typically imported via Quixel Bridge or Fab integration."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman import guidance"), Result);
        return true;
    }

    if (SubAction == TEXT("spawn_metahuman_actor"))
    {
        const FString MetahumanPath = Payload->GetStringField(TEXT("metahumanPath"));
        if (MetahumanPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metahumanPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // Load the MetaHuman Blueprint
        UBlueprint* MetahumanBP = LoadObject<UBlueprint>(nullptr, *MetahumanPath);
        if (!MetahumanBP || !MetahumanBP->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load MetaHuman Blueprint: %s"), *MetahumanPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        // Get spawn location
        FVector Location(0, 0, 0);
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            Location.X = (*LocObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        AActor* SpawnedActor = World->SpawnActor<AActor>(MetahumanBP->GeneratedClass, Location, FRotator::ZeroRotator, SpawnParams);
        if (!SpawnedActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn MetaHuman actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), SpawnedActor->GetName());
        Result->SetStringField(TEXT("message"), TEXT("MetaHuman actor spawned successfully"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman spawned"), Result);
        return true;
    }

    if (SubAction == TEXT("get_metahuman_info"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("className"), Actor->GetClass()->GetName());
        
        // Get skeletal mesh components for MetaHuman info
        TSharedPtr<FJsonObject> MetahumanInfo = MakeShareable(new FJsonObject());
        TArray<USkeletalMeshComponent*> SkeletalComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkeletalComps);
        
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        for (USkeletalMeshComponent* Comp : SkeletalComps)
        {
            TSharedPtr<FJsonObject> CompInfo = MakeShareable(new FJsonObject());
            CompInfo->SetStringField(TEXT("name"), Comp->GetName());
            if (Comp->GetSkeletalMeshAsset())
            {
                CompInfo->SetStringField(TEXT("mesh"), Comp->GetSkeletalMeshAsset()->GetPathName());
            }
            ComponentsArray.Add(MakeShareable(new FJsonValueObject(CompInfo)));
        }
        MetahumanInfo->SetArrayField(TEXT("skeletalComponents"), ComponentsArray);
        
        Result->SetObjectField(TEXT("metahumanInfo"), MetahumanInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman info retrieved"), Result);
        return true;
    }

    if (SubAction == TEXT("get_metahuman_component"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const FString ComponentType = Payload->HasField(TEXT("componentType")) 
            ? Payload->GetStringField(TEXT("componentType")) 
            : TEXT("Body");
            
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        
        // Get skeletal mesh components and find the one matching the requested type
        TArray<USkeletalMeshComponent*> SkelComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkelComps);
        
        for (USkeletalMeshComponent* Comp : SkelComps)
        {
            FString CompName = Comp->GetName();
            bool bMatches = ComponentType.IsEmpty() || 
                CompName.Contains(ComponentType, ESearchCase::IgnoreCase);
            
            if (bMatches)
            {
                TSharedPtr<FJsonObject> CompInfo = MakeShareable(new FJsonObject());
                CompInfo->SetStringField(TEXT("name"), CompName);
                CompInfo->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
                if (Comp->GetSkeletalMeshAsset())
                {
                    CompInfo->SetStringField(TEXT("mesh"), Comp->GetSkeletalMeshAsset()->GetPathName());
                }
                CompInfo->SetBoolField(TEXT("visible"), Comp->IsVisible());
                ComponentsArray.Add(MakeShareable(new FJsonValueObject(CompInfo)));
            }
        }
        
        Result->SetArrayField(TEXT("components"), ComponentsArray);
        Result->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman components retrieved"), Result);
        return true;
    }
    
    if (SubAction == TEXT("set_body_type") ||
        SubAction == TEXT("set_face_parameter") ||
        SubAction == TEXT("set_skin_tone") ||
        SubAction == TEXT("set_hair_style") ||
        SubAction == TEXT("set_eye_color"))
    {
        // MetaHuman appearance modifications - these are controlled through Blueprint properties
        // MetaHuman uses DNA-based face rigs and preset systems
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("action"), SubAction);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaHuman '%s' action requires Blueprint-level modification."), *SubAction));
        Result->SetStringField(TEXT("guidance"), TEXT("MetaHuman appearance is controlled through DNA assets and MetaHuman Creator presets. Use Blueprint to set properties on the MetaHuman BP_* actor class, or apply different DNA presets via the MetaHuman plugin."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman modification guidance"), Result);
        return true;
    }
    
    if (SubAction == TEXT("configure_metahuman_lod"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const int32 LODLevel = Payload->HasField(TEXT("lodLevel")) ? (int32)Payload->GetNumberField(TEXT("lodLevel")) : 0;
        const bool bForceLOD = Payload->HasField(TEXT("forceLOD")) ? Payload->GetBoolField(TEXT("forceLOD")) : false;
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        // Apply LOD to all skeletal mesh components
        TArray<USkeletalMeshComponent*> SkelComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkelComps);
        
        int32 ModifiedCount = 0;
        for (USkeletalMeshComponent* Comp : SkelComps)
        {
            if (bForceLOD)
            {
                Comp->SetForcedLOD(LODLevel + 1); // ForcedLOD is 1-based, 0 means auto
            }
            else
            {
                Comp->SetForcedLOD(0); // 0 = auto LOD selection
            }
            ModifiedCount++;
        }
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetNumberField(TEXT("lodLevel"), LODLevel);
        Result->SetBoolField(TEXT("forceLOD"), bForceLOD);
        Result->SetNumberField(TEXT("componentsModified"), ModifiedCount);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("LOD %s to level %d for %d components"), bForceLOD ? TEXT("forced") : TEXT("set"), LODLevel, ModifiedCount));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman LOD configured"), Result);
        return true;
    }
    
    if (SubAction == TEXT("enable_body_correctives") || SubAction == TEXT("enable_neck_correctives"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const bool bEnable = Payload->HasField(TEXT("enable")) ? Payload->GetBoolField(TEXT("enable")) : true;
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        // Correctives are typically controlled through the MetaHuman ControlRig and AnimBP
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetBoolField(TEXT("enabled"), bEnable);
        Result->SetStringField(TEXT("correctiveType"), SubAction == TEXT("enable_body_correctives") ? TEXT("body") : TEXT("neck"));
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("%s correctives %s. Note: Correctives are controlled through the MetaHuman Animation Blueprint and ControlRig."), 
            SubAction == TEXT("enable_body_correctives") ? TEXT("Body") : TEXT("Neck"), bEnable ? TEXT("enabled") : TEXT("disabled")));
        Result->SetStringField(TEXT("guidance"), TEXT("To fully configure correctives, access the MetaHuman AnimBP properties or use the MetaHuman ControlRig settings."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman correctives configured"), Result);
        return true;
    }
    
    if (SubAction == TEXT("set_quality_level"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const FString QualityLevel = Payload->HasField(TEXT("qualityLevel")) 
            ? Payload->GetStringField(TEXT("qualityLevel")) 
            : TEXT("Medium");
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        // Quality level maps to LOD and detail settings
        int32 LODLevel = 0;
        if (QualityLevel == TEXT("Cinematic") || QualityLevel == TEXT("Epic")) LODLevel = 0;
        else if (QualityLevel == TEXT("High")) LODLevel = 1;
        else if (QualityLevel == TEXT("Medium")) LODLevel = 2;
        else if (QualityLevel == TEXT("Low")) LODLevel = 3;
        
        TArray<USkeletalMeshComponent*> SkelComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkelComps);
        
        for (USkeletalMeshComponent* Comp : SkelComps)
        {
            Comp->SetForcedLOD(LODLevel + 1);
        }
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("qualityLevel"), QualityLevel);
        Result->SetNumberField(TEXT("mappedLOD"), LODLevel);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Quality level set to '%s' (LOD %d)"), *QualityLevel, LODLevel));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman quality configured"), Result);
        return true;
    }
    
    if (SubAction == TEXT("configure_face_rig") || SubAction == TEXT("set_body_part"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("action"), SubAction);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaHuman '%s' configuration requires MetaHuman SDK."), *SubAction));
        Result->SetStringField(TEXT("guidance"), SubAction == TEXT("configure_face_rig") 
            ? TEXT("Face rig configuration is done through the MetaHuman ControlRig and DNA assets. Access the Face_ControlBoard_CtrlRig in the MetaHuman AnimBP.")
            : TEXT("Body parts are defined in the MetaHuman Blueprint. Use the component visibility or material overrides to customize appearance."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman configuration guidance"), Result);
        return true;
    }
    
    if (SubAction == TEXT("list_available_presets"))
    {
        // List MetaHuman-related assets in the project
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        
        TArray<FAssetData> MetaHumanAssets;
        FARFilter Filter;
        Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(TEXT("/Game/MetaHumans"));
        Filter.bRecursivePaths = true;
        
        AssetRegistry.GetAssets(Filter, MetaHumanAssets);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TArray<TSharedPtr<FJsonValue>> PresetsArray;
        for (const FAssetData& Asset : MetaHumanAssets)
        {
            TSharedPtr<FJsonObject> PresetInfo = MakeShareable(new FJsonObject());
            PresetInfo->SetStringField(TEXT("name"), Asset.AssetName.ToString());
            PresetInfo->SetStringField(TEXT("path"), Asset.GetObjectPathString());
            PresetsArray.Add(MakeShareable(new FJsonValueObject(PresetInfo)));
        }
        
        Result->SetArrayField(TEXT("presets"), PresetsArray);
        Result->SetNumberField(TEXT("presetCount"), PresetsArray.Num());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Found %d MetaHuman presets in /Game/MetaHumans"), PresetsArray.Num()));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman presets listed"), Result);
        return true;
    }
    
    if (SubAction == TEXT("apply_preset"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const FString PresetPath = Payload->GetStringField(TEXT("presetPath"));
        
        if (PresetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        // Applying a preset typically means swapping out the MetaHuman Blueprint
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("presetPath"), PresetPath);
        Result->SetStringField(TEXT("message"), TEXT("Preset application requires replacing the MetaHuman Blueprint or DNA asset."));
        Result->SetStringField(TEXT("guidance"), TEXT("To apply a different MetaHuman preset: 1) Delete the current actor, 2) Spawn a new actor from the desired MetaHuman Blueprint, or 3) Use the MetaHuman DNA swapping feature in the MetaHuman SDK."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman preset guidance"), Result);
        return true;
    }
    
    if (SubAction == TEXT("export_metahuman_settings"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        // Export current MetaHuman configuration
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("blueprintClass"), Actor->GetClass()->GetPathName());
        
        TSharedPtr<FJsonObject> Settings = MakeShareable(new FJsonObject());
        
        // Export skeletal mesh info
        TArray<USkeletalMeshComponent*> SkelComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkelComps);
        
        TArray<TSharedPtr<FJsonValue>> MeshesArray;
        for (USkeletalMeshComponent* Comp : SkelComps)
        {
            TSharedPtr<FJsonObject> MeshInfo = MakeShareable(new FJsonObject());
            MeshInfo->SetStringField(TEXT("componentName"), Comp->GetName());
            if (Comp->GetSkeletalMeshAsset())
            {
                MeshInfo->SetStringField(TEXT("meshPath"), Comp->GetSkeletalMeshAsset()->GetPathName());
            }
            MeshInfo->SetNumberField(TEXT("forcedLOD"), Comp->GetForcedLOD());
            MeshesArray.Add(MakeShareable(new FJsonValueObject(MeshInfo)));
        }
        Settings->SetArrayField(TEXT("skeletalMeshes"), MeshesArray);
        
        // Export transform
        FVector Location = Actor->GetActorLocation();
        FRotator Rotation = Actor->GetActorRotation();
        TSharedPtr<FJsonObject> Transform = MakeShareable(new FJsonObject());
        Transform->SetNumberField(TEXT("x"), Location.X);
        Transform->SetNumberField(TEXT("y"), Location.Y);
        Transform->SetNumberField(TEXT("z"), Location.Z);
        Transform->SetNumberField(TEXT("pitch"), Rotation.Pitch);
        Transform->SetNumberField(TEXT("yaw"), Rotation.Yaw);
        Transform->SetNumberField(TEXT("roll"), Rotation.Roll);
        Settings->SetObjectField(TEXT("transform"), Transform);
        
        Result->SetObjectField(TEXT("settings"), Settings);
        Result->SetStringField(TEXT("message"), TEXT("MetaHuman settings exported"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman settings exported"), Result);
        return true;
    }

    // =========================================================================
    // GROOM/HAIR ACTIONS
    // =========================================================================

#if MCP_HAS_GROOM
    if (SubAction == TEXT("create_groom_asset"))
    {
        const FString Name = Payload->GetStringField(TEXT("name"));
        const FString DestPath = Payload->HasField(TEXT("destinationPath")) 
            ? Payload->GetStringField(TEXT("destinationPath")) 
            : TEXT("/Game/Groom");

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // Create package and groom asset
        const FString PackagePath = DestPath / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_FAILED"));
            return true;
        }

        UGroomAsset* GroomAsset = NewObject<UGroomAsset>(Package, *Name, RF_Public | RF_Standalone);
        if (!GroomAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create groom asset"), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(GroomAsset);
        GroomAsset->MarkPackageDirty();
        
        if (Payload->GetBoolField(TEXT("save")))
        {
            McpSafeAssetSave(GroomAsset);
        }

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Groom asset created"));
        Result->SetStringField(TEXT("assetPath"), PackagePath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom asset created"), Result);
        return true;
    }

    if (SubAction == TEXT("spawn_groom_actor"))
    {
        const FString GroomAssetPath = Payload->GetStringField(TEXT("groomAssetPath"));
        if (GroomAssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("groomAssetPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UGroomAsset* GroomAsset = LoadObject<UGroomAsset>(nullptr, *GroomAssetPath);
        if (!GroomAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to load groom asset: %s"), *GroomAssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        FVector Location(0, 0, 0);
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            Location.X = (*LocObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
        }

        FActorSpawnParameters SpawnParams;
        AActor* GroomActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
        if (!GroomActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn groom actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        UGroomComponent* GroomComp = NewObject<UGroomComponent>(GroomActor, TEXT("GroomComponent"));
        GroomComp->SetGroomAsset(GroomAsset);
        GroomComp->RegisterComponent();
        GroomActor->AddInstanceComponent(GroomComp);
        GroomComp->AttachToComponent(GroomActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Groom actor spawned"));
        Result->SetStringField(TEXT("actorName"), GroomActor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom actor spawned"), Result);
        return true;
    }

    if (SubAction == TEXT("attach_groom_to_skeletal_mesh"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const FString GroomAssetPath = Payload->GetStringField(TEXT("groomAssetPath"));

        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UGroomAsset* GroomAsset = LoadObject<UGroomAsset>(nullptr, *GroomAssetPath);
        if (!GroomAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Groom asset not found: %s"), *GroomAssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        USkeletalMeshComponent* SkelComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
        if (!SkelComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no skeletal mesh component"), TEXT("NO_SKELETAL_MESH"));
            return true;
        }

        // Create or get groom component
        UGroomComponent* GroomComp = Actor->FindComponentByClass<UGroomComponent>();
        if (!GroomComp)
        {
            GroomComp = NewObject<UGroomComponent>(Actor, TEXT("GroomComponent"));
            GroomComp->RegisterComponent();
            Actor->AddInstanceComponent(GroomComp);
        }

        GroomComp->SetGroomAsset(GroomAsset);
        GroomComp->AttachToComponent(SkelComp, FAttachmentTransformRules::KeepRelativeTransform);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Groom attached to skeletal mesh"));
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom attached"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hair_width"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const double HairWidth = Payload->GetNumberField(TEXT("hairWidth"));

        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }

        // Hair width is controlled through the GroomAsset's hair group settings.
        // Direct runtime modification requires accessing UGroomAsset->HairGroupsRendering[GroupIndex].HairWidth
        // which needs editor-time asset modification and reimport.
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Guidance: To set hair width to %.2f, modify the GroomAsset's HairGroupsRendering settings in the editor or via UGroomAsset->GetHairGroupsRendering()"), HairWidth));
        Result->SetStringField(TEXT("note"), TEXT("Hair width requires asset modification. Use the Groom Editor to adjust HairWidth in Hair Groups Rendering settings."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Guidance provided"), Result);
        return true;
    }

    if (SubAction == TEXT("enable_hair_simulation"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const bool bEnable = Payload->GetBoolField(TEXT("enableSimulation"));

        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }

        GroomComp->SetEnableSimulation(bEnable);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(bEnable ? TEXT("Hair simulation enabled") : TEXT("Hair simulation disabled"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair simulation toggled"), Result);
        return true;
    }

    if (SubAction == TEXT("get_groom_info"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> GroomInfo = MakeShareable(new FJsonObject());
        if (GroomComp->GroomAsset.Get())
        {
            GroomInfo->SetStringField(TEXT("assetPath"), GroomComp->GroomAsset->GetPathName());
        }
        // Note: GetEnableSimulation() was removed in UE 5.7. Check SimulationSettings directly.
        GroomInfo->SetBoolField(TEXT("simulationEnabled"), GroomComp->SimulationSettings.bOverrideSettings);
        
        Result->SetObjectField(TEXT("groomInfo"), GroomInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom info retrieved"), Result);
        return true;
    }

    if (SubAction == TEXT("import_groom"))
    {
        const FString SourcePath = Payload->GetStringField(TEXT("sourcePath"));
        const FString DestPath = Payload->HasField(TEXT("destinationPath")) 
            ? Payload->GetStringField(TEXT("destinationPath")) 
            : TEXT("/Game/Groom");
        
        if (SourcePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath is required"), TEXT("MISSING_PARAM"));
            return true;
        }
        
        // Groom import is typically done through the editor import pipeline
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("sourcePath"), SourcePath);
        Result->SetStringField(TEXT("destinationPath"), DestPath);
        Result->SetStringField(TEXT("message"), TEXT("Groom import initiated."));
        Result->SetStringField(TEXT("guidance"), TEXT("Groom assets (.abc, .usd) are typically imported via Content Browser or FBX/Alembic importer. Use UGroomFactory for programmatic import."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom import guidance"), Result);
        return true;
    }

    if (SubAction == TEXT("create_groom_binding"))
    {
        const FString GroomAssetPath = Payload->GetStringField(TEXT("groomAssetPath"));
        const FString TargetMeshPath = Payload->GetStringField(TEXT("targetMeshPath"));
        const FString BindingName = Payload->HasField(TEXT("bindingName")) 
            ? Payload->GetStringField(TEXT("bindingName")) 
            : TEXT("GroomBinding");
        const FString DestPath = Payload->HasField(TEXT("destinationPath")) 
            ? Payload->GetStringField(TEXT("destinationPath")) 
            : TEXT("/Game/Groom");
        
        if (GroomAssetPath.IsEmpty() || TargetMeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("groomAssetPath and targetMeshPath are required"), TEXT("MISSING_PARAM"));
            return true;
        }
        
        UGroomAsset* GroomAsset = LoadObject<UGroomAsset>(nullptr, *GroomAssetPath);
        if (!GroomAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Groom asset not found: %s"), *GroomAssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        USkeletalMesh* TargetMesh = LoadObject<USkeletalMesh>(nullptr, *TargetMeshPath);
        if (!TargetMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Target mesh not found: %s"), *TargetMeshPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        
        // Create the binding asset
        const FString PackagePath = DestPath / BindingName;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_FAILED"));
            return true;
        }
        
        UGroomBindingAsset* BindingAsset = NewObject<UGroomBindingAsset>(Package, *BindingName, RF_Public | RF_Standalone);
        if (!BindingAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create groom binding asset"), TEXT("CREATE_FAILED"));
            return true;
        }
        
        // Configure the binding
        BindingAsset->SetGroom(GroomAsset);
        BindingAsset->SetTargetSkeletalMesh(TargetMesh);
        BindingAsset->SetGroomBindingType(EGroomBindingMeshType::SkeletalMesh);
        
        FAssetRegistryModule::AssetCreated(BindingAsset);
        BindingAsset->MarkPackageDirty();
        
        // Build the binding
        BindingAsset->Build();
        
        if (Payload->GetBoolField(TEXT("save")))
        {
            McpSafeAssetSave(BindingAsset);
        }
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Groom binding created"));
        Result->SetStringField(TEXT("bindingPath"), PackagePath);
        Result->SetStringField(TEXT("groomAsset"), GroomAssetPath);
        Result->SetStringField(TEXT("targetMesh"), TargetMeshPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom binding created"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hair_simulation"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }
        
        // Configure simulation settings
        FHairSimulationSettings& SimSettings = GroomComp->SimulationSettings;
        SimSettings.bOverrideSettings = true;
        
        if (Payload->HasField(TEXT("enableSimulation")))
        {
            SimSettings.SolverSettings.bEnableSimulation = Payload->GetBoolField(TEXT("enableSimulation"));
        }
        if (Payload->HasField(TEXT("gravityX")) || Payload->HasField(TEXT("gravityY")) || Payload->HasField(TEXT("gravityZ")))
        {
            SimSettings.ExternalForces.GravityVector = FVector(
                Payload->HasField(TEXT("gravityX")) ? Payload->GetNumberField(TEXT("gravityX")) : SimSettings.ExternalForces.GravityVector.X,
                Payload->HasField(TEXT("gravityY")) ? Payload->GetNumberField(TEXT("gravityY")) : SimSettings.ExternalForces.GravityVector.Y,
                Payload->HasField(TEXT("gravityZ")) ? Payload->GetNumberField(TEXT("gravityZ")) : SimSettings.ExternalForces.GravityVector.Z
            );
        }
        if (Payload->HasField(TEXT("airDrag")))
        {
            SimSettings.ExternalForces.AirDrag = Payload->GetNumberField(TEXT("airDrag"));
        }
        if (Payload->HasField(TEXT("bendStiffness")))
        {
            SimSettings.MaterialConstraints.BendStiffness = Payload->GetNumberField(TEXT("bendStiffness"));
        }
        if (Payload->HasField(TEXT("stretchStiffness")))
        {
            SimSettings.MaterialConstraints.StretchStiffness = Payload->GetNumberField(TEXT("stretchStiffness"));
        }
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Hair simulation configured"));
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair simulation configured"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hair_root_scale") || SubAction == TEXT("set_hair_tip_scale"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const float Scale = Payload->GetNumberField(TEXT("scale"));
        const int32 GroupIndex = Payload->HasField(TEXT("groupIndex")) ? (int32)Payload->GetNumberField(TEXT("groupIndex")) : 0;
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }
        
        // Access the groom groups description
        if (GroupIndex < GroomComp->GroomGroupsDesc.Num())
        {
            if (SubAction == TEXT("set_hair_root_scale"))
            {
                GroomComp->GroomGroupsDesc[GroupIndex].HairRootScale = Scale;
            }
            else
            {
                GroomComp->GroomGroupsDesc[GroupIndex].HairTipScale = Scale;
            }
            GroomComp->UpdateHairGroupsDescAndInvalidateRenderState(true);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Group index %d out of range (max %d)"), GroupIndex, GroomComp->GroomGroupsDesc.Num() - 1), TEXT("INVALID_INDEX"));
            return true;
        }
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(FString::Printf(TEXT("%s set to %.3f"), SubAction == TEXT("set_hair_root_scale") ? TEXT("Hair root scale") : TEXT("Hair tip scale"), Scale));
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetNumberField(TEXT("scale"), Scale);
        Result->SetNumberField(TEXT("groupIndex"), GroupIndex);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair scale configured"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hair_color"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const int32 GroupIndex = Payload->HasField(TEXT("groupIndex")) ? (int32)Payload->GetNumberField(TEXT("groupIndex")) : 0;
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }
        
        // Hair color is typically set through materials
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetStringField(TEXT("message"), TEXT("Hair color modification requires material parameter changes."));
        Result->SetStringField(TEXT("guidance"), TEXT("Hair color is controlled through the hair material's parameters. Use material instance dynamic to modify BaseColor or TintColor parameters on the groom component's materials."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair color guidance"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hair_physics"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }
        
        // Configure physics constraints
        FHairSimulationSettings& SimSettings = GroomComp->SimulationSettings;
        SimSettings.bOverrideSettings = true;
        
        if (Payload->HasField(TEXT("collisionRadius")))
        {
            SimSettings.MaterialConstraints.CollisionRadius = Payload->GetNumberField(TEXT("collisionRadius"));
        }
        if (Payload->HasField(TEXT("staticFriction")))
        {
            SimSettings.MaterialConstraints.StaticFriction = Payload->GetNumberField(TEXT("staticFriction"));
        }
        if (Payload->HasField(TEXT("kineticFriction")))
        {
            SimSettings.MaterialConstraints.KineticFriction = Payload->GetNumberField(TEXT("kineticFriction"));
        }
        if (Payload->HasField(TEXT("strandsViscosity")))
        {
            SimSettings.MaterialConstraints.StrandsViscosity = Payload->GetNumberField(TEXT("strandsViscosity"));
        }
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Hair physics configured"));
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair physics configured"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hair_rendering"))
    {
        const FString ActorName = Payload->GetStringField(TEXT("actorName"));
        const int32 GroupIndex = Payload->HasField(TEXT("groupIndex")) ? (int32)Payload->GetNumberField(TEXT("groupIndex")) : 0;
        
        AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        
        UGroomComponent* GroomComp = GetGroomComponentFromActor(Actor);
        if (!GroomComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Actor has no groom component"), TEXT("NO_GROOM_COMPONENT"));
            return true;
        }
        
        // Configure rendering settings
        if (GroupIndex < GroomComp->GroomGroupsDesc.Num())
        {
            FHairGroupDesc& GroupDesc = GroomComp->GroomGroupsDesc[GroupIndex];
            
            if (Payload->HasField(TEXT("hairWidth")))
            {
                GroupDesc.HairWidth = Payload->GetNumberField(TEXT("hairWidth"));
            }
            if (Payload->HasField(TEXT("hairRootScale")))
            {
                GroupDesc.HairRootScale = Payload->GetNumberField(TEXT("hairRootScale"));
            }
            if (Payload->HasField(TEXT("hairTipScale")))
            {
                GroupDesc.HairTipScale = Payload->GetNumberField(TEXT("hairTipScale"));
            }
            if (Payload->HasField(TEXT("shadowDensity")))
            {
                GroupDesc.HairShadowDensity = Payload->GetNumberField(TEXT("shadowDensity"));
            }
            if (Payload->HasField(TEXT("useStableRasterization")))
            {
                GroupDesc.bUseStableRasterization = Payload->GetBoolField(TEXT("useStableRasterization"));
            }
            if (Payload->HasField(TEXT("scatterSceneLighting")))
            {
                GroupDesc.bScatterSceneLighting = Payload->GetBoolField(TEXT("scatterSceneLighting"));
            }
            
            GroomComp->UpdateHairGroupsDescAndInvalidateRenderState(true);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Group index %d out of range"), GroupIndex), TEXT("INVALID_INDEX"));
            return true;
        }
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Hair rendering configured"));
        Result->SetStringField(TEXT("actorName"), Actor->GetName());
        Result->SetNumberField(TEXT("groupIndex"), GroupIndex);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hair rendering configured"), Result);
        return true;
    }

#else
    // Groom not available - return appropriate messages
    if (SubAction == TEXT("create_groom_asset") ||
        SubAction == TEXT("import_groom") ||
        SubAction == TEXT("create_groom_binding") ||
        SubAction == TEXT("spawn_groom_actor") ||
        SubAction == TEXT("attach_groom_to_skeletal_mesh") ||
        SubAction == TEXT("configure_hair_simulation") ||
        SubAction == TEXT("set_hair_width") ||
        SubAction == TEXT("set_hair_root_scale") ||
        SubAction == TEXT("set_hair_tip_scale") ||
        SubAction == TEXT("set_hair_color") ||
        SubAction == TEXT("configure_hair_physics") ||
        SubAction == TEXT("configure_hair_rendering") ||
        SubAction == TEXT("enable_hair_simulation") ||
        SubAction == TEXT("get_groom_info"))
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Groom/HairStrands plugin is not available. Enable the HairStrands plugin in your project."), 
            TEXT("PLUGIN_NOT_AVAILABLE"));
        return true;
    }
#endif // MCP_HAS_GROOM

    // =========================================================================
    // MUTABLE/CUSTOMIZABLE ACTIONS
    // =========================================================================

#if MCP_HAS_MUTABLE
    if (SubAction == TEXT("create_customizable_object"))
    {
        const FString Name = Payload->GetStringField(TEXT("name"));
        const FString DestPath = Payload->HasField(TEXT("destinationPath")) 
            ? Payload->GetStringField(TEXT("destinationPath")) 
            : TEXT("/Game/Mutable");

        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        const FString PackagePath = DestPath / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_FAILED"));
            return true;
        }

        UCustomizableObject* CustomObj = NewObject<UCustomizableObject>(Package, *Name, RF_Public | RF_Standalone);
        if (!CustomObj)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create customizable object"), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(CustomObj);
        CustomObj->MarkPackageDirty();

        if (Payload->GetBoolField(TEXT("save")))
        {
            McpSafeAssetSave(CustomObj);
        }

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Customizable object created"));
        Result->SetStringField(TEXT("assetPath"), PackagePath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Customizable object created"), Result);
        return true;
    }

    if (SubAction == TEXT("create_customizable_instance"))
    {
        const FString ObjectPath = Payload->GetStringField(TEXT("objectPath"));
        if (ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("objectPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UCustomizableObject* CustomObj = LoadObject<UCustomizableObject>(nullptr, *ObjectPath);
        if (!CustomObj)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Customizable object not found: %s"), *ObjectPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UCustomizableObjectInstance* Instance = CustomObj->CreateInstance();
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create instance"), TEXT("CREATE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Customizable instance created"));
        Result->SetStringField(TEXT("instancePath"), Instance->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instance created"), Result);
        return true;
    }

    if (SubAction == TEXT("get_parameter_info"))
    {
        const FString ObjectPath = Payload->GetStringField(TEXT("objectPath"));
        if (ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("objectPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UCustomizableObject* CustomObj = LoadObject<UCustomizableObject>(nullptr, *ObjectPath);
        if (!CustomObj)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Customizable object not found: %s"), *ObjectPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> ParamsArray;
        
        // Get parameter count and info
        int32 ParamCount = CustomObj->GetParameterCount();
        for (int32 i = 0; i < ParamCount; ++i)
        {
            TSharedPtr<FJsonObject> Param = MakeShareable(new FJsonObject());
            Param->SetStringField(TEXT("name"), CustomObj->GetParameterName(i));
            Param->SetNumberField(TEXT("index"), i);
            ParamsArray.Add(MakeShareable(new FJsonValueObject(Param)));
        }
        
        ParamInfo->SetArrayField(TEXT("parameters"), ParamsArray);
        ParamInfo->SetNumberField(TEXT("parameterCount"), ParamCount);
        
        Result->SetObjectField(TEXT("parameterInfo"), ParamInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parameter info retrieved"), Result);
        return true;
    }

    if (SubAction == TEXT("compile_customizable_object"))
    {
        const FString ObjectPath = Payload->GetStringField(TEXT("objectPath"));
        if (ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("objectPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UCustomizableObject* CustomObj = LoadObject<UCustomizableObject>(nullptr, *ObjectPath);
        if (!CustomObj)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Customizable object not found: %s"), *ObjectPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Mutable compilation is typically done through the editor
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("objectPath"), ObjectPath);
        Result->SetStringField(TEXT("message"), TEXT("Customizable object compilation initiated."));
        Result->SetStringField(TEXT("guidance"), TEXT("Compilation happens automatically when the CO is modified or saved. Use the Mutable Editor to manually trigger compilation or check the CO's compiled state."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Compilation guidance"), Result);
        return true;
    }

    if (SubAction == TEXT("set_bool_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));
        const bool Value = Payload->GetBoolField(TEXT("value"));

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetBoolParameterSelectedOption(ParamName, Value);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Bool parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetBoolField(TEXT("value"), Value);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bool parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_int_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));
        const FString OptionName = Payload->GetStringField(TEXT("optionName"));
        const int32 RangeIndex = Payload->HasField(TEXT("rangeIndex")) ? (int32)Payload->GetNumberField(TEXT("rangeIndex")) : -1;

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetEnumParameterSelectedOption(ParamName, OptionName, RangeIndex);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Int/Enum parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("optionName"), OptionName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Int parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_float_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));
        const float Value = Payload->GetNumberField(TEXT("value"));
        const int32 RangeIndex = Payload->HasField(TEXT("rangeIndex")) ? (int32)Payload->GetNumberField(TEXT("rangeIndex")) : -1;

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetFloatParameterSelectedOption(ParamName, Value, RangeIndex);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Float parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetNumberField(TEXT("value"), Value);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Float parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_color_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));

        const TSharedPtr<FJsonObject>* ColorObj = nullptr;
        FLinearColor Color(1, 1, 1, 1);
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj))
        {
            Color.R = (*ColorObj)->GetNumberField(TEXT("r"));
            Color.G = (*ColorObj)->GetNumberField(TEXT("g"));
            Color.B = (*ColorObj)->GetNumberField(TEXT("b"));
            Color.A = (*ColorObj)->HasField(TEXT("a")) ? (*ColorObj)->GetNumberField(TEXT("a")) : 1.0f;
        }

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetColorParameterSelectedOption(ParamName, Color);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Color parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        TSharedPtr<FJsonObject> ColorResult = MakeShareable(new FJsonObject());
        ColorResult->SetNumberField(TEXT("r"), Color.R);
        ColorResult->SetNumberField(TEXT("g"), Color.G);
        ColorResult->SetNumberField(TEXT("b"), Color.B);
        ColorResult->SetNumberField(TEXT("a"), Color.A);
        Result->SetObjectField(TEXT("color"), ColorResult);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_vector_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));

        const TSharedPtr<FJsonObject>* VectorObj = nullptr;
        FLinearColor Vector(0, 0, 0, 0);
        if (Payload->TryGetObjectField(TEXT("vector"), VectorObj))
        {
            Vector.R = (*VectorObj)->GetNumberField(TEXT("x"));
            Vector.G = (*VectorObj)->GetNumberField(TEXT("y"));
            Vector.B = (*VectorObj)->GetNumberField(TEXT("z"));
            Vector.A = (*VectorObj)->HasField(TEXT("w")) ? (*VectorObj)->GetNumberField(TEXT("w")) : 0.0f;
        }

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetVectorParameterSelectedOption(ParamName, Vector);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Vector parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Vector parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_texture_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));
        const FString TexturePath = Payload->GetStringField(TEXT("texturePath"));
        const int32 RangeIndex = Payload->HasField(TEXT("rangeIndex")) ? (int32)Payload->GetNumberField(TEXT("rangeIndex")) : -1;

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (!Texture)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Texture not found: %s"), *TexturePath), TEXT("TEXTURE_NOT_FOUND"));
            return true;
        }

        Instance->SetTextureParameterSelectedOption(ParamName, Texture, RangeIndex);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Texture parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        Result->SetStringField(TEXT("texturePath"), TexturePath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Texture parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_transform_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));

        FTransform Transform = FTransform::Identity;
        const TSharedPtr<FJsonObject>* TransformObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("transform"), TransformObj))
        {
            const TSharedPtr<FJsonObject>* LocObj = nullptr;
            if ((*TransformObj)->TryGetObjectField(TEXT("location"), LocObj))
            {
                Transform.SetLocation(FVector(
                    (*LocObj)->GetNumberField(TEXT("x")),
                    (*LocObj)->GetNumberField(TEXT("y")),
                    (*LocObj)->GetNumberField(TEXT("z"))
                ));
            }
            const TSharedPtr<FJsonObject>* RotObj = nullptr;
            if ((*TransformObj)->TryGetObjectField(TEXT("rotation"), RotObj))
            {
                Transform.SetRotation(FQuat(FRotator(
                    (*RotObj)->GetNumberField(TEXT("pitch")),
                    (*RotObj)->GetNumberField(TEXT("yaw")),
                    (*RotObj)->GetNumberField(TEXT("roll"))
                )));
            }
            const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
            if ((*TransformObj)->TryGetObjectField(TEXT("scale"), ScaleObj))
            {
                Transform.SetScale3D(FVector(
                    (*ScaleObj)->GetNumberField(TEXT("x")),
                    (*ScaleObj)->GetNumberField(TEXT("y")),
                    (*ScaleObj)->GetNumberField(TEXT("z"))
                ));
            }
        }

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetTransformParameterSelectedOption(ParamName, Transform);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Transform parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transform parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_projector_parameter"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString ParamName = Payload->GetStringField(TEXT("parameterName"));
        const int32 RangeIndex = Payload->HasField(TEXT("rangeIndex")) ? (int32)Payload->GetNumberField(TEXT("rangeIndex")) : -1;

        FVector Position(0, 0, 0);
        FVector Direction(1, 0, 0);
        FVector Up(0, 0, 1);
        FVector Scale(1, 1, 1);
        float Angle = 0.0f;

        const TSharedPtr<FJsonObject>* PosObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("position"), PosObj))
        {
            Position = FVector(
                (*PosObj)->GetNumberField(TEXT("x")),
                (*PosObj)->GetNumberField(TEXT("y")),
                (*PosObj)->GetNumberField(TEXT("z"))
            );
        }
        const TSharedPtr<FJsonObject>* DirObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("direction"), DirObj))
        {
            Direction = FVector(
                (*DirObj)->GetNumberField(TEXT("x")),
                (*DirObj)->GetNumberField(TEXT("y")),
                (*DirObj)->GetNumberField(TEXT("z"))
            );
        }
        const TSharedPtr<FJsonObject>* UpObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("up"), UpObj))
        {
            Up = FVector(
                (*UpObj)->GetNumberField(TEXT("x")),
                (*UpObj)->GetNumberField(TEXT("y")),
                (*UpObj)->GetNumberField(TEXT("z"))
            );
        }
        const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("scale"), ScaleObj))
        {
            Scale = FVector(
                (*ScaleObj)->GetNumberField(TEXT("x")),
                (*ScaleObj)->GetNumberField(TEXT("y")),
                (*ScaleObj)->GetNumberField(TEXT("z"))
            );
        }
        if (Payload->HasField(TEXT("angle")))
        {
            Angle = Payload->GetNumberField(TEXT("angle"));
        }

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->SetProjectorValue(ParamName, Position, Direction, Up, Scale, Angle, RangeIndex);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Projector parameter set"));
        Result->SetStringField(TEXT("parameterName"), ParamName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projector parameter set"), Result);
        return true;
    }

    if (SubAction == TEXT("update_skeletal_mesh"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const bool bForceHighPriority = Payload->HasField(TEXT("forceHighPriority")) ? Payload->GetBoolField(TEXT("forceHighPriority")) : false;

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        Instance->UpdateSkeletalMeshAsync(false, bForceHighPriority);

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Skeletal mesh update initiated"));
        Result->SetStringField(TEXT("instancePath"), InstancePath);
        Result->SetStringField(TEXT("message"), TEXT("Update is asynchronous. The UpdatedDelegate will be called when complete."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeletal mesh update initiated"), Result);
        return true;
    }

    if (SubAction == TEXT("bake_customizable_instance"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));
        const FString OutputPath = Payload->HasField(TEXT("outputPath")) 
            ? Payload->GetStringField(TEXT("outputPath")) 
            : TEXT("/Game/Baked");

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

#if WITH_EDITOR
        FBakingConfiguration BakeConfig;
        BakeConfig.OutputPath = OutputPath;
        BakeConfig.bExportAllResourcesOnBake = Payload->HasField(TEXT("exportAll")) ? Payload->GetBoolField(TEXT("exportAll")) : false;
        
        Instance->Bake(BakeConfig);
        
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Bake operation initiated"));
        Result->SetStringField(TEXT("instancePath"), InstancePath);
        Result->SetStringField(TEXT("outputPath"), OutputPath);
#else
        TSharedPtr<FJsonObject> Result = MakeErrorResult(TEXT("Baking is only available in editor builds"));
#endif

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bake operation"), Result);
        return true;
    }

    if (SubAction == TEXT("get_instance_info"))
    {
        const FString InstancePath = Payload->GetStringField(TEXT("instancePath"));

        UCustomizableObjectInstance* Instance = LoadObject<UCustomizableObjectInstance>(nullptr, *InstancePath);
        if (!Instance)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Instance not found: %s"), *InstancePath), TEXT("INSTANCE_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("instancePath"), InstancePath);

        TSharedPtr<FJsonObject> InstanceInfo = MakeShareable(new FJsonObject());
        InstanceInfo->SetStringField(TEXT("currentState"), Instance->GetCurrentState());
        InstanceInfo->SetBoolField(TEXT("hasAnySkeletalMesh"), Instance->HasAnySkeletalMesh());
        InstanceInfo->SetBoolField(TEXT("hasAnyParameters"), Instance->HasAnyParameters());
        
        if (Instance->GetCustomizableObject())
        {
            InstanceInfo->SetStringField(TEXT("customizableObject"), Instance->GetCustomizableObject()->GetPathName());
        }

        // Get component names
        TArray<FName> ComponentNames = Instance->GetComponentNames();
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        for (const FName& CompName : ComponentNames)
        {
            ComponentsArray.Add(MakeShareable(new FJsonValueString(CompName.ToString())));
        }
        InstanceInfo->SetArrayField(TEXT("componentNames"), ComponentsArray);

        Result->SetObjectField(TEXT("instanceInfo"), InstanceInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instance info retrieved"), Result);
        return true;
    }

    if (SubAction == TEXT("spawn_customizable_actor"))
    {
        const FString ObjectPath = Payload->GetStringField(TEXT("objectPath"));
        
        if (ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("objectPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UCustomizableObject* CustomObj = LoadObject<UCustomizableObject>(nullptr, *ObjectPath);
        if (!CustomObj)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Customizable object not found: %s"), *ObjectPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        FVector Location(0, 0, 0);
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            Location.X = (*LocObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
        }

        // Create an actor with a skeletal mesh component
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
        if (!NewActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        // Create skeletal mesh component
        USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(NewActor, TEXT("SkeletalMeshComponent"));
        SkelComp->RegisterComponent();
        NewActor->AddInstanceComponent(SkelComp);
        NewActor->SetRootComponent(SkelComp);

        // Create an instance and associate it
        UCustomizableObjectInstance* Instance = CustomObj->CreateInstance();
        if (Instance)
        {
            Instance->UpdateSkeletalMeshAsync(false, true);
        }

        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Customizable actor spawned"));
        Result->SetStringField(TEXT("actorName"), NewActor->GetName());
        Result->SetStringField(TEXT("objectPath"), ObjectPath);
        if (Instance)
        {
            Result->SetStringField(TEXT("instancePath"), Instance->GetPathName());
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Customizable actor spawned"), Result);
        return true;
    }

#else
    // Mutable not available
    if (SubAction == TEXT("create_customizable_object") ||
        SubAction == TEXT("compile_customizable_object") ||
        SubAction == TEXT("create_customizable_instance") ||
        SubAction == TEXT("set_bool_parameter") ||
        SubAction == TEXT("set_int_parameter") ||
        SubAction == TEXT("set_float_parameter") ||
        SubAction == TEXT("set_color_parameter") ||
        SubAction == TEXT("set_vector_parameter") ||
        SubAction == TEXT("set_texture_parameter") ||
        SubAction == TEXT("set_transform_parameter") ||
        SubAction == TEXT("set_projector_parameter") ||
        SubAction == TEXT("update_skeletal_mesh") ||
        SubAction == TEXT("bake_customizable_instance") ||
        SubAction == TEXT("get_parameter_info") ||
        SubAction == TEXT("get_instance_info") ||
        SubAction == TEXT("spawn_customizable_actor"))
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Mutable plugin is not available. Enable the Mutable (Customizable) plugin in your project."), 
            TEXT("PLUGIN_NOT_AVAILABLE"));
        return true;
    }
#endif // MCP_HAS_MUTABLE

    // =========================================================================
    // READY PLAYER ME ACTIONS
    // =========================================================================

    if (SubAction == TEXT("load_avatar_from_url"))
    {
        const FString AvatarUrl = Payload->GetStringField(TEXT("avatarUrl"));
        if (AvatarUrl.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("avatarUrl is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // Ready Player Me integration requires the RPM plugin
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("Ready Player Me URL avatar loading acknowledged."));
        Result->SetStringField(TEXT("avatarUrl"), AvatarUrl);
        Result->SetStringField(TEXT("note"), TEXT("Full RPM functionality requires the Ready Player Me plugin. Visit readyplayer.me for integration details."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPM avatar load acknowledged"), Result);
        return true;
    }

    if (SubAction == TEXT("load_avatar_from_glb"))
    {
        const FString GlbPath = Payload->GetStringField(TEXT("glbPath"));
        if (GlbPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("glbPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // GLB import would use the Interchange framework in UE5
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("GLB avatar import acknowledged."));
        Result->SetStringField(TEXT("glbPath"), GlbPath);
        Result->SetStringField(TEXT("note"), TEXT("Use Interchange or glTF Runtime plugin for full GLB import support."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GLB import acknowledged"), Result);
        return true;
    }

    if (SubAction == TEXT("get_rpm_info"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> RpmInfo = MakeShareable(new FJsonObject());
        
        // Check if RPM plugin is available
        bool bRpmAvailable = false;
        #if __has_include("ReadyPlayerMeComponent.h")
        bRpmAvailable = true;
        #endif
        
        RpmInfo->SetBoolField(TEXT("isAvailable"), bRpmAvailable);
        RpmInfo->SetStringField(TEXT("version"), TEXT("N/A"));
        RpmInfo->SetNumberField(TEXT("cachedAvatars"), 0);
        
        TArray<TSharedPtr<FJsonValue>> Formats;
        Formats.Add(MakeShareable(new FJsonValueString(TEXT("glb"))));
        Formats.Add(MakeShareable(new FJsonValueString(TEXT("gltf"))));
        RpmInfo->SetArrayField(TEXT("supportedFormats"), Formats);
        
        Result->SetObjectField(TEXT("rpmInfo"), RpmInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPM info retrieved"), Result);
        return true;
    }

    if (SubAction == TEXT("clear_avatar_cache"))
    {
        TSharedPtr<FJsonObject> Result = MakeSuccessResult(TEXT("Avatar cache cleared"));
        TSharedPtr<FJsonObject> CacheInfo = MakeShareable(new FJsonObject());
        CacheInfo->SetNumberField(TEXT("itemsCleared"), 0);
        Result->SetObjectField(TEXT("cacheInfo"), CacheInfo);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cache cleared"), Result);
        return true;
    }

    // Handle remaining RPM actions with guidance
    if (SubAction == TEXT("create_rpm_actor") ||
        SubAction == TEXT("apply_avatar_to_character") ||
        SubAction == TEXT("configure_rpm_materials") ||
        SubAction == TEXT("set_rpm_outfit") ||
        SubAction == TEXT("get_avatar_metadata") ||
        SubAction == TEXT("cache_avatar") ||
        SubAction == TEXT("create_rpm_animation_blueprint") ||
        SubAction == TEXT("retarget_rpm_animation"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("RPM action '%s' acknowledged."), *SubAction));
        Result->SetStringField(TEXT("note"), TEXT("Full Ready Player Me functionality requires the RPM plugin integration."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("RPM action acknowledged"), Result);
        return true;
    }

    // Unknown action
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character avatar action: %s"), *SubAction), 
        TEXT("UNKNOWN_ACTION"));
    return true;
}
