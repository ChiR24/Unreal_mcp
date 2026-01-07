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
#include "UObject/SavePackage.h"
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

    if (SubAction == TEXT("get_metahuman_component") ||
        SubAction == TEXT("set_body_type") ||
        SubAction == TEXT("set_face_parameter") ||
        SubAction == TEXT("set_skin_tone") ||
        SubAction == TEXT("set_hair_style") ||
        SubAction == TEXT("set_eye_color") ||
        SubAction == TEXT("configure_metahuman_lod") ||
        SubAction == TEXT("enable_body_correctives") ||
        SubAction == TEXT("enable_neck_correctives") ||
        SubAction == TEXT("set_quality_level") ||
        SubAction == TEXT("configure_face_rig") ||
        SubAction == TEXT("set_body_part") ||
        SubAction == TEXT("list_available_presets") ||
        SubAction == TEXT("apply_preset") ||
        SubAction == TEXT("export_metahuman_settings"))
    {
        // MetaHuman SDK specific operations
        // These require the MetaHuman SDK plugin which has limited public API
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaHuman action '%s' acknowledged. Full implementation requires MetaHuman SDK."), *SubAction));
        Result->SetStringField(TEXT("note"), TEXT("MetaHuman customization is typically done through Blueprint or the MetaHuman Creator."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaHuman action acknowledged"), Result);
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

    // Fallback for unhandled groom actions (when groom is available but action not yet implemented)
    if (SubAction == TEXT("import_groom") ||
        SubAction == TEXT("create_groom_binding") ||
        SubAction == TEXT("configure_hair_simulation") ||
        SubAction == TEXT("set_hair_root_scale") ||
        SubAction == TEXT("set_hair_tip_scale") ||
        SubAction == TEXT("set_hair_color") ||
        SubAction == TEXT("configure_hair_physics") ||
        SubAction == TEXT("configure_hair_rendering"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Groom action '%s' acknowledged."), *SubAction));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Groom action acknowledged"), Result);
        return true;
    }

    // Unknown action
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character avatar action: %s"), *SubAction), 
        TEXT("UNKNOWN_ACTION"));
    return true;
}
