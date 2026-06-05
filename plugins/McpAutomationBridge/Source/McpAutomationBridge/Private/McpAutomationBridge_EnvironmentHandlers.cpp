// =============================================================================
// McpAutomationBridge_EnvironmentHandlers.cpp
// =============================================================================
// Environment, Console, and Inspection System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: Build Environment Actions
//   - HandleBuildEnvironmentAction     : Main dispatcher for environment sub-actions
//     Sub-actions: export_snapshot, import_snapshot, delete, create_sky_sphere,
//                  set_time_of_day, create_fog_volume, foliage dispatch
//
// Section 2: Control Environment Actions
//   - HandleControlEnvironmentAction   : Environment control (time, lighting)
//     Sub-actions: set_time_of_day, set_sun_intensity, set_skylight_intensity
//
// Section 3: Console Command Handler
//   - HandleConsoleCommandAction       : Execute console commands with security filtering
//     Supports: Direct "console_command" and "system_control" with subAction
//     Security: Blocks dangerous commands (quit, exit, crash, etc.)
//
// Section 4: Environment Utilities
//   - HandleBakeLightmap               : Lightmap baking via BUILD_LIGHTING
//   - HandleCreateProceduralTerrain    : Procedural terrain mesh generation
//   - HandleInspectAction              : Object introspection and inspection
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// build_environment:
//   Payload: { "action": "<sub-action>", ... }
//   Response: { "success": bool, "action": string, ... }
//
// control_environment:
//   Payload: { "action": "<sub-action>", "hour"?: number, "intensity"?: number }
//   Response: { "success": bool, "hour"?: number, "intensity"?: number }
//
// console_command:
//   Payload: { "command": string }
//   Response: { "success": bool, "command": string, "executed": bool }
//
// inspect:
//   Payload: { "action": "<sub-action>", "objectPath"?: string }
//   Response: { "success": bool, "objectPath"?: string, "className"?: string }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - ProceduralMeshComponent available in all versions
// - UEditorActorSubsystem available via conditional includes
// - Console command execution uses GEngine->Exec()
//
// REFACTORING NOTES:
// ------------------
// - Extracted utility functions should use McpHandlerUtils namespace
// - File path validation uses SanitizeProjectFilePath()
// - Actor operations use FindActorByName() helper
// - Component lookups use FindComponentByName() helper
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"
#include "McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpLandscapeMetadataTags.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"

#include <initializer_list>

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Selection.h"

// Subsystem includes with version-specific paths
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif

// =============================================================================
// Engine Component Includes
// =============================================================================
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/WindDirectionalSource.h"
#include "Materials/MaterialInterface.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// =============================================================================
// Editor & Asset Includes
// =============================================================================
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "EditorValidatorSubsystem.h"
#include "DynamicRHI.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/EngineVersion.h"
#include "Settings/LevelEditorViewportSettings.h"

// =============================================================================
// Procedural & Mesh Includes
// =============================================================================
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"

// =============================================================================
// Landscape Includes (for foliage dispatch)
// =============================================================================
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeGrassType.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpEnvironmentHandlers, Log, All);

#if WITH_EDITOR
static TSharedPtr<FJsonObject> McpMakeVectorObject(const FVector &Vector)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("x"), Vector.X);
    Obj->SetNumberField(TEXT("y"), Vector.Y);
    Obj->SetNumberField(TEXT("z"), Vector.Z);
    return Obj;
}

static TSharedPtr<FJsonObject> McpMakeRotatorObject(const FRotator &Rotator)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Obj;
}

static TSharedPtr<FJsonObject> McpMakeTransformObject(const FTransform &Transform)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetObjectField(TEXT("location"), McpMakeVectorObject(Transform.GetLocation()));
    Obj->SetObjectField(TEXT("rotation"), McpMakeRotatorObject(Transform.GetRotation().Rotator()));
    Obj->SetObjectField(TEXT("scale"), McpMakeVectorObject(Transform.GetScale3D()));
    return Obj;
}

static FString McpGetFirstStringField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields)
{
    if (!Payload.IsValid())
    {
        return FString();
    }

    for (const TCHAR *Field : Fields)
    {
        FString Value;
        if (Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString();
}

static FVector McpGetVectorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FVector &DefaultValue)
{
    if (!Payload.IsValid())
    {
        return DefaultValue;
    }

    FVector Result = DefaultValue;
    const TSharedPtr<FJsonObject> *Obj = nullptr;
    if (Payload->TryGetObjectField(FieldName, Obj) && Obj && Obj->IsValid())
    {
        McpPropertyReflection::JsonToVector(*Obj, Result);
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
    if (Payload->TryGetArrayField(FieldName, Arr) && Arr)
    {
        McpPropertyReflection::JsonArrayToVector(*Arr, Result);
    }
    return Result;
}

static FRotator McpGetRotatorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FRotator &DefaultValue)
{
    if (!Payload.IsValid())
    {
        return DefaultValue;
    }

    FRotator Result = DefaultValue;
    const TSharedPtr<FJsonObject> *Obj = nullptr;
    if (Payload->TryGetObjectField(FieldName, Obj) && Obj && Obj->IsValid())
    {
        McpPropertyReflection::JsonToRotator(*Obj, Result);
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
    if (Payload->TryGetArrayField(FieldName, Arr) && Arr)
    {
        McpPropertyReflection::JsonArrayToRotator(*Arr, Result);
    }
    return Result;
}

static FProperty *McpFindPropertyCaseInsensitive(UObject *Object, const FString &PropertyName)
{
    if (!Object || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    if (FProperty *Exact = Object->GetClass()->FindPropertyByName(FName(*PropertyName)))
    {
        return Exact;
    }

    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty *Property = *It;
        if (!Property)
        {
            continue;
        }

        const FString Candidate = Property->GetName();
        if (Candidate.Equals(PropertyName, ESearchCase::IgnoreCase) ||
            (Candidate.StartsWith(TEXT("b")) && Candidate.Mid(1).Equals(PropertyName, ESearchCase::IgnoreCase)))
        {
            return Property;
        }
    }
    return nullptr;
}

static UObject *McpGetObjectPropertyValue(UObject *Object, const FString &PropertyName)
{
    FProperty *Property = McpFindPropertyCaseInsensitive(Object, PropertyName);
    if (FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property))
    {
        return ObjectProperty->GetObjectPropertyValue_InContainer(Object);
    }
    return nullptr;
}

static bool McpSetObjectPropertyValue(UObject *Object, const FString &PropertyName, UObject *Value)
{
    FProperty *Property = McpFindPropertyCaseInsensitive(Object, PropertyName);
    if (FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property))
    {
        Object->Modify();
        ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
        Object->MarkPackageDirty();
        return true;
    }
    return false;
}

static UObject *McpInvokeObjectGetter(UObject *Object, const FName &FunctionName)
{
    if (!Object)
    {
        return nullptr;
    }

    UFunction *Function = Object->FindFunction(FunctionName);
    if (!Function)
    {
        return nullptr;
    }

    struct FObjectGetterParams
    {
        UObject *ReturnValue = nullptr;
    };

    FObjectGetterParams Params;
    Object->ProcessEvent(Function, &Params);
    return Params.ReturnValue;
}

static bool McpInvokeObjectSetter(UObject *Object, const FName &FunctionName, UObject *Value)
{
    if (!Object)
    {
        return false;
    }

    UFunction *Function = Object->FindFunction(FunctionName);
    if (!Function)
    {
        return false;
    }

    struct FObjectSetterParams
    {
        UObject *Value = nullptr;
    };

    FObjectSetterParams Params;
    Params.Value = Value;
    Object->ProcessEvent(Function, &Params);
    return true;
}

static bool McpGetFirstNumberField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields, double &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    for (const TCHAR *Field : Fields)
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Field, Value))
        {
            OutValue = Value;
            return true;
        }
    }
    return false;
}

static bool McpApplyNumberProperty(UObject *Target, std::initializer_list<const TCHAR *> PropertyNames, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied)
{
    if (!Target)
    {
        return false;
    }

    for (const TCHAR *PropertyName : PropertyNames)
    {
        if (FProperty *Property = McpFindPropertyCaseInsensitive(Target, PropertyName))
        {
            FString ApplyError;
            if (McpPropertyReflection::ApplyJsonValueToProperty(Target, Property, MakeShared<FJsonValueNumber>(Value), ApplyError))
            {
                Applied.Add(Property->GetName());
                Resp->SetNumberField(ResponseName, Value);
                return true;
            }
        }
    }
    return false;
}

static int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties)
{
    if (!Target || !Payload.IsValid())
    {
        return 0;
    }

    auto ApplyObject = [&](const TSharedPtr<FJsonObject> &ObjectToApply) -> int32
    {
        int32 AppliedCount = 0;
        if (!ObjectToApply.IsValid())
        {
            return AppliedCount;
        }

        static const TSet<FString> IgnoredFields = {
            TEXT("action"), TEXT("name"), TEXT("actorName"), TEXT("targetActor"), TEXT("waterBodyName"),
            TEXT("path"), TEXT("outputPath"), TEXT("heightmapPath"), TEXT("landscapePath"), TEXT("foliageType"),
            TEXT("foliageTypePath"), TEXT("meshPath"), TEXT("staticMesh"), TEXT("materialPath"), TEXT("particleSystemPath"),
            TEXT("curvePath"), TEXT("settings"), TEXT("location"), TEXT("rotation"), TEXT("direction"), TEXT("points")
        };

        for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : ObjectToApply->Values)
        {
            if (IgnoredFields.Contains(Pair.Key) || !Pair.Value.IsValid())
            {
                continue;
            }

            FProperty *Property = McpFindPropertyCaseInsensitive(Target, Pair.Key);
            if (!Property)
            {
                continue;
            }

            FString ApplyError;
            if (McpPropertyReflection::ApplyJsonValueToProperty(Target, Property, Pair.Value, ApplyError))
            {
                AppliedProperties.Add(Property->GetName());
                ++AppliedCount;
            }
            else
            {
                FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *ApplyError));
            }
        }
        return AppliedCount;
    };

    int32 TotalApplied = ApplyObject(Payload);

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid())
    {
        TotalApplied += ApplyObject(*SettingsObj);
    }

    if (TotalApplied > 0)
    {
        Target->Modify();
        Target->MarkPackageDirty();
        Target->PostEditChange();
    }

    return TotalApplied;
}

static void McpAddStringArrayField(TSharedPtr<FJsonObject> Obj, const TCHAR *FieldName, const TArray<FString> &Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString &Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    Obj->SetArrayField(FieldName, JsonValues);
}

static UWorld *McpGetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName)
{
    UWorld *World = McpGetEditorWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor *FirstClassMatch = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (!Actor || (ActorClass && !Actor->IsA(ActorClass)))
        {
            continue;
        }

        if (!FirstClassMatch)
        {
            FirstClassMatch = Actor;
        }

        if (!ActorName.IsEmpty() &&
            (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
             Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }

    return ActorName.IsEmpty() ? FirstClassMatch : nullptr;
}

static AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                                   const FRotator &Rotation)
{
    if (!ActorClass)
    {
        return nullptr;
    }

    if (AActor *Existing = McpFindActorByNameOrClass(ActorClass, ActorName))
    {
        return Existing;
    }

    const FString Label = ActorName.IsEmpty() ? ActorClass->GetName() : ActorName;
    return SpawnActorInActiveWorld<AActor>(ActorClass, Location, Rotation, Label);
}

static UActorComponent *McpFindComponentByClass(AActor *Actor, UClass *ComponentClass)
{
    if (!Actor || !ComponentClass)
    {
        return nullptr;
    }

    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    for (UActorComponent *Component : Components)
    {
        if (Component && Component->IsA(ComponentClass))
        {
            return Component;
        }
    }
    return nullptr;
}

static UActorComponent *McpFindOrAddComponent(AActor *Actor, UClass *ComponentClass, const FString &ComponentName)
{
    if (!Actor || !ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return nullptr;
    }

    if (UActorComponent *Existing = McpFindComponentByClass(Actor, ComponentClass))
    {
        return Existing;
    }

    UActorComponent *Component = NewObject<UActorComponent>(Actor, ComponentClass,
        FName(*(ComponentName.IsEmpty() ? ComponentClass->GetName() : ComponentName)), RF_Transactional);
    if (!Component)
    {
        return nullptr;
    }

    Actor->Modify();
    Actor->AddInstanceComponent(Component);
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
    {
        if (USceneComponent *Root = Actor->GetRootComponent())
        {
            SceneComp->SetupAttachment(Root);
        }
        else
        {
            Actor->SetRootComponent(SceneComp);
        }
    }
    Component->RegisterComponent();
    Actor->MarkPackageDirty();
    return Component;
}

static bool McpConfigureActorAndComponent(const TSharedPtr<FJsonObject> &Payload, const FString &ActorClassPath,
                                          const FString &DefaultActorName, const FString &ComponentClassPath,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    UClass *ActorClass = LoadClass<AActor>(nullptr, *ActorClassPath);
    if (!ActorClass)
    {
        OutMessage = FString::Printf(TEXT("Required actor class is unavailable: %s"), *ActorClassPath);
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        Resp->SetStringField(TEXT("classPath"), ActorClassPath);
        return false;
    }

    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    AActor *Actor = McpFindOrSpawnActor(ActorClass, ActorName.IsEmpty() ? DefaultActorName : ActorName, Location, Rotation);
    if (!Actor)
    {
        OutMessage = FString::Printf(TEXT("Failed to create or find actor for class: %s"), *ActorClassPath);
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    UObject *ConfigTarget = Actor;
    UActorComponent *Component = nullptr;
    if (!ComponentClassPath.IsEmpty())
    {
        UClass *ComponentClass = LoadClass<UActorComponent>(nullptr, *ComponentClassPath);
        if (ComponentClass)
        {
            Component = McpFindOrAddComponent(Actor, ComponentClass, ComponentClass->GetName());
            if (Component)
            {
                ConfigTarget = Component;
            }
        }
    }

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ActorApplied = McpApplyPayloadSettings(Actor, Payload, Applied, Failed);
    int32 ComponentApplied = 0;
    if (Component)
    {
        ComponentApplied = McpApplyPayloadSettings(Component, Payload, Applied, Failed);
        Component->MarkRenderStateDirty();
    }

    Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    Resp->SetStringField(TEXT("classPath"), ActorClassPath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ActorApplied + ComponentApplied);
    if (Component)
    {
        Resp->SetStringField(TEXT("componentName"), Component->GetName());
        Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    }
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    McpHandlerUtils::AddVerification(Resp, Actor);
    if (ConfigTarget)
    {
        Resp->SetStringField(TEXT("configuredTarget"), ConfigTarget->GetPathName());
    }
    OutMessage = FString::Printf(TEXT("Configured environment actor %s"), *Actor->GetActorLabel());
    return true;
}

static ALandscape *McpFindLandscape(const TSharedPtr<FJsonObject> &Payload)
{
    const FString LandscapeName = McpGetFirstStringField(Payload, {TEXT("landscapeName"), TEXT("name"), TEXT("targetActor")});
    FString LandscapePath = McpGetFirstStringField(Payload, {TEXT("landscapePath"), TEXT("landscapeActorPath"), TEXT("actorPath")});
    LandscapePath.TrimStartAndEndInline();
    if (!LandscapePath.IsEmpty() && !LandscapePath.StartsWith(TEXT("/")))
    {
        LandscapePath = TEXT("/") + LandscapePath;
    }

    UWorld *World = McpGetEditorWorld();
    if (World)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            ALandscape *Landscape = *It;
            if (!Landscape)
            {
                continue;
            }
            if (!LandscapeName.IsEmpty() && Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
            {
                return Landscape;
            }
            if (!LandscapePath.IsEmpty() &&
                ((Landscape->GetPackage() && Landscape->GetPackage()->GetPathName().Equals(LandscapePath, ESearchCase::IgnoreCase)) ||
                 Landscape->GetPathName().Equals(LandscapePath, ESearchCase::IgnoreCase) ||
                 Landscape->GetPathName(nullptr).Equals(LandscapePath, ESearchCase::IgnoreCase)))
            {
                return Landscape;
            }
        }

        if (LandscapeName.IsEmpty() && LandscapePath.IsEmpty())
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                return *It;
            }
        }
    }

    if (!LandscapePath.IsEmpty())
    {
        return Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    return nullptr;
}

