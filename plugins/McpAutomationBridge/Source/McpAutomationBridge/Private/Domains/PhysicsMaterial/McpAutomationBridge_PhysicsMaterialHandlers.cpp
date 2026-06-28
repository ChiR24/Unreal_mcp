// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
//
// McpAutomationBridge_PhysicsMaterialHandlers.cpp
// Physics Material Management
//
// Implements creation, configuration, querying, and assignment of UPhysicalMaterial assets:
// - create_physics_material: Create a new physical material asset
// - set_physics_material_properties: Modify properties on an existing physical material
// - get_physics_material_properties: Read all properties from a physical material
// - list_physics_materials: List physical material assets via asset registry
// - assign_physics_material: Apply a physical material to an actor's primitive components

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PrimitiveComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpPhysicsMaterialHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================
// Uses consolidated JSON helpers from McpAutomationBridgeHelpers.h:
//   - GetJsonStringField(Obj, Field, Default)
//   - GetJsonNumberField(Obj, Field, Default)
//   - GetJsonBoolField(Obj, Field, Default)
//   - GetJsonIntField(Obj, Field, Default)

#if WITH_EDITOR

// Find an actor by editor label or internal name
static AActor* PhysMatFindActorByLabelOrName(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor)
        {
            if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
            if (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }
    return nullptr;
}

// Map string to EFrictionCombineMode
static FString FrictionCombineModeToString(EFrictionCombineMode::Type Mode)
{
    switch (Mode)
    {
    case EFrictionCombineMode::Average: return TEXT("Average");
    case EFrictionCombineMode::Min: return TEXT("Min");
    case EFrictionCombineMode::Multiply: return TEXT("Multiply");
    case EFrictionCombineMode::Max: return TEXT("Max");
    default: return TEXT("Average");
    }
}

// ============================================================================
// Sub-Action Handlers
// ============================================================================

// ----------------------------------------------------------------------------
// create_physics_material
// Creates a new UPhysicalMaterial asset with specified properties.
// ----------------------------------------------------------------------------
static bool HandleCreatePhysicsMaterial(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT("/Game/PhysicsMaterials"));
    double Friction = GetJsonNumberField(Payload, TEXT("friction"), 0.7);
    double Restitution = GetJsonNumberField(Payload, TEXT("restitution"), 0.3);
    double Density = GetJsonNumberField(Payload, TEXT("density"), 1.0);

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString SanitizedName = SanitizeAssetName(AssetName);
    FString PathError;
    UPackage* Package = CreateValidatedAssetPackage(FolderPath, SanitizedName, PathError);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
            nullptr, TEXT("PACKAGE_CREATE_FAILED"));
        return true;
    }

    UPhysicalMaterial* NewMaterial = NewObject<UPhysicalMaterial>(
        Package, FName(*SanitizedName), RF_Public | RF_Standalone);

    if (!NewMaterial)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UPhysicalMaterial object"), nullptr);
        return true;
    }

    NewMaterial->Friction = static_cast<float>(Friction);
    NewMaterial->Restitution = static_cast<float>(Restitution);
    NewMaterial->Density = static_cast<float>(Density);

    McpSafeAssetSave(NewMaterial);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
    Result->SetStringField(TEXT("assetName"), SanitizedName);
    Result->SetNumberField(TEXT("friction"), NewMaterial->Friction);
    Result->SetNumberField(TEXT("restitution"), NewMaterial->Restitution);
    Result->SetNumberField(TEXT("density"), NewMaterial->Density);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created physics material: %s"), *SanitizedName), Result);
    return true;
}

// ----------------------------------------------------------------------------
// set_physics_material_properties
// Modifies properties on an existing UPhysicalMaterial asset.
// ----------------------------------------------------------------------------
static bool HandleSetPhysicsMaterialProperties(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid asset path"), nullptr, TEXT("INVALID_PATH"));
        return true;
    }

    UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *SafePath);
    if (!PhysMat)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Physics material not found at: %s"), *SafePath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Apply optional properties (only set if present in payload)
    if (Payload->HasField(TEXT("friction")))
    {
        PhysMat->Friction = static_cast<float>(GetJsonNumberField(Payload, TEXT("friction"), PhysMat->Friction));
    }
    if (Payload->HasField(TEXT("staticFriction")))
    {
        PhysMat->StaticFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("staticFriction"), PhysMat->StaticFriction));
    }
    if (Payload->HasField(TEXT("restitution")))
    {
        PhysMat->Restitution = static_cast<float>(GetJsonNumberField(Payload, TEXT("restitution"), PhysMat->Restitution));
    }
    if (Payload->HasField(TEXT("density")))
    {
        PhysMat->Density = static_cast<float>(GetJsonNumberField(Payload, TEXT("density"), PhysMat->Density));
    }
    if (Payload->HasField(TEXT("surfaceType")))
    {
        int32 SurfaceTypeInt = GetJsonIntField(Payload, TEXT("surfaceType"), static_cast<int32>(PhysMat->SurfaceType));
        if (SurfaceTypeInt >= 0 && SurfaceTypeInt <= static_cast<int32>(EPhysicalSurface::SurfaceType_Max))
        {
            PhysMat->SurfaceType = static_cast<EPhysicalSurface>(SurfaceTypeInt);
        }
    }

    McpSafeAssetSave(PhysMat);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), PhysMat->GetPathName());
    Result->SetNumberField(TEXT("friction"), PhysMat->Friction);
    Result->SetNumberField(TEXT("staticFriction"), PhysMat->StaticFriction);
    Result->SetNumberField(TEXT("restitution"), PhysMat->Restitution);
    Result->SetNumberField(TEXT("density"), PhysMat->Density);
    Result->SetNumberField(TEXT("surfaceType"), static_cast<int32>(PhysMat->SurfaceType));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Physics material properties updated"), Result);
    return true;
}