static bool McpResolveProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError)
{
    OutSafePath = SanitizeProjectFilePath(InputPath);
    if (OutSafePath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid or unsafe project-relative file path: %s"), *InputPath);
        return false;
    }

    OutAbsolutePath = FPaths::ProjectDir() / OutSafePath;
    OutAbsolutePath = FPaths::ConvertRelativePathToFull(OutAbsolutePath);
    FPaths::NormalizeFilename(OutAbsolutePath);
    return McpValidateProjectSnapshotFilePath(OutAbsolutePath, OutError);
}

static bool McpGetLandscapeExtentForEnvironmentAction(ALandscape *Landscape, int32 &OutMinX, int32 &OutMinY, int32 &OutMaxX, int32 &OutMaxY)
{
    ULandscapeInfo *LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
    if (!LandscapeInfo)
    {
        return false;
    }
    if (LandscapeInfo->GetLandscapeExtent(OutMinX, OutMinY, OutMaxX, OutMaxY))
    {
        return true;
    }
    return McpLandscapeMetadataTags::GetLandscapeMetadataExtent(Landscape, OutMinX, OutMinY, OutMaxX, OutMaxY);
}

static void McpApplyHeightmapRegionFromPayload(const TSharedPtr<FJsonObject> &Payload,
                                               const int32 FullMinX, const int32 FullMinY,
                                               const int32 FullMaxX, const int32 FullMaxY,
                                               int32 &OutMinX, int32 &OutMinY,
                                               int32 &OutMaxX, int32 &OutMaxY)
{
    OutMinX = FullMinX;
    OutMinY = FullMinY;
    OutMaxX = FullMaxX;
    OutMaxY = FullMaxY;

    const TSharedPtr<FJsonObject> *RegionObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj && RegionObj->IsValid())
    {
        (*RegionObj)->TryGetNumberField(TEXT("minX"), OutMinX);
        (*RegionObj)->TryGetNumberField(TEXT("minY"), OutMinY);
        (*RegionObj)->TryGetNumberField(TEXT("maxX"), OutMaxX);
        (*RegionObj)->TryGetNumberField(TEXT("maxY"), OutMaxY);
    }
    else
    {
        Payload->TryGetNumberField(TEXT("minX"), OutMinX);
        Payload->TryGetNumberField(TEXT("minY"), OutMinY);
        Payload->TryGetNumberField(TEXT("maxX"), OutMaxX);
        Payload->TryGetNumberField(TEXT("maxY"), OutMaxY);
    }

    OutMinX = FMath::Clamp(OutMinX, FullMinX, FullMaxX);
    OutMinY = FMath::Clamp(OutMinY, FullMinY, FullMaxY);
    OutMaxX = FMath::Clamp(OutMaxX, FullMinX, FullMaxX);
    OutMaxY = FMath::Clamp(OutMaxY, FullMinY, FullMaxY);
}

static bool McpExportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscape(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for export_heightmap");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    FString OutputPath = McpGetFirstStringField(Payload, {TEXT("outputPath"), TEXT("path")});
    if (OutputPath.IsEmpty())
    {
        OutMessage = TEXT("outputPath required for export_heightmap");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString AbsolutePath;
    FString SafePath;
    FString PathError;
    if (!McpResolveProjectFilePath(OutputPath, AbsolutePath, SafePath, PathError))
    {
        OutMessage = PathError;
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 FullMinX = 0, FullMinY = 0, FullMaxX = 0, FullMaxY = 0;
    if (!McpGetLandscapeExtentForEnvironmentAction(Landscape, FullMinX, FullMinY, FullMaxX, FullMaxY))
    {
        OutMessage = TEXT("Failed to read landscape extent");
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        return false;
    }

    int32 MinX = FullMinX;
    int32 MinY = FullMinY;
    int32 MaxX = FullMaxX;
    int32 MaxY = FullMaxY;
    McpApplyHeightmapRegionFromPayload(Payload, FullMinX, FullMinY, FullMaxX, FullMaxY, MinX, MinY, MaxX, MaxY);

    if (MinX > MaxX || MinY > MaxY)
    {
        OutMessage = FString::Printf(TEXT("Invalid heightmap export region: min (%d, %d) must not exceed max (%d, %d)"),
                                     MinX, MinY, MaxX, MaxY);
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    TArray<uint16> Heights;
    Heights.SetNumZeroed(SizeX * SizeY);
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), 0);

    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Heights.Num() * sizeof(uint16));
    FMemory::Memcpy(Bytes.GetData(), Heights.GetData(), Bytes.Num());
    if (!FFileHelper::SaveArrayToFile(Bytes, *AbsolutePath))
    {
        OutMessage = TEXT("Failed to write heightmap file");
        OutErrorCode = TEXT("WRITE_FAILED");
        return false;
    }

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("outputPath"), SafePath);
    Resp->SetNumberField(TEXT("width"), SizeX);
    Resp->SetNumberField(TEXT("height"), SizeY);
    Resp->SetNumberField(TEXT("sampleCount"), Heights.Num());
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape heightmap exported");
    return true;
}

static bool McpImportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscape(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for import_heightmap");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    FString HeightmapPath;
    if (!Payload->TryGetStringField(TEXT("heightmapPath"), HeightmapPath) || HeightmapPath.IsEmpty())
    {
        OutMessage = TEXT("heightmapPath required for import_heightmap when heightData is not provided");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString AbsolutePath;
    FString SafePath;
    FString PathError;
    if (!McpResolveProjectFilePath(HeightmapPath, AbsolutePath, SafePath, PathError))
    {
        OutMessage = PathError;
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath) || Bytes.Num() < 2 || Bytes.Num() % 2 != 0)
    {
        OutMessage = TEXT("heightmapPath must point to a readable raw 16-bit heightmap file");
        OutErrorCode = TEXT("LOAD_FAILED");
        return false;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 FullMinX = 0, FullMinY = 0, FullMaxX = 0, FullMaxY = 0;
    if (!McpGetLandscapeExtentForEnvironmentAction(Landscape, FullMinX, FullMinY, FullMaxX, FullMaxY))
    {
        OutMessage = TEXT("Failed to read landscape extent");
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        return false;
    }

    int32 MinX = FullMinX;
    int32 MinY = FullMinY;
    int32 MaxX = FullMaxX;
    int32 MaxY = FullMaxY;
    McpApplyHeightmapRegionFromPayload(Payload, FullMinX, FullMinY, FullMaxX, FullMaxY, MinX, MinY, MaxX, MaxY);

    if (MinX > MaxX || MinY > MaxY)
    {
        OutMessage = FString::Printf(TEXT("Invalid heightmap import region: min (%d, %d) must not exceed max (%d, %d)"),
                                     MinX, MinY, MaxX, MaxY);
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    const int64 SampleCount64 = static_cast<int64>(SizeX) * static_cast<int64>(SizeY);
    if (SampleCount64 <= 0 || SampleCount64 > MAX_int32)
    {
        OutMessage = TEXT("Invalid heightmap import region size");
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SampleCount = static_cast<int32>(SampleCount64);
    if (Bytes.Num() / sizeof(uint16) < SampleCount)
    {
        OutMessage = FString::Printf(TEXT("Heightmap file has %d samples but selected landscape region needs %d"),
                                     Bytes.Num() / static_cast<int32>(sizeof(uint16)), SampleCount);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    TArray<uint16> Heights;
    Heights.SetNumUninitialized(SampleCount);
    FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), SampleCount * sizeof(uint16));

    bool bUpdateNormals = false;
    Payload->TryGetBoolField(TEXT("updateNormals"), bUpdateNormals);
    bool bSkipFlush = false;
    Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), SizeX, bUpdateNormals);
    if (!bSkipFlush)
    {
        LandscapeEdit.Flush();
    }
    Landscape->MarkPackageDirty();

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("heightmapPath"), SafePath);
    Resp->SetNumberField(TEXT("width"), SizeX);
    Resp->SetNumberField(TEXT("height"), SizeY);
    Resp->SetNumberField(TEXT("sampleCount"), SampleCount);
    Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape heightmap imported");
    return true;
}

static UFoliageType *McpLoadFoliageTypeFromPayload(const TSharedPtr<FJsonObject> &Payload, FString &OutPath)
{
    OutPath = McpGetFirstStringField(Payload, {TEXT("foliageTypePath"), TEXT("foliageType"), TEXT("path")});
    if (OutPath.IsEmpty())
    {
        return nullptr;
    }
    FString SafePath = SanitizeProjectRelativePath(OutPath);
    if (SafePath.IsEmpty())
    {
        return nullptr;
    }
    OutPath = SafePath;
    return LoadObject<UFoliageType>(nullptr, *OutPath);
}

static bool McpConfigureFoliageType(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    FString FoliagePath;
    UFoliageType *FoliageType = McpLoadFoliageTypeFromPayload(Payload, FoliagePath);
    if (!FoliageType)
    {
        OutMessage = TEXT("Foliage type asset not found");
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        return false;
    }

    if (UFoliageType_InstancedStaticMesh *Instanced = Cast<UFoliageType_InstancedStaticMesh>(FoliageType))
    {
        FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
        if (!MeshPath.IsEmpty())
        {
            if (UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
            {
                Instanced->SetStaticMesh(Mesh);
                Resp->SetStringField(TEXT("meshPath"), MeshPath);
            }
        }
    }

    double NumberValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("density"), NumberValue))
    {
        FoliageType->Density = static_cast<float>(NumberValue);
    }

    double MinScale = 0.0;
    double MaxScale = 0.0;
    if (Payload->TryGetNumberField(TEXT("minScale"), MinScale) || Payload->TryGetNumberField(TEXT("maxScale"), MaxScale))
    {
        if (MinScale <= 0.0)
        {
            MinScale = FoliageType->ScaleX.Min;
        }
        if (MaxScale <= 0.0)
        {
            MaxScale = FoliageType->ScaleX.Max;
        }
        FoliageType->Scaling = EFoliageScaling::Uniform;
        FoliageType->ScaleX = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
        FoliageType->ScaleY = FoliageType->ScaleX;
        FoliageType->ScaleZ = FoliageType->ScaleX;
    }

    bool BoolValue = false;
    if (Payload->TryGetBoolField(TEXT("alignToNormal"), BoolValue))
    {
        FoliageType->AlignToNormal = BoolValue;
    }
    if (Payload->TryGetBoolField(TEXT("randomYaw"), BoolValue))
    {
        FoliageType->RandomYaw = BoolValue;
    }

    int32 CullDistance = 0;
    if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance))
    {
        FoliageType->CullDistance.Max = CullDistance;
    }

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ReflectedCount = McpApplyPayloadSettings(FoliageType, Payload, Applied, Failed);
    FoliageType->MarkPackageDirty();
    McpSafeAssetSave(FoliageType);

    Resp->SetStringField(TEXT("foliageTypePath"), FoliagePath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ReflectedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    McpHandlerUtils::AddVerification(Resp, FoliageType);
    OutMessage = TEXT("Foliage type configured");
    return true;
}

static bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("name")});
    if (LayerName.IsEmpty())
    {
        OutMessage = TEXT("layerName or name required for create_landscape_layer_info");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("layerInfoPath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Landscape");
    }
    FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty())
    {
        OutMessage = FString::Printf(TEXT("Invalid layer info path: %s"), *Path);
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    FString PackagePath = SafePath;
    if (!FPaths::GetBaseFilename(PackagePath).Equals(LayerName, ESearchCase::IgnoreCase))
    {
        PackagePath = SafePath / LayerName;
    }

    UPackage *Package = CreatePackage(*PackagePath);
    ULandscapeLayerInfoObject *LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*LayerName), RF_Public | RF_Standalone);
    if (!LayerInfo)
    {
        OutMessage = TEXT("Failed to create landscape layer info");
        OutErrorCode = TEXT("CREATION_FAILED");
        return false;
    }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
    LayerInfo->SetLayerName(FName(*LayerName), true);
#else
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

    double Hardness = 0.0;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness))
    {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    FString PhysicalMaterialPath;
    if (Payload->TryGetStringField(TEXT("physicalMaterialPath"), PhysicalMaterialPath) && !PhysicalMaterialPath.IsEmpty())
    {
        if (UPhysicalMaterial *PhysicalMaterial = LoadObject<UPhysicalMaterial>(nullptr, *PhysicalMaterialPath))
        {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            LayerInfo->PhysMaterial = PhysicalMaterial;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
    }

    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend))
    {
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
        LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
        LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
#endif
    }

    FAssetRegistryModule::AssetCreated(LayerInfo);
    LayerInfo->MarkPackageDirty();
    McpSafeAssetSave(LayerInfo);

    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetStringField(TEXT("assetPath"), LayerInfo->GetPathName());
    McpHandlerUtils::AddVerification(Resp, LayerInfo);
    OutMessage = TEXT("Landscape layer info created");
    return true;
}

static bool McpCreateLinearColorCurve(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                      TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    FString Name = McpGetFirstStringField(Payload, {TEXT("name"), TEXT("curveName")});
    if (Name.IsEmpty())
    {
        Name = DefaultName;
    }
    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("curvePath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Environment/Curves");
    }
    FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty())
    {
        OutMessage = FString::Printf(TEXT("Invalid curve path: %s"), *Path);
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    FString PackagePath = SafePath;
    if (!FPaths::GetBaseFilename(PackagePath).Equals(Name, ESearchCase::IgnoreCase))
    {
        PackagePath = SafePath / Name;
    }
    UPackage *Package = CreatePackage(*PackagePath);
    UCurveLinearColor *Curve = NewObject<UCurveLinearColor>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (!Curve)
    {
        OutMessage = TEXT("Failed to create color curve");
        OutErrorCode = TEXT("CREATION_FAILED");
        return false;
    }

    Curve->FloatCurves[0].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[1].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[2].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[3].UpdateOrAddKey(0.0f, 1.0f);
    FAssetRegistryModule::AssetCreated(Curve);
    Curve->MarkPackageDirty();
    McpSafeAssetSave(Curve);

    Resp->SetStringField(TEXT("curvePath"), Curve->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Curve);
    OutMessage = TEXT("Color curve created");
    return true;
}

static ALandscape *McpFindLandscapeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload)
{
    return McpFindLandscape(Payload);
}

static bool McpBuildProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError)
{
    return McpResolveProjectFilePath(InputPath, OutAbsolutePath, OutSafePath, OutError);
}

static AActor *McpFindOrSpawnEnvironmentActor(const TSharedPtr<FJsonObject> &Payload, UClass *ActorClass, const FString &DefaultActorName)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    return McpFindOrSpawnActor(ActorClass, ActorName.IsEmpty() ? DefaultActorName : ActorName, Location, Rotation);
}

static void McpApplyEnvironmentSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 AppliedCount = McpApplyPayloadSettings(Target, Payload, Applied, Failed);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), AppliedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
}

static UFoliageType *McpLoadFoliageTypeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload)
{
    FString FoliagePath;
    return McpLoadFoliageTypeFromPayload(Payload, FoliagePath);
}

static AActor *McpFindActorFromEnvironmentPayload(const TSharedPtr<FJsonObject> &Payload)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name"), TEXT("actorPath")});
    if (ActorName.IsEmpty())
    {
        return nullptr;
    }
    if (AActor *ActorByPath = FindObject<AActor>(nullptr, *ActorName))
    {
        return ActorByPath;
    }
    return McpFindActorByNameOrClass(nullptr, ActorName);
}

static AActor *McpFindWaterBodyActor(const TSharedPtr<FJsonObject> &Payload)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("waterBodyName"), TEXT("targetActor"), TEXT("actorName"), TEXT("name")});
    const TArray<FString> ClassPaths = {
        TEXT("/Script/Water.WaterBodyOcean"), TEXT("/Script/Water.WaterBodyLake"),
        TEXT("/Script/Water.WaterBodyRiver"), TEXT("/Script/Water.WaterBodyCustom")
    };
    for (const FString &ClassPath : ClassPaths)
    {
        if (UClass *WaterClass = LoadClass<AActor>(nullptr, *ClassPath))
        {
            if (AActor *Actor = McpFindActorByNameOrClass(WaterClass, ActorName))
            {
                return Actor;
            }
        }
    }
    return ActorName.IsEmpty() ? nullptr : McpFindActorFromEnvironmentPayload(Payload);
}

static int32 McpSetMaterialOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    FString MaterialPath;
    if (!Actor || !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return 0;
    }
    UMaterialInterface *Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        Resp->SetStringField(TEXT("materialError"), FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
        return 0;
    }

    int32 MaterialIndex = 0;
    Payload->TryGetNumberField(TEXT("materialIndex"), MaterialIndex);
    int32 AppliedCount = 0;
    TInlineComponentArray<UPrimitiveComponent *> Components;
    Actor->GetComponents(Components);
    for (UPrimitiveComponent *Component : Components)
    {
        if (Component)
        {
            Component->Modify();
            Component->SetMaterial(MaterialIndex, Material);
            Component->MarkRenderStateDirty();
            ++AppliedCount;
        }
    }
    if (AppliedCount > 0)
    {
        Actor->MarkPackageDirty();
        Resp->SetStringField(TEXT("materialPath"), MaterialPath);
        Resp->SetNumberField(TEXT("materialComponentCount"), AppliedCount);
    }
    return AppliedCount;
}

static int32 McpSetCollisionOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    bool bCollisionEnabled = true;
    if (!Actor || !Payload->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled))
    {
        return 0;
    }
    int32 AppliedCount = 0;
    TInlineComponentArray<UPrimitiveComponent *> Components;
    Actor->GetComponents(Components);
    for (UPrimitiveComponent *Component : Components)
    {
        if (Component)
        {
            Component->Modify();
            Component->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            Component->MarkRenderStateDirty();
            ++AppliedCount;
        }
    }
    Resp->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
    Resp->SetNumberField(TEXT("collisionComponentCount"), AppliedCount);
    return AppliedCount;
}

static bool McpConfigureParticleEmitter(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                        TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    AActor *Emitter = nullptr;
    UClass *EmitterClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.Emitter"));
    if (EmitterClass)
    {
        Emitter = McpFindOrSpawnEnvironmentActor(Payload, EmitterClass, DefaultName);
    }
    if (!Emitter)
    {
        OutMessage = TEXT("Failed to create particle emitter actor");
        OutErrorCode = EmitterClass ? TEXT("SPAWN_FAILED") : TEXT("CLASS_NOT_FOUND");
        return false;
    }

    UParticleSystemComponent *ParticleComponent = Cast<UParticleSystemComponent>(McpFindOrAddComponent(Emitter, UParticleSystemComponent::StaticClass(), TEXT("ParticleSystem")));
    FString ParticleSystemPath;
    if (ParticleComponent && Payload->TryGetStringField(TEXT("particleSystemPath"), ParticleSystemPath) && !ParticleSystemPath.IsEmpty())
    {
        if (UParticleSystem *ParticleSystem = LoadObject<UParticleSystem>(nullptr, *ParticleSystemPath))
        {
            ParticleComponent->Modify();
            ParticleComponent->SetTemplate(ParticleSystem);
            ParticleComponent->MarkRenderStateDirty();
            Resp->SetStringField(TEXT("particleSystemPath"), ParticleSystemPath);
        }
        else
        {
            Resp->SetStringField(TEXT("particleSystemError"), FString::Printf(TEXT("Particle system not found: %s"), *ParticleSystemPath));
        }
    }

    McpApplyEnvironmentSettings(Emitter, Payload, Resp);
    if (ParticleComponent)
    {
        McpApplyEnvironmentSettings(ParticleComponent, Payload, Resp);
        Resp->SetStringField(TEXT("componentName"), ParticleComponent->GetName());
    }
    Resp->SetStringField(TEXT("actorName"), Emitter->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Emitter->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Emitter);
    OutMessage = FString::Printf(TEXT("Configured particle emitter %s"), *Emitter->GetActorLabel());
    return true;
}

static bool McpConfigureSunPosition(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    AActor *SunActor = McpFindOrSpawnEnvironmentActor(Payload, ADirectionalLight::StaticClass(), TEXT("SunLight"));
    if (!SunActor)
    {
        OutMessage = TEXT("Failed to create or find directional light");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    double Azimuth = SunActor->GetActorRotation().Yaw;
    double Elevation = SunActor->GetActorRotation().Pitch;
    double Hour = 0.0;
    if (Payload->TryGetNumberField(TEXT("hour"), Hour) || Payload->TryGetNumberField(TEXT("time"), Hour))
    {
        Elevation = (FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f) / 24.0f) * 360.0f - 90.0f;
    }
    Payload->TryGetNumberField(TEXT("azimuth"), Azimuth);
    Payload->TryGetNumberField(TEXT("elevation"), Elevation);

    SunActor->Modify();
    SunActor->SetActorRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    McpApplyEnvironmentSettings(SunActor, Payload, Resp);
    if (UDirectionalLightComponent *LightComponent = Cast<UDirectionalLightComponent>(SunActor->FindComponentByClass<UDirectionalLightComponent>()))
    {
        McpApplyEnvironmentSettings(LightComponent, Payload, Resp);
        LightComponent->MarkRenderStateDirty();
    }

    Resp->SetStringField(TEXT("actorName"), SunActor->GetActorLabel());
    Resp->SetNumberField(TEXT("azimuth"), Azimuth);
    Resp->SetNumberField(TEXT("elevation"), Elevation);
    McpHandlerUtils::AddVerification(Resp, SunActor);
    OutMessage = TEXT("Sun position configured");
    return true;
}

static bool McpConfigureWaterBody(const TSharedPtr<FJsonObject> &Payload, const FString &ClassPath, const FString &DefaultName,
                                  TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    const bool bCreatedOrFound = McpConfigureActorAndComponent(Payload, ClassPath, DefaultName, FString(), Resp, OutMessage, OutErrorCode);
    if (bCreatedOrFound)
    {
        if (AActor *WaterActor = McpFindWaterBodyActor(Payload))
        {
            McpSetMaterialOnActor(WaterActor, Payload, Resp);
            McpSetCollisionOnActor(WaterActor, Payload, Resp);
        }
    }
    return bCreatedOrFound;
}

static bool McpConfigureWaterBodyActor(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode)
{
    AActor *WaterActor = McpFindWaterBodyActor(Payload);
    if (!WaterActor)
    {
        OutMessage = TEXT("Water body actor not found");
        OutErrorCode = TEXT("WATER_BODY_NOT_FOUND");
        return false;
    }

    WaterActor->Modify();
    McpApplyEnvironmentSettings(WaterActor, Payload, Resp);
    McpSetMaterialOnActor(WaterActor, Payload, Resp);
    McpSetCollisionOnActor(WaterActor, Payload, Resp);
    WaterActor->MarkPackageDirty();
    Resp->SetStringField(TEXT("waterBodyName"), WaterActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), WaterActor->GetPathName());
    McpHandlerUtils::AddVerification(Resp, WaterActor);
    OutMessage = TEXT("Water body configured");
    return true;
}

static bool McpPayloadHasWaterWaveSettings(const TSharedPtr<FJsonObject> &Payload)
{
    double NumberValue = 0.0;
    const TSharedPtr<FJsonObject> *DirectionObj = nullptr;
    return McpGetFirstNumberField(Payload, {TEXT("waveHeight"), TEXT("waveLength"), TEXT("amplitude")}, NumberValue) ||
           (Payload.IsValid() && Payload->TryGetObjectField(TEXT("direction"), DirectionObj) && DirectionObj && DirectionObj->IsValid());
}

static bool McpTryGetNumberFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, double &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    if (Payload->TryGetNumberField(FieldName, OutValue))
    {
        return true;
    }

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    return Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid() &&
           (*SettingsObj)->TryGetNumberField(FieldName, OutValue);
}

static bool McpTryGetBoolFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, bool &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    if (Payload->TryGetBoolField(FieldName, OutValue))
    {
        return true;
    }

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    return Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid() &&
           (*SettingsObj)->TryGetBoolField(FieldName, OutValue);
}

static bool McpReadLandscapeSplinePoint(const TSharedPtr<FJsonValue> &PointValue, FVector &OutPoint)
{
    const TSharedPtr<FJsonObject> *PointObject = nullptr;
    if (!PointValue.IsValid() || !PointValue->TryGetObject(PointObject) || !PointObject || !PointObject->IsValid())
    {
        return false;
    }

    auto TryReadVector = [](const TSharedPtr<FJsonObject> &VectorObject, FVector &OutVector) -> bool
    {
        if (!VectorObject.IsValid())
        {
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!VectorObject->TryGetNumberField(TEXT("x"), X) ||
            !VectorObject->TryGetNumberField(TEXT("y"), Y) ||
            !VectorObject->TryGetNumberField(TEXT("z"), Z))
        {
            return false;
        }

        OutVector = FVector(X, Y, Z);
        return true;
    };

    const TSharedPtr<FJsonObject> *LocationObject = nullptr;
    if ((*PointObject)->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject && LocationObject->IsValid())
    {
        return TryReadVector(*LocationObject, OutPoint);
    }

    return TryReadVector(*PointObject, OutPoint);
}

static void McpClearLandscapeSplines(ULandscapeSplinesComponent *SplinesComponent)
{
    if (!SplinesComponent)
    {
        return;
    }

    for (TObjectPtr<ULandscapeSplineSegment> &SegmentPtr : SplinesComponent->GetSegments())
    {
        if (ULandscapeSplineSegment *Segment = SegmentPtr.Get())
        {
            Segment->Modify();
            Segment->DeleteSplinePoints();
        }
    }
    for (TObjectPtr<ULandscapeSplineControlPoint> &ControlPointPtr : SplinesComponent->GetControlPoints())
    {
        if (ULandscapeSplineControlPoint *ControlPoint = ControlPointPtr.Get())
        {
            ControlPoint->Modify();
            ControlPoint->DeleteSplinePoints();
        }
    }

    SplinesComponent->GetSegments().Empty();
    SplinesComponent->GetControlPoints().Empty();
}

static ULandscapeSplineSegment *McpAddLandscapeSplineSegment(ULandscapeSplinesComponent *SplinesComponent,
                                                            ULandscapeSplineControlPoint *Start,
                                                            ULandscapeSplineControlPoint *End)
{
    if (!SplinesComponent || !Start || !End || Start == End)
    {
        return nullptr;
    }

    ULandscapeSplineSegment *Segment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
    if (!Segment)
    {
        return nullptr;
    }

    SplinesComponent->GetSegments().Add(Segment);
    Segment->Connections[0].ControlPoint = Start;
    Segment->Connections[1].ControlPoint = End;
    Segment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
    Segment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

    FVector StartLocation;
    FRotator StartRotation;
    FVector EndLocation;
    FRotator EndRotation;
    Start->GetConnectionLocationAndRotation(Segment->Connections[0].SocketName, StartLocation, StartRotation);
    End->GetConnectionLocationAndRotation(Segment->Connections[1].SocketName, EndLocation, EndRotation);
    Segment->Connections[0].TangentLen = static_cast<float>((EndLocation - StartLocation).Size());
    Segment->Connections[1].TangentLen = Segment->Connections[0].TangentLen;
    Segment->AutoFlipTangents();

    Start->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
    End->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));
    return Segment;
}