// ----------------------------------------------------------------------------
// get_physics_material_properties
// Reads and returns all properties from a UPhysicalMaterial asset.
// ----------------------------------------------------------------------------
static bool HandleGetPhysicsMaterialProperties(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid asset path"), nullptr, TEXT("INVALID_PATH"));
        return true;
    }

    UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *SafePath);
    if (!PhysMat)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Physics material not found at: %s"), *SafePath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), PhysMat->GetPathName());
    Result->SetStringField(TEXT("assetName"), PhysMat->GetName());
    Result->SetNumberField(TEXT("friction"), PhysMat->Friction);
    Result->SetStringField(TEXT("frictionCombineMode"), FrictionCombineModeToString(PhysMat->FrictionCombineMode));
    Result->SetNumberField(TEXT("staticFriction"), PhysMat->StaticFriction);
    Result->SetNumberField(TEXT("restitution"), PhysMat->Restitution);
    Result->SetStringField(TEXT("restitutionCombineMode"), FrictionCombineModeToString(
        static_cast<EFrictionCombineMode::Type>(PhysMat->RestitutionCombineMode)));
    Result->SetNumberField(TEXT("density"), PhysMat->Density);
    Result->SetNumberField(TEXT("surfaceType"), static_cast<int32>(PhysMat->SurfaceType));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Physics material properties retrieved"), Result);
    return true;
}

// ----------------------------------------------------------------------------
// list_physics_materials
// Lists UPhysicalMaterial assets via the asset registry.
// ----------------------------------------------------------------------------
static bool HandleListPhysicsMaterials(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString SearchPath = GetJsonStringField(Payload, TEXT("searchPath"), TEXT(""));

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    FARFilter Filter;
    Filter.ClassPaths.Add(UPhysicalMaterial::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.bRecursiveClasses = true;

    if (!SearchPath.IsEmpty())
    {
        FString SafePath = SanitizeProjectRelativePath(SearchPath);
        if (!SafePath.IsEmpty())
        {
            Filter.PackagePaths.Add(FName(*SafePath));
        }
    }

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    TArray<TSharedPtr<FJsonValue>> MaterialArray;
    for (const FAssetData& AssetData : AssetList)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        Entry->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
        Entry->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
        MaterialArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"), MaterialArray.Num());
    Result->SetArrayField(TEXT("materials"), MaterialArray);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d physics material(s)"), MaterialArray.Num()), Result);
    return true;
}

// ----------------------------------------------------------------------------
// assign_physics_material
// Applies a UPhysicalMaterial to an actor's primitive component(s).
// ----------------------------------------------------------------------------
static bool HandleAssignPhysicsMaterial(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid asset path"), nullptr, TEXT("INVALID_PATH"));
        return true;
    }

    UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *SafePath);
    if (!PhysMat)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Physics material not found at: %s"), *SafePath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Find actor in editor world
    UWorld* World = nullptr;
    if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = PhysMatFindActorByLabelOrName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    int32 AssignedCount = 0;
    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
    {
        if (!PrimComp)
        {
            continue;
        }

        // If a specific component name is given, only apply to that component
        if (!ComponentName.IsEmpty())
        {
            if (!PrimComp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        PrimComp->SetPhysMaterialOverride(PhysMat);
        AssignedCount++;
    }

    if (AssignedCount == 0)
    {
        FString Msg = ComponentName.IsEmpty()
            ? FString::Printf(TEXT("No primitive components found on actor: %s"), *ActorName)
            : FString::Printf(TEXT("Component '%s' not found on actor: %s"), *ComponentName, *ActorName);
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            Msg, nullptr, TEXT("NO_COMPONENTS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Result->SetStringField(TEXT("materialPath"), PhysMat->GetPathName());
    Result->SetNumberField(TEXT("componentsUpdated"), AssignedCount);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Assigned physics material to %d component(s) on %s"),
            AssignedCount, *Actor->GetActorLabel()),
        Result);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManagePhysicsMaterialAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpPhysicsMaterialHandlers, Verbose,
        TEXT("HandleManagePhysicsMaterialAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_physics_material"))
    {
        return HandleCreatePhysicsMaterial(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_physics_material_properties"))
    {
        return HandleSetPhysicsMaterialProperties(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_physics_material_properties"))
    {
        return HandleGetPhysicsMaterialProperties(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("list_physics_materials"))
    {
        return HandleListPhysicsMaterials(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("assign_physics_material"))
    {
        return HandleAssignPhysicsMaterial(this, RequestId, Payload, Socket);
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown physics_material subAction: %s"), *SubAction),
        nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Physics material operations require editor build"),
        nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