static bool McpConfigureLandscapeSplines(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                         FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for spline configuration");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>> *Points = nullptr;
    if (!Payload->TryGetArrayField(TEXT("points"), Points) || !Points || Points->Num() < 2)
    {
        OutMessage = TEXT("configure_landscape_splines requires at least two points");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    double WidthValue = 256.0;
    Payload->TryGetNumberField(TEXT("width"), WidthValue);
    const float Width = FMath::Max(0.0f, static_cast<float>(WidthValue));

    double SideFalloffValue = WidthValue * 0.5;
    Payload->TryGetNumberField(TEXT("sideFalloff"), SideFalloffValue);
    const float SideFalloff = FMath::Max(0.0f, static_cast<float>(SideFalloffValue));

    bool bClosedLoop = false;
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("closedLoop"), bClosedLoop);
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("ClosedLoop"), bClosedLoop);

    const FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("splineLayerName")});
    bool bRaiseTerrain = true;
    bool bLowerTerrain = true;
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("raiseTerrain"), bRaiseTerrain);
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("lowerTerrain"), bLowerTerrain);

    TArray<FVector> WorldPoints;
    WorldPoints.Reserve(Points->Num());
    for (int32 Index = 0; Index < Points->Num(); ++Index)
    {
        FVector WorldPoint = FVector::ZeroVector;
        if (!McpReadLandscapeSplinePoint((*Points)[Index], WorldPoint))
        {
            OutMessage = FString::Printf(TEXT("Invalid landscape spline point at index %d"), Index);
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
        WorldPoints.Add(WorldPoint);
    }

    ULandscapeSplinesComponent *SplinesComponent = Landscape->GetSplinesComponent();
    if (!SplinesComponent)
    {
        Landscape->Modify();
        Landscape->CreateSplineComponent();
        SplinesComponent = Landscape->GetSplinesComponent();
    }
    if (!SplinesComponent)
    {
        OutMessage = TEXT("Failed to create landscape splines component");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    TArray<ULandscapeSplineControlPoint *> ControlPoints;
    ControlPoints.Reserve(WorldPoints.Num());
    for (const FVector &WorldPoint : WorldPoints)
    {
        ULandscapeSplineControlPoint *ControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
        if (!ControlPoint)
        {
            OutMessage = TEXT("Failed to create landscape spline control point");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }

        ControlPoint->Location = Landscape->GetTransform().InverseTransformPosition(WorldPoint);
        ControlPoint->Width = Width;
        ControlPoint->SideFalloff = SideFalloff;
        ControlPoint->EndFalloff = SideFalloff;
        ControlPoint->bRaiseTerrain = bRaiseTerrain;
        ControlPoint->bLowerTerrain = bLowerTerrain;
        if (!LayerName.IsEmpty())
        {
            ControlPoint->LayerName = FName(*LayerName);
        }
        ControlPoints.Add(ControlPoint);
    }

    Landscape->Modify();
    SplinesComponent->Modify();
    McpClearLandscapeSplines(SplinesComponent);
    for (ULandscapeSplineControlPoint *ControlPoint : ControlPoints)
    {
        ControlPoint->Modify();
        SplinesComponent->GetControlPoints().Add(ControlPoint);
    }

    TArray<ULandscapeSplineSegment *> Segments;
    for (int32 Index = 0; Index < ControlPoints.Num() - 1; ++Index)
    {
        if (ULandscapeSplineSegment *Segment = McpAddLandscapeSplineSegment(SplinesComponent, ControlPoints[Index], ControlPoints[Index + 1]))
        {
            Segment->bRaiseTerrain = bRaiseTerrain;
            Segment->bLowerTerrain = bLowerTerrain;
            if (!LayerName.IsEmpty())
            {
                Segment->LayerName = FName(*LayerName);
            }
            Segments.Add(Segment);
        }
    }
    if (bClosedLoop && ControlPoints.Num() > 2)
    {
        if (ULandscapeSplineSegment *Segment = McpAddLandscapeSplineSegment(SplinesComponent, ControlPoints.Last(), ControlPoints[0]))
        {
            Segment->bRaiseTerrain = bRaiseTerrain;
            Segment->bLowerTerrain = bLowerTerrain;
            if (!LayerName.IsEmpty())
            {
                Segment->LayerName = FName(*LayerName);
            }
            Segments.Add(Segment);
        }
    }

    for (ULandscapeSplineControlPoint *ControlPoint : ControlPoints)
    {
        ControlPoint->AutoCalcRotation(false);
        ControlPoint->UpdateSplinePoints(false, true, false);
    }
    for (ULandscapeSplineSegment *Segment : Segments)
    {
        Segment->UpdateSplinePoints(false, false);
    }

    if (!SplinesComponent->IsRegistered())
    {
        SplinesComponent->RegisterComponent();
    }
    SplinesComponent->RebuildAllSplines(false);
    SplinesComponent->MarkRenderStateDirty();
    SplinesComponent->MarkPackageDirty();
    Landscape->MarkPackageDirty();
    Landscape->PostEditChange();

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage() ? Landscape->GetPackage()->GetPathName() : Landscape->GetPathName());
    Resp->SetStringField(TEXT("actorPath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("componentName"), SplinesComponent->GetName());
    Resp->SetStringField(TEXT("componentPath"), SplinesComponent->GetPathName());
    Resp->SetNumberField(TEXT("pointCount"), ControlPoints.Num());
    Resp->SetNumberField(TEXT("segmentCount"), Segments.Num());
    Resp->SetNumberField(TEXT("width"), Width);
    Resp->SetBoolField(TEXT("closedLoop"), bClosedLoop);
    Resp->SetBoolField(TEXT("raiseTerrain"), bRaiseTerrain);
    Resp->SetBoolField(TEXT("lowerTerrain"), bLowerTerrain);
    if (!LayerName.IsEmpty())
    {
        Resp->SetStringField(TEXT("layerName"), LayerName);
    }
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape spline configuration updated");
    return true;
}

static bool McpCreateLandscapeStreamingProxy(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                            FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for streaming proxy creation");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), Landscape->GetActorLocation());
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), Landscape->GetActorRotation());

    ALandscapeStreamingProxy *Proxy = Cast<ALandscapeStreamingProxy>(
        McpFindOrSpawnActor(ALandscapeStreamingProxy::StaticClass(), ActorName.IsEmpty() ? TEXT("LandscapeStreamingProxy") : ActorName, Location, Rotation));
    if (!Proxy)
    {
        OutMessage = TEXT("Failed to create landscape streaming proxy");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    Landscape->Modify();
    Proxy->Modify();
    Proxy->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    Proxy->SetLandscapeGuid(Landscape->GetLandscapeGuid(), false);
    Proxy->SetLandscapeActor(Landscape);
    Proxy->CopySharedProperties(Landscape);
    Proxy->CreateLandscapeInfo(false, false);
    Proxy->MarkPackageDirty();
    Proxy->PostEditChange();

    Resp->SetStringField(TEXT("actorName"), Proxy->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Proxy->GetPathName());
    Resp->SetStringField(TEXT("sourceLandscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("sourceLandscapePath"), Landscape->GetPackage() ? Landscape->GetPackage()->GetPathName() : Landscape->GetPathName());
    Resp->SetStringField(TEXT("sourceLandscapeActorPath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("landscapeGuid"), Landscape->GetLandscapeGuid().ToString());
    Resp->SetBoolField(TEXT("linkedToLandscape"), Proxy->GetLandscapeActor() == Landscape);
    McpHandlerUtils::AddVerification(Resp, Proxy);
    OutMessage = TEXT("Landscape streaming proxy created");
    return true;
}

static bool McpCreateTimeOfDaySystem(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    AActor *Actor = McpFindOrSpawnEnvironmentActor(Payload, AActor::StaticClass(), TEXT("TimeOfDaySystem"));
    if (!Actor)
    {
        OutMessage = TEXT("Failed to create time-of-day system actor");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    UDirectionalLightComponent *SunComponent = Cast<UDirectionalLightComponent>(
        McpFindOrAddComponent(Actor, UDirectionalLightComponent::StaticClass(), TEXT("TimeOfDaySun")));
    USkyLightComponent *SkyLightComponent = Cast<USkyLightComponent>(
        McpFindOrAddComponent(Actor, USkyLightComponent::StaticClass(), TEXT("TimeOfDaySkyLight")));
    USkyAtmosphereComponent *SkyAtmosphereComponent = Cast<USkyAtmosphereComponent>(
        McpFindOrAddComponent(Actor, USkyAtmosphereComponent::StaticClass(), TEXT("TimeOfDaySkyAtmosphere")));
    if (!SunComponent || !SkyLightComponent || !SkyAtmosphereComponent)
    {
        OutMessage = TEXT("Failed to create time-of-day lighting components");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    double Hour = 12.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("CurrentHour"), Hour);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("hour"), Hour);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("time"), Hour);
    const float ClampedHour = FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);

    double Azimuth = 0.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("azimuth"), Azimuth);
    double Elevation = (ClampedHour / 24.0f) * 360.0f - 90.0f;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("elevation"), Elevation);

    double SunIntensity = 10.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("intensity"), SunIntensity);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("sunIntensity"), SunIntensity);
    double SkyIntensity = 1.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("skyLightIntensity"), SkyIntensity);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("skylightIntensity"), SkyIntensity);

    Actor->Modify();
    Actor->SetActorRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->Modify();
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->SetRelativeRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->SetIntensity(static_cast<float>(SunIntensity));
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);
    SunComponent->MarkRenderStateDirty();

    SkyLightComponent->Modify();
    SkyLightComponent->SetMobility(EComponentMobility::Movable);
    SkyLightComponent->SetIntensity(static_cast<float>(SkyIntensity));
    SkyLightComponent->MarkRenderStateDirty();

    SkyAtmosphereComponent->Modify();
    SkyAtmosphereComponent->SetMobility(EComponentMobility::Movable);
    SkyAtmosphereComponent->MarkRenderStateDirty();

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ActorApplied = McpApplyPayloadSettings(Actor, Payload, Applied, Failed);
    const int32 SunApplied = McpApplyPayloadSettings(SunComponent, Payload, Applied, Failed);
    const int32 SkyLightApplied = McpApplyPayloadSettings(SkyLightComponent, Payload, Applied, Failed);
    const int32 SkyAtmosphereApplied = McpApplyPayloadSettings(SkyAtmosphereComponent, Payload, Applied, Failed);

    SunComponent->SetRelativeRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->SetIntensity(static_cast<float>(SunIntensity));
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);
    SunComponent->MarkRenderStateDirty();
    SkyLightComponent->SetIntensity(static_cast<float>(SkyIntensity));
    SkyLightComponent->MarkRenderStateDirty();
    SkyAtmosphereComponent->MarkRenderStateDirty();

    Actor->MarkPackageDirty();
    Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    Resp->SetStringField(TEXT("sunComponentName"), SunComponent->GetName());
    Resp->SetStringField(TEXT("skyLightComponentName"), SkyLightComponent->GetName());
    Resp->SetStringField(TEXT("skyAtmosphereComponentName"), SkyAtmosphereComponent->GetName());
    Resp->SetBoolField(TEXT("hasSunLight"), true);
    Resp->SetBoolField(TEXT("hasSkyLight"), true);
    Resp->SetBoolField(TEXT("hasSkyAtmosphere"), true);
    Resp->SetNumberField(TEXT("componentCount"), 3);
    Resp->SetNumberField(TEXT("currentHour"), ClampedHour);
    Resp->SetNumberField(TEXT("azimuth"), Azimuth);
    Resp->SetNumberField(TEXT("elevation"), Elevation);
    Resp->SetNumberField(TEXT("sunIntensity"), SunIntensity);
    Resp->SetNumberField(TEXT("skyLightIntensity"), SkyIntensity);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ActorApplied + SunApplied + SkyLightApplied + SkyAtmosphereApplied);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    McpHandlerUtils::AddVerification(Resp, Actor);
    OutMessage = TEXT("Time-of-day lighting rig created");
    return true;
}

static bool McpConfigureWaterWavesOnActor(AActor *WaterActor, const TSharedPtr<FJsonObject> &Payload,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    if (!WaterActor)
    {
        OutMessage = TEXT("Water body actor not found");
        OutErrorCode = TEXT("WATER_BODY_NOT_FOUND");
        return false;
    }

    UClass *GerstnerWavesClass = LoadClass<UObject>(nullptr, TEXT("/Script/Water.GerstnerWaterWaves"));
    UClass *GerstnerGeneratorClass = LoadClass<UObject>(nullptr, TEXT("/Script/Water.GerstnerWaterWaveGeneratorSimple"));
    if (!GerstnerWavesClass || !GerstnerGeneratorClass)
    {
        OutMessage = TEXT("Water plugin Gerstner wave classes are unavailable");
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        return false;
    }

    UObject *WaterWaves = McpGetObjectPropertyValue(WaterActor, TEXT("WaterWaves"));
    if (!WaterWaves)
    {
        WaterWaves = McpInvokeObjectGetter(WaterActor, FName(TEXT("GetWaterWaves")));
    }
    if (!WaterWaves || !WaterWaves->IsA(GerstnerWavesClass))
    {
        WaterWaves = NewObject<UObject>(WaterActor, GerstnerWavesClass,
            MakeUniqueObjectName(WaterActor, GerstnerWavesClass, TEXT("McpGerstnerWaterWaves")), RF_Transactional);
        if (!WaterWaves)
        {
            OutMessage = TEXT("Failed to create Gerstner water waves");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }

        if (!McpInvokeObjectSetter(WaterActor, FName(TEXT("SetWaterWaves")), WaterWaves) &&
            !McpSetObjectPropertyValue(WaterActor, TEXT("WaterWaves"), WaterWaves))
        {
            OutMessage = TEXT("Failed to assign Gerstner water waves to water body");
            OutErrorCode = TEXT("PROPERTY_SET_FAILED");
            return false;
        }
    }

    UObject *Generator = McpGetObjectPropertyValue(WaterWaves, TEXT("GerstnerWaveGenerator"));
    if (!Generator || !Generator->IsA(GerstnerGeneratorClass))
    {
        Generator = NewObject<UObject>(WaterWaves, GerstnerGeneratorClass,
            MakeUniqueObjectName(WaterWaves, GerstnerGeneratorClass, TEXT("McpGerstnerWaterWaveGenerator")), RF_Transactional);
        if (!Generator || !McpSetObjectPropertyValue(WaterWaves, TEXT("GerstnerWaveGenerator"), Generator))
        {
            OutMessage = TEXT("Failed to create Gerstner wave generator");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }
    }

    TArray<FString> Applied;
    double NumberValue = 0.0;
    if (McpGetFirstNumberField(Payload, {TEXT("waveHeight")}, NumberValue))
    {
        const double Height = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinAmplitude")}, Height, TEXT("waveHeight"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxAmplitude")}, Height, TEXT("waveHeight"), Resp, Applied);
    }
    else if (McpGetFirstNumberField(Payload, {TEXT("amplitude")}, NumberValue))
    {
        const double Height = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinAmplitude")}, Height, TEXT("amplitude"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxAmplitude")}, Height, TEXT("amplitude"), Resp, Applied);
    }

    if (McpGetFirstNumberField(Payload, {TEXT("waveLength")}, NumberValue))
    {
        const double Wavelength = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinWavelength")}, Wavelength, TEXT("waveLength"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxWavelength")}, Wavelength, TEXT("waveLength"), Resp, Applied);
    }

    if (McpGetFirstNumberField(Payload, {TEXT("steepness")}, NumberValue))
    {
        const double Steepness = FMath::Clamp(NumberValue, 0.0, 1.0);
        McpApplyNumberProperty(Generator, {TEXT("SmallWaveSteepness")}, Steepness, TEXT("steepness"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("LargeWaveSteepness")}, Steepness, TEXT("steepness"), Resp, Applied);
    }

    const TSharedPtr<FJsonObject> *DirectionObj = nullptr;
    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("direction"), DirectionObj) && DirectionObj && DirectionObj->IsValid())
    {
        const FRotator Direction = McpGetRotatorField(Payload, TEXT("direction"), FRotator::ZeroRotator);
        McpApplyNumberProperty(Generator, {TEXT("WindAngleDeg")}, Direction.Yaw, TEXT("directionYaw"), Resp, Applied);
    }

    if (Applied.Num() == 0)
    {
        OutMessage = TEXT("No supported water wave properties were applied");
        OutErrorCode = TEXT("PROPERTY_NOT_FOUND");
        Resp->SetBoolField(TEXT("waterWaveConfigured"), false);
        return false;
    }

    Generator->Modify();
    Generator->MarkPackageDirty();
    WaterWaves->Modify();
    WaterWaves->MarkPackageDirty();
    WaterWaves->PostEditChange();
    WaterActor->MarkPackageDirty();

    Resp->SetBoolField(TEXT("waterWaveConfigured"), true);
    Resp->SetStringField(TEXT("waterWaveClass"), WaterWaves->GetClass()->GetPathName());
    Resp->SetStringField(TEXT("waveGeneratorClass"), Generator->GetClass()->GetPathName());
    Resp->SetNumberField(TEXT("waterWaveConfiguredPropertyCount"), Applied.Num());
    McpAddStringArrayField(Resp, TEXT("waterWaveConfiguredProperties"), Applied);
    OutMessage = TEXT("Water waves configured");
    return true;
}

static bool McpCreateBuoyancyComponent(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode)
{
    AActor *TargetActor = McpFindActorFromEnvironmentPayload(Payload);
    if (!TargetActor)
    {
        OutMessage = TEXT("actorPath, targetActor, actorName, or name required for create_buoyancy_component");
        OutErrorCode = TEXT("ACTOR_NOT_FOUND");
        return false;
    }
    UClass *BuoyancyClass = LoadClass<UActorComponent>(nullptr, TEXT("/Script/Water.BuoyancyComponent"));
    if (!BuoyancyClass)
    {
        OutMessage = TEXT("Water plugin buoyancy component class is unavailable");
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        return false;
    }
    UActorComponent *Component = McpFindOrAddComponent(TargetActor, BuoyancyClass, TEXT("BuoyancyComponent"));
    if (!Component)
    {
        OutMessage = TEXT("Failed to create buoyancy component");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    McpApplyEnvironmentSettings(Component, Payload, Resp);
    Resp->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    Resp->SetStringField(TEXT("componentName"), Component->GetName());
    Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    McpHandlerUtils::AddVerification(Resp, TargetActor);
    OutMessage = TEXT("Buoyancy component created");
    return true;
}

static UWorld *McpGetRuntimeInspectionWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }

    if (GEditor->PlayWorld)
    {
        return GEditor->PlayWorld.Get();
    }

    if (GEngine)
    {
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            {
                if (UWorld *World = Context.World())
                {
                    return World;
                }
            }
        }
    }

    return GEditor->GetEditorWorldContext().World();
}

static FString McpGetWorldTypeName(UWorld *World)
{
    if (!World)
    {
        return TEXT("None");
    }

    switch (World->WorldType)
    {
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::Game:
        return TEXT("Game");
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::GameRPC:
        return TEXT("GameRPC");
    case EWorldType::Inactive:
        return TEXT("Inactive");
    default:
        return TEXT("Unknown");
    }
}

static void McpAddActorTags(TSharedPtr<FJsonObject> Obj, const AActor *Actor)
{
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    if (Actor)
    {
        for (const FName &Tag : Actor->Tags)
        {
            TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
    }
    Obj->SetArrayField(TEXT("tags"), TagsArray);
}

static TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Component)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Component->GetName());
    Obj->SetStringField(TEXT("path"), Component->GetPathName());
    Obj->SetStringField(TEXT("class"), Component->GetClass() ? Component->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Component->GetClass() ? Component->GetClass()->GetPathName() : TEXT(""));
    Obj->SetBoolField(TEXT("isActive"), Component->IsActive());

    if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSceneComponent"), true);
        Obj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
        Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(SceneComp->GetComponentTransform()));
        Obj->SetStringField(TEXT("attachParent"), SceneComp->GetAttachParent() ? SceneComp->GetAttachParent()->GetName() : TEXT(""));
    }

    if (UCameraComponent *CameraComp = Cast<UCameraComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isCamera"), true);
        Obj->SetNumberField(TEXT("fieldOfView"), CameraComp->FieldOfView);
        Obj->SetBoolField(TEXT("isActive"), CameraComp->IsActive());
    }

    if (USpringArmComponent *SpringArm = Cast<USpringArmComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSpringArm"), true);
        Obj->SetNumberField(TEXT("targetArmLength"), SpringArm->TargetArmLength);
        Obj->SetBoolField(TEXT("usePawnControlRotation"), SpringArm->bUsePawnControlRotation);
    }

    if (PropertyNames.Num() > 0)
    {
        TSharedPtr<FJsonObject> PropertiesObj = McpHandlerUtils::CreateResultObject();
        for (const FString &PropertyName : PropertyNames)
        {
            if (PropertyName.IsEmpty())
            {
                continue;
            }
            McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(Component, PropertyName);
            if (PropResult.IsValid())
            {
                if (TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(PropResult.Container, PropResult.Property))
                {
                    PropertiesObj->SetField(PropertyName, Value);
                }
            }
        }
        Obj->SetObjectField(TEXT("properties"), PropertiesObj);
    }

    return Obj;
}

static TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Actor)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Actor->GetName());
    Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
    Obj->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
    Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(Actor->GetActorTransform()));
    McpAddActorTags(Obj, Actor);

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    for (UActorComponent *Component : Components)
    {
        if (!Component)
        {
            continue;
        }

        const bool bRequestedByName = ComponentNames.Num() == 0 || ComponentNames.ContainsByPredicate([Component](const FString &RequestedName) {
            return Component->GetName().Equals(RequestedName, ESearchCase::IgnoreCase);
        });
        const bool bAlwaysReportCameraState = Component->IsA<UCameraComponent>() || Component->IsA<USpringArmComponent>();
        if (bRequestedByName || bAlwaysReportCameraState)
        {
            ComponentsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeComponent(Component, PropertyNames)));
        }
    }
    Obj->SetArrayField(TEXT("components"), ComponentsArray);
    Obj->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    return Obj;
}
#endif

// =============================================================================
// Section 1: Build Environment Actions
// =============================================================================

/**
 * HandleBuildEnvironmentAction
 * ----------------------------
 * Main dispatcher for environment building actions.
 *
 * Payload:
 *   - action: string (required) - Sub-action to execute
 *   - Other params vary by sub-action
 *
 * Supported Sub-actions:
 *   - add_foliage_instances: Dispatch to HandlePaintFoliage
 *   - get_foliage_instances: Dispatch to HandleGetFoliageInstances
 *   - remove_foliage: Dispatch to HandleRemoveFoliage
 *   - paint_foliage: Dispatch to HandlePaintFoliage
 *   - create_procedural_foliage: Dispatch to HandleCreateProceduralFoliage
 *   - create_procedural_terrain: Dispatch to HandleCreateProceduralTerrain
 *   - add_foliage_type/add_foliage: Dispatch to HandleAddFoliageType
 *   - create_landscape: Dispatch to HandleCreateLandscape
 *   - paint_landscape/paint_landscape_layer: Dispatch to HandlePaintLandscapeLayer
 *   - sculpt_landscape/sculpt: Dispatch to HandleSculptLandscape
 *   - modify_heightmap: Dispatch to HandleModifyHeightmap
 *   - set_landscape_material: Dispatch to HandleSetLandscapeMaterial
 *   - create_landscape_grass_type: Dispatch to HandleCreateLandscapeGrassType
 *   - generate_lods: Dispatch to HandleGenerateLODs
 *   - bake_lightmap: Dispatch to HandleBakeLightmap
 *   - export_snapshot: Export environment snapshot to JSON file
 *   - import_snapshot: Import environment snapshot from JSON file
 *   - delete: Delete environment actors by name
 *   - create_sky_sphere: Create sky sphere actor
 *   - set_time_of_day: Set time of day on sky sphere
 *   - create_fog_volume: Create exponential height fog
 */
bool UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("build_environment"), ESearchCase::IgnoreCase) &&
        !Lower.StartsWith(TEXT("build_environment")))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("build_environment payload missing."),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract sub-action
    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    UE_LOG(LogMcpEnvironmentHandlers, Verbose,
           TEXT("HandleBuildEnvironmentAction: SubAction=%s"), *LowerSub);

    // =========================================================================
    // Foliage Sub-actions (dispatch to dedicated handlers)
    // =========================================================================
    if (LowerSub == TEXT("add_foliage_instances"))
    {
        FString FoliageTypePath;
        if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath) ||
            FoliageTypePath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        }

        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }

        // Preserve full transform data so callers can specify rotation and scale.
        const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
        Payload->TryGetArrayField(TEXT("transforms"), Transforms);
        if (Transforms)
        {
            FoliagePayload->SetArrayField(TEXT("transforms"), *Transforms);
        }

        const TArray<TSharedPtr<FJsonValue>> *Locations = nullptr;
        if (Payload->TryGetArrayField(TEXT("locations"), Locations) && Locations)
        {
            FoliagePayload->SetArrayField(TEXT("locations"), *Locations);
        }

        return HandleAddFoliageInstances(RequestId, TEXT("add_foliage_instances"),
                                         FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("get_foliage_instances"))
    {
        FString FoliageTypePath;
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"),
                                         FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("remove_foliage") || LowerSub == TEXT("remove_foliage_instances"))
    {
        FString FoliageTypePath = McpGetFirstStringField(Payload, {TEXT("foliageTypePath"), TEXT("foliageType")});
        bool bRemoveAll = false;
        Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);

        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        if (LowerSub == TEXT("remove_foliage_instances") && FoliageTypePath.IsEmpty() && !bRemoveAll)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                                   TEXT("remove_foliage_instances requires foliageTypePath/foliageType or removeAll=true"),
                                   FoliagePayload, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"),
                                   FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("paint_foliage") || LowerSub == TEXT("paint_foliage_instances"))
    {
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), Payload,
                                  RequestingSocket);
    }
    else if (LowerSub == TEXT("create_procedural_foliage"))
    {
        return HandleCreateProceduralFoliage(RequestId,
                                             TEXT("create_procedural_foliage"),
                                             Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_procedural_terrain"))
    {
        return HandleCreateProceduralTerrain(RequestId,
                                             TEXT("create_procedural_terrain"),
                                             Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("add_foliage_type") || LowerSub == TEXT("add_foliage") || LowerSub == TEXT("create_foliage_type"))
    {
        return HandleAddFoliageType(RequestId, TEXT("add_foliage_type"),
                                    Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_landscape"))
    {
        return HandleCreateLandscape(RequestId, TEXT("create_landscape"),
                                     Payload, RequestingSocket);
    }

    // =========================================================================
    // Landscape Operations (dispatch to dedicated handlers)
    // =========================================================================
    else if (LowerSub == TEXT("paint_landscape") ||
             LowerSub == TEXT("paint_landscape_layer"))
    {
        return HandlePaintLandscapeLayer(RequestId, TEXT("paint_landscape_layer"),
                                         Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("sculpt_landscape") || LowerSub == TEXT("sculpt"))
    {
        return HandleSculptLandscape(RequestId, TEXT("sculpt_landscape"), Payload,
                                     RequestingSocket);
    }
    else if (LowerSub == TEXT("modify_heightmap"))
    {
        return HandleModifyHeightmap(RequestId, TEXT("modify_heightmap"), Payload,
                                     RequestingSocket);
    }
    else if (LowerSub == TEXT("set_landscape_material") || LowerSub == TEXT("configure_landscape_material"))
    {
        return HandleSetLandscapeMaterial(RequestId, TEXT("set_landscape_material"),
                                          Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_landscape_grass_type"))
    {
        return HandleCreateLandscapeGrassType(RequestId,
                                              TEXT("create_landscape_grass_type"),
                                              Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("generate_lods"))
    {
        return HandleGenerateLODs(RequestId, TEXT("generate_lods"), Payload,
                                  RequestingSocket);
    }
    else if (LowerSub == TEXT("bake_lightmap"))
    {
        return HandleBakeLightmap(RequestId, TEXT("bake_lightmap"), Payload,
                                  RequestingSocket);
    }

#if WITH_EDITOR
    // =========================================================================
    // Editor-Only Environment Actions
    // =========================================================================
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), LowerSub);
    bool bSuccess = true;
    FString Message = FString::Printf(TEXT("Environment action '%s' completed"), *LowerSub);
    FString ErrorCode;

    // -------------------------------------------------------------------------
    // export_snapshot: Export environment snapshot to JSON file
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("export_snapshot"))
    {
        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for export_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            // SECURITY: Validate file path to prevent directory traversal
            FString SafePath = SanitizeProjectFilePath(Path);
            if (SafePath.IsEmpty())
            {
                bSuccess = false;
                Message = FString::Printf(
                    TEXT("Invalid or unsafe path: %s. Path must be relative to project (e.g., /Temp/snapshot.json)"),
                    *Path);
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                // Convert project-relative path to absolute file path
                FString AbsolutePath = FPaths::ProjectDir() / SafePath;
                FPaths::MakeStandardFilename(AbsolutePath);

                // CRITICAL: Convert to absolute path for proper comparison
                // This prevents path traversal via leading slash (e.g., /etc/passwd)
                AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
                FPaths::NormalizeFilename(AbsolutePath);

                FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
                FPaths::NormalizeDirectoryName(NormalizedProjectDir);
                if (!NormalizedProjectDir.EndsWith(TEXT("/")))
                {
                    NormalizedProjectDir += TEXT("/");
                }

                if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
                    ErrorCode = TEXT("SECURITY_VIOLATION");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else if (!McpValidateProjectSnapshotFilePath(AbsolutePath, Message))
                {
                    bSuccess = false;
                    ErrorCode = TEXT("SECURITY_VIOLATION");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    TSharedPtr<FJsonObject> Snapshot = McpHandlerUtils::CreateResultObject();
                    Snapshot->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToString());
                    Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

                    FString JsonString;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                    if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
                    {
                        if (FFileHelper::SaveStringToFile(JsonString, *AbsolutePath))
                        {
                            Resp->SetStringField(TEXT("exportPath"), SafePath);
                            Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
                        }
                        else
                        {
                            bSuccess = false;
                            Message = TEXT("Failed to write snapshot file");
                            ErrorCode = TEXT("WRITE_FAILED");
                            Resp->SetStringField(TEXT("error"), Message);
                        }
                    }
                    else
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to serialize snapshot");
                        ErrorCode = TEXT("SERIALIZE_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // import_snapshot: Import environment snapshot from JSON file
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("import_snapshot"))
    {
        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for import_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            // SECURITY: Validate file path to prevent directory traversal
            FString SafePath = SanitizeProjectFilePath(Path);
            if (SafePath.IsEmpty())
            {
                bSuccess = false;
                Message = FString::Printf(
                    TEXT("Invalid or unsafe path: %s. Path must be relative to project (e.g., /Temp/snapshot.json)"),
                    *Path);
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                FString AbsolutePath = FPaths::ProjectDir() / SafePath;
                FPaths::MakeStandardFilename(AbsolutePath);

                // CRITICAL: Convert to absolute path for proper comparison
                // This prevents path traversal via leading slash (e.g., /etc/passwd)
                AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
                FPaths::NormalizeFilename(AbsolutePath);

                FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
                FPaths::NormalizeDirectoryName(NormalizedProjectDir);
                if (!NormalizedProjectDir.EndsWith(TEXT("/")))
                {
                    NormalizedProjectDir += TEXT("/");
                }

                if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
                {
                    bSuccess = false;
                    Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
                    ErrorCode = TEXT("SECURITY_VIOLATION");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else if (!McpValidateProjectSnapshotFilePath(AbsolutePath, Message))
                {
                    bSuccess = false;
                    ErrorCode = TEXT("SECURITY_VIOLATION");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    FString JsonString;
                    if (!FFileHelper::LoadFileToString(JsonString, *AbsolutePath))
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to read snapshot file");
                        ErrorCode = TEXT("LOAD_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        TSharedPtr<FJsonObject> SnapshotObj;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                        if (!FJsonSerializer::Deserialize(Reader, SnapshotObj) || !SnapshotObj.IsValid())
                        {
                            bSuccess = false;
                            Message = TEXT("Failed to parse snapshot");
                            ErrorCode = TEXT("PARSE_FAILED");
                            Resp->SetStringField(TEXT("error"), Message);
                        }
                        else
                        {
                            Resp->SetObjectField(TEXT("snapshot"), SnapshotObj.ToSharedRef());
                            Resp->SetStringField(TEXT("message"), TEXT("Snapshot imported"));
                        }
                    }
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // delete: Delete environment actors by name
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("delete"))
    {
        const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray)
        {
            bSuccess = false;
            Message = TEXT("names array required for delete");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!GEditor)
        {
            bSuccess = false;
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (!ActorSS)
            {
                bSuccess = false;
                Message = TEXT("EditorActorSubsystem not available");
                ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TArray<FString> Deleted;
                TArray<FString> Missing;

                for (const TSharedPtr<FJsonValue> &Val : *NamesArray)
                {
                    if (Val.IsValid() && Val->Type == EJson::String)
                    {
                        FString Name = Val->AsString();
                        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
                        bool bRemoved = false;

                        for (AActor *A : AllActors)
                        {
                            if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
                            {
                                if (ActorSS->DestroyActor(A))
                                {
                                    Deleted.Add(Name);
                                    bRemoved = true;
                                }
                                break;
                            }
                        }

                        if (!bRemoved)
                        {
                            Missing.Add(Name);
                        }
                    }
                }

                // Build response arrays
                TArray<TSharedPtr<FJsonValue>> DeletedArray;
                for (const FString &Name : Deleted)
                {
                    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("deleted"), DeletedArray);
                Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

                if (Missing.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> MissingArray;
                    for (const FString &Name : Missing)
                    {
                        MissingArray.Add(MakeShared<FJsonValueString>(Name));
                    }
                    Resp->SetArrayField(TEXT("missing"), MissingArray);
                    bSuccess = false;
                    Message = TEXT("Some environment actors could not be removed");
                    ErrorCode = TEXT("DELETE_PARTIAL");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    Message = TEXT("Environment actors deleted");
                }
            }
        }
    }
    else if (LowerSub == TEXT("import_heightmap"))
    {
        bSuccess = McpImportLandscapeHeightmap(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("export_heightmap"))
    {
        bSuccess = McpExportLandscapeHeightmap(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_landscape_layer_info"))
    {
        bSuccess = McpCreateLandscapeLayerInfo(Payload, Resp, Message, ErrorCode);
    }
    // -------------------------------------------------------------------------
    // configure_landscape_splines / configure_landscape_lod / streaming proxy
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("configure_landscape_splines"))
    {
        bSuccess = McpConfigureLandscapeSplines(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_landscape_lod"))
    {
        bSuccess = false;
        ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
        if (!Landscape)
        {
            Message = TEXT("Landscape not found for LOD configuration");
            ErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        }
        else
        {
            Landscape->Modify();
            McpApplyEnvironmentSettings(Landscape, Payload, Resp);
            Landscape->PostEditChange();
            bSuccess = true;
            Message = TEXT("Landscape LOD configuration updated");
            Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
            McpHandlerUtils::AddVerification(Resp, Landscape);
        }
    }
    else if (LowerSub == TEXT("create_landscape_streaming_proxy"))
    {
        bSuccess = McpCreateLandscapeStreamingProxy(Payload, Resp, Message, ErrorCode);
    }
    // -------------------------------------------------------------------------
    // configure_foliage_*: Update existing foliage type assets
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("configure_foliage_mesh") || LowerSub == TEXT("configure_foliage_placement") ||
             LowerSub == TEXT("configure_foliage_lod") || LowerSub == TEXT("configure_foliage_collision") ||
             LowerSub == TEXT("configure_foliage_culling"))
    {
        bSuccess = false;
        UFoliageType *FoliageType = McpLoadFoliageTypeForEnvironmentAction(Payload);
        if (!FoliageType)
        {
            Message = TEXT("Foliage type not found");
            ErrorCode = TEXT("ASSET_NOT_FOUND");
        }
        else
        {
            FoliageType->Modify();
            if (LowerSub == TEXT("configure_foliage_mesh"))
            {
                FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
                MeshPath = SanitizeProjectRelativePath(MeshPath);
                if (UFoliageType_InstancedStaticMesh *InstancedType = Cast<UFoliageType_InstancedStaticMesh>(FoliageType))
                {
                    if (UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
                    {
                        InstancedType->SetStaticMesh(StaticMesh);
                        Resp->SetStringField(TEXT("meshPath"), MeshPath);
                    }
                }
            }
            double Density = 0.0;
            if (Payload->TryGetNumberField(TEXT("density"), Density))
            {
                FoliageType->Density = static_cast<float>(Density);
            }
            double MinScale = 0.0;
            double MaxScale = 0.0;
            if (Payload->TryGetNumberField(TEXT("minScale"), MinScale) && Payload->TryGetNumberField(TEXT("maxScale"), MaxScale))
            {
                FoliageType->ScaleX = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
                FoliageType->ScaleY = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
                FoliageType->ScaleZ = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
            }
            bool bBoolValue = false;
            if (Payload->TryGetBoolField(TEXT("alignToNormal"), bBoolValue))
            {
                FoliageType->AlignToNormal = bBoolValue;
            }
            if (Payload->TryGetBoolField(TEXT("randomYaw"), bBoolValue))
            {
                FoliageType->RandomYaw = bBoolValue;
            }
            int32 CullDistance = 0;
            if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance) && CullDistance >= 0)
            {
                FoliageType->CullDistance.Min = 0;
                FoliageType->CullDistance.Max = CullDistance;
            }
            McpApplyEnvironmentSettings(FoliageType, Payload, Resp);
            TArray<FString> ConfigurationErrors;
            const TArray<TSharedPtr<FJsonValue>> *ConfigurationErrorValues = nullptr;
            if (Resp->TryGetArrayField(TEXT("configurationErrors"), ConfigurationErrorValues) && ConfigurationErrorValues)
            {
                for (const TSharedPtr<FJsonValue> &Value : *ConfigurationErrorValues)
                {
                    if (!Value.IsValid())
                    {
                        continue;
                    }
                    const FString ErrorText = Value->AsString();
                    if (!ErrorText.StartsWith(TEXT("cullDistance:")))
                    {
                        ConfigurationErrors.Add(ErrorText);
                    }
                }
                McpAddStringArrayField(Resp, TEXT("configurationErrors"), ConfigurationErrors);
            }
            FoliageType->MarkPackageDirty();
            McpSafeAssetSave(FoliageType);
            bSuccess = true;
            Message = TEXT("Foliage type configuration updated");
            Resp->SetStringField(TEXT("foliageTypePath"), FoliageType->GetOutermost()->GetName());
            McpHandlerUtils::AddVerification(Resp, FoliageType);
        }
    }
    // -------------------------------------------------------------------------
    // create_sky_sphere: Create sky sphere actor
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("create_sky_sphere"))
    {
        // Initialize to false - only set true on successful creation
        bSuccess = false;

        if (!GEditor)
        {
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        }
        else
        {
            UClass *SkySphereClass = LoadClass<AActor>(
                nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/"
                              "SkySphere.SkySphere_C'"));
            if (!SkySphereClass)
            {
                FString RequestedName = TEXT("SkySphere");
                Payload->TryGetStringField(TEXT("name"), RequestedName);

                ADirectionalLight *SunLight = Cast<ADirectionalLight>(
                    SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                                    FVector::ZeroVector,
                                                    FRotator(-45.0f, -35.0f, 0.0f),
                                                    TEXT("SkySunLight")));
                ASkyLight *SkyLight = Cast<ASkyLight>(
                    SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                                    FVector::ZeroVector,
                                                    FRotator::ZeroRotator,
                                                    TEXT("SkyLight")));

                if (SunLight && SkyLight)
                {
                    SunLight->SetActorLabel(FString::Printf(TEXT("%s_Sun"), *RequestedName));
                    SkyLight->SetActorLabel(FString::Printf(TEXT("%s_SkyLight"), *RequestedName));

                    if (UDirectionalLightComponent *SunComp =
                            Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
                    {
                        SunComp->SetIntensity(10.0f);
                        SunComp->MarkRenderStateDirty();
                    }
                    if (USkyLightComponent *SkyComp = SkyLight->GetLightComponent())
                    {
                        SkyComp->SetIntensity(1.0f);
                        SkyComp->MarkRenderStateDirty();
                    }

                    bSuccess = true;
                    Message = TEXT("Native sky lighting rig created");
                    Resp->SetBoolField(TEXT("fallbackUsed"), true);
                    Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
                    Resp->SetStringField(TEXT("actorName"), RequestedName);
                    Resp->SetStringField(TEXT("sunActorName"), SunLight->GetActorLabel());
                    Resp->SetStringField(TEXT("skyLightActorName"), SkyLight->GetActorLabel());
                    McpHandlerUtils::AddVerification(Resp, SunLight);
                }
                else
                {
                    Message = TEXT("SkySphere class not found and native sky rig fallback failed");
                    ErrorCode = TEXT("SPAWN_FAILED");
                    Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
                }
            }
            else
            {
                AActor *SkySphere = SpawnActorInActiveWorld<AActor>(
                    SkySphereClass, FVector::ZeroVector, FRotator::ZeroRotator,
                    TEXT("SkySphere"));
                if (SkySphere)
                {
                    bSuccess = true;
                    Message = TEXT("Sky sphere created");
                    Resp->SetStringField(TEXT("actorName"), SkySphere->GetActorLabel());
                }
                else
                {
                    Message = TEXT("Failed to spawn sky sphere actor");
                    ErrorCode = TEXT("SPAWN_FAILED");
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // set_time_of_day: Set time of day on sky sphere
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("set_time_of_day"))
    {
        float TimeOfDay = 12.0f;
        if (!Payload->TryGetNumberField(TEXT("time"), TimeOfDay))
        {
            Payload->TryGetNumberField(TEXT("hour"), TimeOfDay);
        }

        if (GEditor)
        {
            UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (ActorSS)
            {
                for (AActor *Actor : ActorSS->GetAllLevelActors())
                {
                    if (Actor->GetClass()->GetName().Contains(TEXT("SkySphere")))
                    {
                        UFunction *SetTimeFunction = Actor->FindFunction(TEXT("SetTimeOfDay"));
                        if (SetTimeFunction)
                        {
                            float TimeParam = TimeOfDay;
                            Actor->ProcessEvent(SetTimeFunction, &TimeParam);
                            bSuccess = true;
                            Message = FString::Printf(TEXT("Time of day set to %.2f"), TimeOfDay);
                            break;
                        }
                    }
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Sky sphere not found or time function not available");
            ErrorCode = TEXT("SET_TIME_FAILED");
        }
    }
    // -------------------------------------------------------------------------
    // create_fog_volume: Create exponential height fog
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("create_fog_volume"))
    {
        // Initialize to false - only set true on successful creation
        bSuccess = false;

        FVector Location(0, 0, 0);
        // Support both top-level x/y/z and location object
        const TSharedPtr<FJsonObject> *LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
        {
            (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
            (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
        else
        {
            Payload->TryGetNumberField(TEXT("x"), Location.X);
            Payload->TryGetNumberField(TEXT("y"), Location.Y);
            Payload->TryGetNumberField(TEXT("z"), Location.Z);
        }

        if (!GEditor)
        {
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        }
        else
        {
            UClass *FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
            if (!FogClass)
            {
                Message = TEXT("ExponentialHeightFog class not found");
                ErrorCode = TEXT("CLASS_NOT_FOUND");
            }
            else
            {
                AActor *FogVolume = SpawnActorInActiveWorld<AActor>(
                    FogClass, Location, FRotator::ZeroRotator, TEXT("FogVolume"));
                if (FogVolume)
                {
                    bSuccess = true;
                    Message = TEXT("Fog volume created");
                    Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
                }
                else
                {
                    Message = TEXT("Failed to spawn fog volume actor");
                    ErrorCode = TEXT("SPAWN_FAILED");
                }
            }
        }
    }
    else if (LowerSub == TEXT("configure_sky_atmosphere"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.SkyAtmosphere"),
                                                 TEXT("SkyAtmosphere"), TEXT("/Script/Engine.SkyAtmosphereComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sky_light"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.SkyLight"),
                                                 TEXT("SkyLight"), TEXT("/Script/Engine.SkyLightComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_directional_light_atmosphere"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.DirectionalLight"),
                                                 TEXT("DirectionalLight"), TEXT("/Script/Engine.DirectionalLightComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_exponential_height_fog"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.ExponentialHeightFog"),
                                                 TEXT("ExponentialHeightFog"), TEXT("/Script/Engine.ExponentialHeightFogComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_volumetric_cloud"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.VolumetricCloud"),
                                                 TEXT("VolumetricCloud"), TEXT("/Script/Engine.VolumetricCloudComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_weather_system"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("WeatherSystem"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_rain_particles"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("RainParticles"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_snow_particles"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("SnowParticles"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_wind"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.WindDirectionalSource"),
                                                 TEXT("WindDirectionalSource"), TEXT("/Script/Engine.WindDirectionalSourceComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_lightning"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("LightningSystem"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_time_of_day_system"))
    {
        bSuccess = McpCreateTimeOfDaySystem(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sun_position"))
    {
        bSuccess = McpConfigureSunPosition(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_light_color_curve"))
    {
        bSuccess = McpCreateLinearColorCurve(Payload, TEXT("LightColorCurve"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sky_color_curve"))
    {
        bSuccess = McpCreateLinearColorCurve(Payload, TEXT("SkyColorCurve"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_water_body_ocean") || LowerSub == TEXT("create_water_body_lake") ||
             LowerSub == TEXT("create_water_body_river") || LowerSub == TEXT("create_water_body_custom"))
    {
        const FString WaterClassPath = LowerSub == TEXT("create_water_body_ocean") ? TEXT("/Script/Water.WaterBodyOcean") :
            LowerSub == TEXT("create_water_body_lake") ? TEXT("/Script/Water.WaterBodyLake") :
            LowerSub == TEXT("create_water_body_river") ? TEXT("/Script/Water.WaterBodyRiver") :
            TEXT("/Script/Water.WaterBodyCustom");
        UClass *WaterClass = LoadClass<AActor>(nullptr, *WaterClassPath);
        AActor *WaterActor = McpFindOrSpawnEnvironmentActor(Payload, WaterClass, LowerSub);
        if (!WaterActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Water body class unavailable or spawn failed: %s"), *WaterClassPath);
            ErrorCode = WaterClass ? TEXT("SPAWN_FAILED") : TEXT("CLASS_NOT_FOUND");
            Resp->SetStringField(TEXT("classPath"), WaterClassPath);
        }
        else
        {
            McpApplyEnvironmentSettings(WaterActor, Payload, Resp);
            McpSetMaterialOnActor(WaterActor, Payload, Resp);
            McpSetCollisionOnActor(WaterActor, Payload, Resp);
            bSuccess = true;
            Message = TEXT("Water body created");
            Resp->SetStringField(TEXT("waterBodyName"), WaterActor->GetActorLabel());
            Resp->SetStringField(TEXT("actorPath"), WaterActor->GetPathName());
            Resp->SetStringField(TEXT("classPath"), WaterClassPath);
            McpHandlerUtils::AddVerification(Resp, WaterActor);
        }
    }
    else if (LowerSub == TEXT("configure_water_waves"))
    {
        AActor *WaterActor = McpFindWaterBodyActor(Payload);
        bSuccess = McpConfigureWaterBodyActor(Payload, Resp, Message, ErrorCode);
        if (bSuccess && McpPayloadHasWaterWaveSettings(Payload))
        {
            bSuccess = McpConfigureWaterWavesOnActor(WaterActor, Payload, Resp, Message, ErrorCode);
        }
    }
    else if (LowerSub == TEXT("configure_water_material") || LowerSub == TEXT("configure_water_collision"))
    {
        bSuccess = McpConfigureWaterBodyActor(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_buoyancy_component"))
    {
        bSuccess = McpCreateBuoyancyComponent(Payload, Resp, Message, ErrorCode);
    }
    else
    {
        bSuccess = false;
        Message = FString::Printf(TEXT("Environment action '%s' not implemented"), *LowerSub);
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;

#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Section 2: Control Environment Actions
// =============================================================================

/**
 * HandleControlEnvironmentAction
 * -------------------------------
 * Handle environment control actions (time, lighting, etc.)
 *
 * Payload:
 *   - action: string (required) - Sub-action to execute
 *   - hour: number (optional) - For set_time_of_day
 *   - intensity: number (optional) - For set_sun_intensity/set_skylight_intensity
 *
 * Response:
 *   - success: bool
 *   - hour/intensity: number (depending on action)
 *   - actor: string - Affected actor path
 *   - pitch: number (for set_time_of_day)
 */
bool UMcpAutomationBridgeSubsystem::HandleControlEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_environment"), ESearchCase::IgnoreCase) &&
        !Lower.StartsWith(TEXT("control_environment")))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("control_environment payload missing."),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    // -------------------------------------------------------------------------
    // Helper lambda for sending results
    // -------------------------------------------------------------------------
    auto SendResult = [&](bool bSuccess, const TCHAR *Message,
                          const FString &ErrorCode,
                          const TSharedPtr<FJsonObject> &Result)
    {
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   Message ? Message : TEXT("Environment control succeeded."),
                                   Result, FString());
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                                   Message ? Message : TEXT("Environment control failed."),
                                   Result, ErrorCode);
        }
    };

    // Get editor world
    UWorld *World = nullptr;
    if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World)
    {
        SendResult(false, TEXT("Editor world is unavailable"),
                   TEXT("WORLD_NOT_AVAILABLE"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // Helper lambdas for finding lights
    // -------------------------------------------------------------------------
    auto FindFirstDirectionalLight = [&]() -> ADirectionalLight *
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            if (ADirectionalLight *Light = *It)
            {
                if (IsValid(Light))
                {
                    return Light;
                }
            }
        }
        return nullptr;
    };

    auto FindFirstSkyLight = [&]() -> ASkyLight *
    {
        for (TActorIterator<ASkyLight> It(World); It; ++It)
        {
            if (ASkyLight *Sky = *It)
            {
                if (IsValid(Sky))
                {
                    return Sky;
                }
            }
        }
        return nullptr;
    };

    // -------------------------------------------------------------------------
    // set_time_of_day: Adjust sun rotation based on hour
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_time_of_day"))
    {
        double Hour = 0.0;
        const bool bHasHour = Payload->TryGetNumberField(TEXT("hour"), Hour);
        if (!bHasHour)
        {
            SendResult(false, TEXT("Missing hour parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight *SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"),
                       TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        const float ClampedHour = FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);
        const float SolarPitch = (ClampedHour / 24.0f) * 360.0f - 90.0f;

        SunLight->Modify();
        FRotator NewRotation = SunLight->GetActorRotation();
        NewRotation.Pitch = SolarPitch;
        SunLight->SetActorRotation(NewRotation);

        if (UDirectionalLightComponent *LightComp =
                Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("hour"), ClampedHour);
        Result->SetNumberField(TEXT("pitch"), SolarPitch);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());

        // Add verification data
        McpHandlerUtils::AddVerification(Result, SunLight);

        SendResult(true, TEXT("Time of day updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_sun_intensity: Set directional light intensity
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_sun_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight *SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"),
                       TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        if (UDirectionalLightComponent *LightComp =
                Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->SetIntensity(static_cast<float>(Intensity));
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
        SendResult(true, TEXT("Sun intensity updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_skylight_intensity: Set sky light intensity
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_skylight_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ASkyLight *SkyActor = FindFirstSkyLight();
        if (!SkyActor)
        {
            SendResult(false, TEXT("No skylight found"), TEXT("SKYLIGHT_NOT_FOUND"),
                       nullptr);
            return true;
        }

        if (USkyLightComponent *SkyComp = SkyActor->GetLightComponent())
        {
            SkyComp->SetIntensity(static_cast<float>(Intensity));
            SkyComp->MarkRenderStateDirty();
            SkyActor->MarkComponentsRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SkyActor->GetPathName());
        SendResult(true, TEXT("Skylight intensity updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // Unknown action
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), LowerSub);
    SendResult(false, TEXT("Unsupported environment control action"),
               TEXT("UNSUPPORTED_ACTION"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Environment control requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}



// =============================================================================
// Section 4: Environment Utilities
// =============================================================================

/**
 * HandleBakeLightmap
 * -------------------
 * Build lighting via editor function.
 *
 * Payload:
 *   - quality: string (optional) - Lighting build quality (default: "Preview")
 *
 * Dispatches to HandleExecuteEditorFunction with BUILD_LIGHTING.
 */
bool UMcpAutomationBridgeSubsystem::HandleBakeLightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("bake_lightmap"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    FString QualityStr = TEXT("Preview");
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("quality"), QualityStr);
    }

    // Reuse HandleExecuteEditorFunction logic
    TSharedPtr<FJsonObject> P = McpHandlerUtils::CreateResultObject();
    P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
    P->SetStringField(TEXT("quality"), QualityStr);

    return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"),
                                       P, RequestingSocket);

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Requires editor"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

/**
 * HandleCreateProceduralTerrain
 * -------------------------------
 * Create a procedural terrain mesh with configurable parameters.
 *
 * Payload:
 *   - sizeX: int (optional, default 100) - Terrain width in grid units
 *   - sizeY: int (optional, default 100) - Terrain depth in grid units
 *   - spacing: float (optional, default 100.0) - Distance between vertices
 *   - heightScale: float (optional, default 500.0) - Maximum height variation
 *   - subdivisions: int (optional, default 50) - Grid subdivisions
 *   - actorName: string (required) - Name for the spawned actor
 *   - location: {x, y, z} (optional) - Spawn location
 *   - rotation: {pitch, yaw, roll} (optional) - Spawn rotation
 *   - material: string (optional) - Material asset path
 *
 * Response:
 *   - success: bool
 *   - actorName: string - Spawned actor name
 *   - actorPath: string - Actor path in level
 *   - vertices: int - Number of vertices generated
 *   - triangles: int - Number of triangles generated
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralTerrain(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_procedural_terrain"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Editor not available"),
                            TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_procedural_terrain payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Extract terrain parameters
    // -------------------------------------------------------------------------
    int32 SizeX = 100;
    int32 SizeY = 100;
    double Spacing = 100.0;
    double HeightScale = 500.0;
    int32 Subdivisions = 50;
    FString ActorName = TEXT("ProceduralTerrain");

    Payload->TryGetNumberField(TEXT("sizeX"), SizeX);
    Payload->TryGetNumberField(TEXT("sizeY"), SizeY);
    Payload->TryGetNumberField(TEXT("spacing"), Spacing);
    Payload->TryGetNumberField(TEXT("heightScale"), HeightScale);
    Payload->TryGetNumberField(TEXT("subdivisions"), Subdivisions);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    // -------------------------------------------------------------------------
    // Validate actorName
    // -------------------------------------------------------------------------
    if (ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName parameter is required for create_procedural_terrain"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Reject invalid characters
    if (ActorName.Contains(TEXT("/")) || ActorName.Contains(TEXT("\\")) ||
        ActorName.Contains(TEXT(":")) || ActorName.Contains(TEXT("*")) ||
        ActorName.Contains(TEXT("?")) || ActorName.Contains(TEXT("\"")) ||
        ActorName.Contains(TEXT("<")) || ActorName.Contains(TEXT(">")) ||
        ActorName.Contains(TEXT("|")))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName contains invalid characters (/, \\, :, *, ?, \", <, >, |)"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Reject excessive length
    if (ActorName.Len() > 128)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName exceeds maximum length of 128 characters"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Clamp values to reasonable limits
    // -------------------------------------------------------------------------
    SizeX = FMath::Clamp(SizeX, 2, 1000);
    SizeY = FMath::Clamp(SizeY, 2, 1000);
    Subdivisions = FMath::Clamp(Subdivisions, 2, 200);
    Spacing = FMath::Max(Spacing, 1.0);
    HeightScale = FMath::Max(HeightScale, 0.0);

    // -------------------------------------------------------------------------
    // Get world and spawn actor
    // -------------------------------------------------------------------------
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("World not available"),
                            TEXT("WORLD_NOT_AVAILABLE"));
        return true;
    }

    // Extract location/rotation
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        Location = FVector(X, Y, Z);
    }

    FRotator Rotation(0, 0, 0);
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
    {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        Rotation = FRotator(Pitch, Yaw, Roll);
    }

    // Spawn actor with requested name
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*ActorName);
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    AActor *TerrainActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!TerrainActor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to spawn terrain actor"),
                            TEXT("SPAWN_FAILED"));
        return true;
    }

    TerrainActor->Modify();
    TerrainActor->SetActorLabel(*ActorName);

    // -------------------------------------------------------------------------
    // Add procedural mesh component
    // -------------------------------------------------------------------------
    UProceduralMeshComponent *ProcMesh = NewObject<UProceduralMeshComponent>(TerrainActor);
    if (!ProcMesh)
    {
        TerrainActor->Destroy();
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create procedural mesh component"),
                            TEXT("COMPONENT_CREATION_FAILED"));
        return true;
    }

    ProcMesh->CreationMethod = EComponentCreationMethod::Instance;
    ProcMesh->SetMobility(EComponentMobility::Movable);
    ProcMesh->SetRelativeTransform(FTransform::Identity);
    TerrainActor->SetRootComponent(ProcMesh);
    TerrainActor->AddInstanceComponent(ProcMesh);
    ProcMesh->RegisterComponent();
    TerrainActor->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);

    // -------------------------------------------------------------------------
    // Generate terrain mesh
    // -------------------------------------------------------------------------
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    // Create grid of vertices
    for (int32 Y = 0; Y <= Subdivisions; ++Y)
    {
        for (int32 X = 0; X <= Subdivisions; ++X)
        {
            // Calculate normalized position (0 to 1)
            double NormX = static_cast<double>(X) / Subdivisions;
            double NormY = static_cast<double>(Y) / Subdivisions;

            // Calculate world position with spacing
            double WorldX = (NormX - 0.5) * SizeX * Spacing;
            double WorldY = (NormY - 0.5) * SizeY * Spacing;

            // Generate height using simple noise/sine combination
            double WorldZ = FMath::Sin(NormX * 4.0 * PI) * FMath::Cos(NormY * 4.0 * PI) * HeightScale * 0.3 +
                            FMath::Sin(NormX * 8.0 * PI) * FMath::Cos(NormY * 8.0 * PI) * HeightScale * 0.15 +
                            FMath::Sin(NormX * 2.0 * PI + NormY * 3.0 * PI) * HeightScale * 0.25;

            Vertices.Add(FVector(WorldX, WorldY, WorldZ));
            UVs.Add(FVector2D(NormX, NormY));
        }
    }

    // Generate triangles
    for (int32 Y = 0; Y < Subdivisions; ++Y)
    {
        for (int32 X = 0; X < Subdivisions; ++X)
        {
            int32 Current = Y * (Subdivisions + 1) + X;
            int32 Next = Current + Subdivisions + 1;

            // First triangle
            Triangles.Add(Current);
            Triangles.Add(Next);
            Triangles.Add(Current + 1);

            // Second triangle
            Triangles.Add(Current + 1);
            Triangles.Add(Next);
            Triangles.Add(Next + 1);
        }
    }

    // Calculate normals and tangents
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

    // Create the mesh section
    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);

    // -------------------------------------------------------------------------
    // Apply material if specified
    // -------------------------------------------------------------------------
    FString MaterialPath;
    if (Payload->TryGetStringField(TEXT("material"), MaterialPath) && !MaterialPath.IsEmpty())
    {
        UMaterialInterface *Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            ProcMesh->SetMaterial(0, Material);
        }
    }

    // Mark the actor as modified
    TerrainActor->MarkPackageDirty();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());
    Resp->SetNumberField(TEXT("vertices"), Vertices.Num());
    const int32 TriangleCount = Triangles.Num() / 3;
    Resp->SetNumberField(TEXT("triangles"), TriangleCount);
    Resp->SetNumberField(TEXT("sizeX"), SizeX);
    Resp->SetNumberField(TEXT("sizeY"), SizeY);
    Resp->SetNumberField(TEXT("subdivisions"), Subdivisions);

    // Add verification data
    McpHandlerUtils::AddVerification(Resp, TerrainActor);
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Procedural terrain created successfully"), Resp, FString());
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("create_procedural_terrain requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

/**
 * HandleInspectAction
 * --------------------
 * Object introspection and inspection handler.
 *
 * Supports both global actions (no objectPath required) and object-specific actions.
 *
 * Global Actions (no objectPath required):
 *   - get_project_settings: Retrieve project settings
 *   - get_editor_settings: Retrieve editor settings
 *   - get_world_settings: Retrieve current world settings
 *   - get_viewport_info: Get active viewport dimensions
 *   - get_selected_actors: List currently selected actors
 *   - get_scene_stats: Get scene statistics (actor count)
  *   - get_performance_stats: Live performance metrics
  *   - get_memory_stats: Live memory metrics
 *   - list_objects: List all actors in current world
 *   - find_by_class: Find actors by class name
 *   - find_by_tag: Find actors by tag
 *   - inspect_class: Inspect a class by name
 *
 * Actor Actions (delegated to HandleControlActorAction):
 *   - get_components, get_component_property, set_component_property
 *   - get_metadata, add_tag, create_snapshot, restore_snapshot
 *   - export, delete_object, get_bounding_box, set_property, get_property
 *
 * Payload:
 *   - action: string (required) - Sub-action to execute
 *   - objectPath: string (required for non-global actions) - Object to inspect
 *   - className: string (for find_by_class, inspect_class)
 *   - tag: string (for find_by_tag)
 */
bool UMcpAutomationBridgeSubsystem::HandleInspectAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("inspect"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Extract sub-action
    // -------------------------------------------------------------------------
    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSubAction = SubAction.ToLower();

    // -------------------------------------------------------------------------
    // Classify action types
    // -------------------------------------------------------------------------
    // Global actions that don't require objectPath
    const bool bIsGlobalAction =
        LowerSubAction.Equals(TEXT("get_project_settings")) ||
        LowerSubAction.Equals(TEXT("get_editor_settings")) ||
        LowerSubAction.Equals(TEXT("get_world_settings")) ||
        LowerSubAction.Equals(TEXT("get_viewport_info")) ||
        LowerSubAction.Equals(TEXT("get_selected_actors")) ||
        LowerSubAction.Equals(TEXT("get_scene_stats")) ||
        LowerSubAction.Equals(TEXT("get_performance_stats")) ||
        LowerSubAction.Equals(TEXT("get_memory_stats")) ||
        LowerSubAction.Equals(TEXT("list_objects")) ||
        LowerSubAction.Equals(TEXT("find_by_class")) ||
        LowerSubAction.Equals(TEXT("find_by_tag")) ||
        LowerSubAction.Equals(TEXT("inspect_class")) ||
        LowerSubAction.Equals(TEXT("inspect_cdo")) ||
        LowerSubAction.Equals(TEXT("runtime_report")) ||
        LowerSubAction.Equals(TEXT("pie_report"));

    // Actor actions (delegated to HandleControlActorAction)
    const bool bIsActorAction =
        LowerSubAction.Equals(TEXT("get_components")) ||
        LowerSubAction.Equals(TEXT("get_component_property")) ||
        LowerSubAction.Equals(TEXT("set_component_property")) ||
        LowerSubAction.Equals(TEXT("get_metadata")) ||
        LowerSubAction.Equals(TEXT("add_tag")) ||
        LowerSubAction.Equals(TEXT("create_snapshot")) ||
        LowerSubAction.Equals(TEXT("restore_snapshot")) ||
        LowerSubAction.Equals(TEXT("export")) ||
        LowerSubAction.Equals(TEXT("delete_object")) ||
        LowerSubAction.Equals(TEXT("get_bounding_box")) ||
        LowerSubAction.Equals(TEXT("set_property")) ||
        LowerSubAction.Equals(TEXT("get_property"));

    // Delegate actor-related actions to the control_actor handler
    if (bIsActorAction)
    {
        FString ActorAlias;
        Payload->TryGetStringField(TEXT("actorName"), ActorAlias);
        ActorAlias.TrimStartAndEndInline();
        if (ActorAlias.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("name"), ActorAlias);
            ActorAlias.TrimStartAndEndInline();
        }
        if (ActorAlias.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("objectPath"), ActorAlias);
            ActorAlias.TrimStartAndEndInline();
        }
        if (!ActorAlias.IsEmpty())
        {
            Payload->SetStringField(TEXT("actorName"), ActorAlias);
        }

        if (LowerSubAction.Equals(TEXT("get_property")) || LowerSubAction.Equals(TEXT("set_property")))
        {
            FString ObjectPath;
            FString BlueprintPath;
            Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
            Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
            if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty() && !ActorAlias.IsEmpty())
            {
                Payload->SetStringField(TEXT("objectPath"), ActorAlias);
            }
        }
        else if (LowerSubAction.Equals(TEXT("delete_object")))
        {
            Payload->SetStringField(TEXT("action"), TEXT("delete"));
        }

        return HandleControlActorAction(RequestId, TEXT("control_actor"), Payload, RequestingSocket);
    }

    // -------------------------------------------------------------------------
    // Require objectPath for non-global actions
    // -------------------------------------------------------------------------
    FString ObjectPath;
    if (!bIsGlobalAction)
    {
        Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
        if (ObjectPath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
        }
        if (ObjectPath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("name"), ObjectPath);
        }
        if (ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("objectPath, actorName, or name required"),
                                TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    // =========================================================================
    // Handle Global Actions
    // =========================================================================
    if (bIsGlobalAction)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

        // ---------------------------------------------------------------------
        // get_project_settings
        // ---------------------------------------------------------------------
        if (LowerSubAction.Equals(TEXT("get_project_settings")))
        {
            Resp->SetStringField(TEXT("action"), TEXT("inspect"));
            Resp->SetStringField(TEXT("subAction"), SubAction);
            Resp->SetStringField(TEXT("message"), TEXT("Project settings retrieved"));
            Resp->SetStringField(TEXT("projectName"), FApp::GetProjectName());
            Resp->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
            Resp->SetStringField(TEXT("buildConfig"), LexToString(FApp::GetBuildConfiguration()));
            Resp->SetStringField(TEXT("projectDir"), FPaths::ProjectDir());
            if (const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>())
            {
                Resp->SetStringField(TEXT("description"), ProjectSettings->Description);
                Resp->SetStringField(TEXT("homepage"), ProjectSettings->Homepage);
                Resp->SetStringField(TEXT("supportContact"), ProjectSettings->SupportContact);
                Resp->SetStringField(TEXT("projectVersion"), ProjectSettings->ProjectVersion);
                Resp->SetStringField(TEXT("companyName"), ProjectSettings->CompanyName);
                Resp->SetStringField(TEXT("copyrightNotice"), ProjectSettings->CopyrightNotice);
                Resp->SetStringField(TEXT("projectID"), ProjectSettings->ProjectID.ToString());
                Resp->SetBoolField(TEXT("startInVR"), ProjectSettings->bStartInVR);
            }
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Project settings retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_editor_settings
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_editor_settings")))
        {
            Resp->SetStringField(TEXT("action"), TEXT("inspect"));
            Resp->SetStringField(TEXT("subAction"), SubAction);
            Resp->SetStringField(TEXT("message"), TEXT("Editor settings retrieved"));
            if (const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
            {
                Resp->SetNumberField(TEXT("mouseSensitivity"), ViewportSettings->MouseSensitivty);
                Resp->SetNumberField(TEXT("mouseScrollCameraSpeed"), ViewportSettings->MouseScrollCameraSpeed);
                Resp->SetBoolField(TEXT("useDistanceScaledCamera"), ViewportSettings->bUseDistanceScaledCameraSpeed);
            }
            if (GEditor)
            {
                Resp->SetBoolField(TEXT("isSimulating"), GEditor->bIsSimulatingInEditor);
                Resp->SetBoolField(TEXT("isPIEActive"), GEditor->PlayWorld != nullptr);
                Resp->SetNumberField(TEXT("gameAgnosticSavedFPS"), GEngine ? GEngine->GetMaxFPS() : 0.0);
            }
            Resp->SetBoolField(TEXT("isEditor"), GIsEditor);
            Resp->SetNumberField(TEXT("gRunningCommandlet"), IsRunningCommandlet() ? 1 : 0);
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Editor settings retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_world_settings
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_world_settings")))
        {
            UWorld* World = McpGetRuntimeInspectionWorld();

            if (World)
            {
                Resp->SetStringField(TEXT("worldName"), World->GetName());
                if (ULevel* CurrentLevel = World->GetCurrentLevel())
                {
                    Resp->SetStringField(TEXT("levelName"), CurrentLevel->GetName());
                }
                Resp->SetStringField(TEXT("packageName"), World->GetOutermost()->GetName());
                Resp->SetNumberField(TEXT("timeSeconds"), World->GetTimeSeconds());
                Resp->SetNumberField(TEXT("realTimeSeconds"), World->GetRealTimeSeconds());
                Resp->SetNumberField(TEXT("deltaTimeSeconds"), World->GetDeltaSeconds());
                Resp->SetBoolField(TEXT("hasBegunPlay"), World->HasBegunPlay());
                Resp->SetBoolField(TEXT("isPlayInEditor"), World->IsPlayInEditor());
                if (AWorldSettings* WorldSettings = World->GetWorldSettings())
                {
                    Resp->SetNumberField(TEXT("killZ"), WorldSettings->KillZ);
                    Resp->SetNumberField(TEXT("worldGravityZ"), WorldSettings->GetGravityZ());
                    Resp->SetNumberField(TEXT("timeDilation"), WorldSettings->TimeDilation);
                    Resp->SetBoolField(TEXT("enableWorldBoundsChecks"), WorldSettings->bEnableWorldBoundsChecks);
                    if (UClass* GameModeClass = WorldSettings->DefaultGameMode.Get())
                    {
                        Resp->SetStringField(TEXT("defaultGameMode"), GameModeClass->GetPathName());
                    }
                }
                Resp->SetBoolField(TEXT("success"), true);
                SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("World settings retrieved"), Resp, FString());
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("No world available"),
                                    TEXT("WORLD_NOT_FOUND"));
            }
            return true;
        }
        // ---------------------------------------------------------------------
        // get_viewport_info
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_viewport_info")))
        {
            if (GEditor && GEditor->GetActiveViewport())
            {
                FViewport* Viewport = GEditor->GetActiveViewport();
                Resp->SetNumberField(TEXT("width"), Viewport->GetSizeXY().X);
                Resp->SetNumberField(TEXT("height"), Viewport->GetSizeXY().Y);
                Resp->SetBoolField(TEXT("success"), true);
                SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("Viewport info retrieved"), Resp, FString());
            }
            else
            {
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Viewport info not available in this context"));
                SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("Viewport info retrieved"), Resp, FString());
            }
            return true;
        }
        // ---------------------------------------------------------------------
        // get_selected_actors
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_selected_actors")))
        {
            TArray<TSharedPtr<FJsonValue>> ActorsArray;
            if (GEditor)
            {
                TArray<AActor*> SelectedActors;
                GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
                for (AActor* Actor : SelectedActors)
                {
                    if (Actor)
                    {
                        TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
                        ActorObj->SetStringField(TEXT("name"), Actor->GetName());
                        ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
                        ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("actors"), ActorsArray);
            Resp->SetNumberField(TEXT("count"), ActorsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Selected actors retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_scene_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_scene_stats")))
        {
            int32 ActorCount = 0;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    ActorCount++;
                }
            }
            Resp->SetNumberField(TEXT("actorCount"), ActorCount);
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Scene stats retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_performance_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_performance_stats")))
        {
            const double DeltaSeconds = FApp::GetDeltaTime();
            const double FrameTimeMs = DeltaSeconds > 0.0 ? DeltaSeconds * 1000.0 : 0.0;
            const double EstimatedFps = DeltaSeconds > 0.0 ? 1.0 / DeltaSeconds : 0.0;
            const double GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
            const double RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
            const double RHIThreadMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
            const double GPUFrameMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

            int32 ActorCount = 0;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    ActorCount++;
                }
            }

            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetNumberField(TEXT("deltaSeconds"), DeltaSeconds);
            Resp->SetNumberField(TEXT("frameTimeMs"), FrameTimeMs);
            Resp->SetNumberField(TEXT("estimatedFps"), EstimatedFps);
            Resp->SetNumberField(TEXT("fps"), EstimatedFps);
            Resp->SetNumberField(TEXT("gameThreadMs"), GameThreadMs);
            Resp->SetNumberField(TEXT("renderThreadMs"), RenderThreadMs);
            Resp->SetNumberField(TEXT("rhiThreadMs"), RHIThreadMs);
            Resp->SetNumberField(TEXT("gpuMs"), GPUFrameMs);
            Resp->SetNumberField(TEXT("actorCount"), ActorCount);
            Resp->SetBoolField(TEXT("isBenchmarking"), FApp::IsBenchmarking());
            Resp->SetBoolField(TEXT("useFixedTimeStep"), FApp::UseFixedTimeStep());
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Performance stats retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_memory_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_memory_stats")))
        {
            const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
            const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetNumberField(TEXT("totalPhysicalBytes"), static_cast<double>(MemoryStats.TotalPhysical));
            Resp->SetNumberField(TEXT("availablePhysicalBytes"), static_cast<double>(MemoryStats.AvailablePhysical));
            Resp->SetNumberField(TEXT("usedPhysicalBytes"), static_cast<double>(MemoryStats.UsedPhysical));
            Resp->SetNumberField(TEXT("peakUsedPhysicalBytes"), static_cast<double>(MemoryStats.PeakUsedPhysical));
            Resp->SetNumberField(TEXT("totalVirtualBytes"), static_cast<double>(MemoryStats.TotalVirtual));
            Resp->SetNumberField(TEXT("availableVirtualBytes"), static_cast<double>(MemoryStats.AvailableVirtual));
            Resp->SetNumberField(TEXT("usedVirtualBytes"), static_cast<double>(MemoryStats.UsedVirtual));
            Resp->SetNumberField(TEXT("peakUsedVirtualBytes"), static_cast<double>(MemoryStats.PeakUsedVirtual));
            Resp->SetNumberField(TEXT("totalPhysicalMB"), static_cast<double>(MemoryConstants.TotalPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("totalVirtualMB"), static_cast<double>(MemoryConstants.TotalVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("availablePhysicalMB"), static_cast<double>(MemoryStats.AvailablePhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("availableVirtualMB"), static_cast<double>(MemoryStats.AvailableVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("usedPhysicalMB"), static_cast<double>(MemoryStats.UsedPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("usedVirtualMB"), static_cast<double>(MemoryStats.UsedVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("peakUsedPhysicalMB"), static_cast<double>(MemoryStats.PeakUsedPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("peakUsedVirtualMB"), static_cast<double>(MemoryStats.PeakUsedVirtual) / (1024.0 * 1024.0));
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Memory stats retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // runtime_report / pie_report
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("runtime_report")) || LowerSubAction.Equals(TEXT("pie_report")))
        {
            UWorld *World = McpGetRuntimeInspectionWorld();
            if (!World)
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("No editor, PIE, or game world available for runtime inspection"),
                                    TEXT("WORLD_NOT_FOUND"));
                return true;
            }

            FString Filter;
            Payload->TryGetStringField(TEXT("filter"), Filter);
            FString ActorName;
            Payload->TryGetStringField(TEXT("actorName"), ActorName);
            if (ActorName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("name"), ActorName);
            }

            TArray<FString> ComponentNames;
            FString ComponentName;
            if (Payload->TryGetStringField(TEXT("componentName"), ComponentName) && !ComponentName.IsEmpty())
            {
                ComponentNames.Add(ComponentName);
            }
            const TArray<TSharedPtr<FJsonValue>> *ComponentNamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("componentNames"), ComponentNamesArray) && ComponentNamesArray)
            {
                for (const TSharedPtr<FJsonValue> &Value : *ComponentNamesArray)
                {
                    if (Value.IsValid() && Value->Type == EJson::String)
                    {
                        ComponentNames.Add(Value->AsString());
                    }
                }
            }

            TArray<FString> PropertyNames;
            FString PropertyName;
            if (Payload->TryGetStringField(TEXT("propertyName"), PropertyName) && !PropertyName.IsEmpty())
            {
                PropertyNames.Add(PropertyName);
            }
            else if (Payload->TryGetStringField(TEXT("propertyPath"), PropertyName) && !PropertyName.IsEmpty())
            {
                PropertyNames.Add(PropertyName);
            }
            const TArray<TSharedPtr<FJsonValue>> *PropertyNamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNamesArray) && PropertyNamesArray)
            {
                for (const TSharedPtr<FJsonValue> &Value : *PropertyNamesArray)
                {
                    if (Value.IsValid() && Value->Type == EJson::String)
                    {
                        PropertyNames.Add(Value->AsString());
                    }
                }
            }

            TSharedPtr<FJsonObject> Report = McpHandlerUtils::CreateResultObject();
            Report->SetBoolField(TEXT("success"), true);
            Report->SetStringField(TEXT("worldName"), World->GetName());
            Report->SetStringField(TEXT("worldType"), McpGetWorldTypeName(World));
            Report->SetStringField(TEXT("worldPath"), World->GetPathName());
            Report->SetBoolField(TEXT("isPIE"), World->WorldType == EWorldType::PIE);

            TArray<TSharedPtr<FJsonValue>> ActorsArray;
            int32 TotalActorCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor *Actor = *It;
                if (!Actor)
                {
                    continue;
                }
                ++TotalActorCount;

                const FString Label = Actor->GetActorLabel();
                const FString Name = Actor->GetName();
                const bool bMatchesActor = ActorName.IsEmpty() ||
                    Label.Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Name.Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetPathName().Equals(ActorName, ESearchCase::IgnoreCase);
                const bool bMatchesFilter = Filter.IsEmpty() ||
                    Label.Contains(Filter) ||
                    Name.Contains(Filter) ||
                    Actor->GetClass()->GetName().Contains(Filter) ||
                    Actor->GetPathName().Contains(Filter);
                if (bMatchesActor && bMatchesFilter)
                {
                    ActorsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeActor(Actor, ComponentNames, PropertyNames)));
                }
            }
            Report->SetArrayField(TEXT("actors"), ActorsArray);
            Report->SetNumberField(TEXT("count"), ActorsArray.Num());
            Report->SetNumberField(TEXT("totalActorCount"), TotalActorCount);

            APlayerController *PlayerController = World->GetFirstPlayerController();
            if (PlayerController)
            {
                TSharedPtr<FJsonObject> ControllerObj = McpDescribeRuntimeActor(PlayerController, ComponentNames, PropertyNames);
                Report->SetObjectField(TEXT("playerController"), ControllerObj);

                if (APawn *Pawn = PlayerController->GetPawn())
                {
                    Report->SetObjectField(TEXT("pawn"), McpDescribeRuntimeActor(Pawn, ComponentNames, PropertyNames));
                }

                if (AActor *ViewTarget = PlayerController->GetViewTarget())
                {
                    Report->SetObjectField(TEXT("viewTarget"), McpDescribeRuntimeActor(ViewTarget, ComponentNames, PropertyNames));
                }

                if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager)
                {
                    TSharedPtr<FJsonObject> CameraManagerObj = McpDescribeRuntimeActor(CameraManager, ComponentNames, PropertyNames);
                    CameraManagerObj->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
                    CameraManagerObj->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
                    Report->SetObjectField(TEXT("playerCameraManager"), CameraManagerObj);
                }
            }

            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Runtime inspection report generated"), Report, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // list_objects
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("list_objects")))
        {
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                    Obj->SetStringField(TEXT("name"), Actor->GetName());
                    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                    Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                    ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects listed"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // find_by_class
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("find_by_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("classPath"), ClassName);
            }
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;

            if (GEditor && GEditor->GetEditorWorldContext().World() && !ClassName.IsEmpty())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (Actor->GetClass()->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
                        Actor->GetClass()->GetPathName().Contains(ClassName))
                    {
                        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                        Obj->SetStringField(TEXT("name"), Actor->GetName());
                        Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects found by class"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // find_by_tag
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("find_by_tag")))
        {
            FString Tag;
            Payload->TryGetStringField(TEXT("tag"), Tag);
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;

            if (GEditor && GEditor->GetEditorWorldContext().World() && !Tag.IsEmpty())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (Actor->ActorHasTag(FName(*Tag)))
                    {
                        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                        Obj->SetStringField(TEXT("name"), Actor->GetName());
                        Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects found by tag"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // inspect_class
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("inspect_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("classPath"), ClassName);
            }
            if (!ClassName.IsEmpty())
            {
                // Try to find the class
                UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
                if (!TargetClass && !ClassName.Contains(TEXT(".")))
                {
                    // Try with /Script/Engine prefix for common classes
                    TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
                }
                if (TargetClass)
                {
                    Resp->SetStringField(TEXT("className"), TargetClass->GetName());
                    Resp->SetStringField(TEXT("classPath"), TargetClass->GetPathName());
                    Resp->SetStringField(TEXT("parentClass"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetName() : TEXT("None"));
                    Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true,
                                           TEXT("Class inspected"), Resp, FString());
                }
                else
                {
                    SendAutomationError(RequestingSocket, RequestId,
                                        FString::Printf(TEXT("Class not found: %s"), *ClassName),
                                        TEXT("CLASS_NOT_FOUND"));
                }
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("className is required for inspect_class"),
                                    TEXT("INVALID_ARGUMENT"));
            }
            return true;
        }
        // ---------------------------------------------------------------------
        // inspect_cdo - delegated to HandleInspectCdoAction (PropertyHandlers)
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("inspect_cdo")))
        {
            return HandleInspectCdoAction(RequestId, Payload, RequestingSocket);
        }

        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Unsupported inspect action: %s"), *SubAction),
                            TEXT("UNKNOWN_ACTION"));
        return true;
    }

    // =========================================================================
    // Handle Object-Specific Inspection
    // =========================================================================
    // Find the target object using centralized helper
    FString ResolvedPath;
    UObject* TargetObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);

    if (!TargetObject)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
                            TEXT("OBJECT_NOT_FOUND"));
        return true;
    }

    // Update path for error messages
    if (!ResolvedPath.IsEmpty())
    {
        ObjectPath = ResolvedPath;
    }

    // -------------------------------------------------------------------------
    // Build inspection result
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    // Basic object info
    Resp->SetStringField(TEXT("objectPath"), TargetObject->GetPathName());
    Resp->SetStringField(TEXT("objectName"), TargetObject->GetName());
    Resp->SetStringField(TEXT("className"), TargetObject->GetClass()->GetName());
    Resp->SetStringField(TEXT("classPath"), TargetObject->GetClass()->GetPathName());

    // If it's an actor, add actor-specific info
    if (AActor *Actor = Cast<AActor>(TargetObject))
    {
        Resp->SetStringField(TEXT("actorLabel"), Actor->GetActorLabel());
        Resp->SetBoolField(TEXT("isActor"), true);
        Resp->SetBoolField(TEXT("isHidden"), Actor->IsHidden());
        Resp->SetBoolField(TEXT("isSelected"), Actor->IsSelected());

        // Transform info
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        const FTransform &Transform = Actor->GetActorTransform();

        TSharedPtr<FJsonObject> LocationObj = McpHandlerUtils::CreateResultObject();
        LocationObj->SetNumberField(TEXT("x"), Transform.GetLocation().X);
        LocationObj->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
        LocationObj->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
        TransformObj->SetObjectField(TEXT("location"), LocationObj);

        TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
        FRotator Rotator = Transform.GetRotation().Rotator();
        RotationObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Rotator.Roll);
        TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

        TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
        ScaleObj->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
        ScaleObj->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
        ScaleObj->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
        TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

        Resp->SetObjectField(TEXT("transform"), TransformObj);

        // Components info
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        TInlineComponentArray<UActorComponent *> Components;
        Actor->GetComponents(Components);

        for (UActorComponent *Component : Components)
        {
            if (Component)
            {
                TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
                CompObj->SetStringField(TEXT("name"), Component->GetName());
                CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
                CompObj->SetBoolField(TEXT("isActive"), Component->IsActive());

                // Add specific info for common component types
                if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isSceneComponent"), true);
                    CompObj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
                }

                if (UStaticMeshComponent *MeshComp = Cast<UStaticMeshComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isStaticMesh"), true);
                    if (MeshComp->GetStaticMesh())
                    {
                        CompObj->SetStringField(TEXT("staticMesh"), MeshComp->GetStaticMesh()->GetName());
                    }
                }

                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        Resp->SetArrayField(TEXT("components"), ComponentsArray);
        Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    }
    else
    {
        Resp->SetBoolField(TEXT("isActor"), false);
    }

    // Tags - only for Actor-derived classes
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    UClass* ObjClass = TargetObject->GetClass();
    if (ObjClass && ObjClass->IsChildOf(AActor::StaticClass()))
    {
        if (AActor* DefaultActor = ObjClass->GetDefaultObject<AActor>())
        {
            for (const FName &Tag : DefaultActor->Tags)
            {
                TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
            }
        }
    }
    Resp->SetArrayField(TEXT("tags"), TagsArray);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Object inspection completed"), Resp, FString());
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("inspect requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
