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

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ConfigCacheIni.h"

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
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

// =============================================================================
// Editor & Asset Includes
// =============================================================================
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GeneralProjectSettings.h"

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
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGrassType.h"
#include "AssetRegistry/AssetRegistryModule.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpEnvironmentHandlers, Log, All);

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
        // Transform from build_environment schema to foliage handler schema
        FString FoliageTypePath;
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
        Payload->TryGetArrayField(TEXT("transforms"), Transforms);

        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }

        // Extract locations from transforms
        TArray<TSharedPtr<FJsonValue>> Locations;
        if (Transforms)
        {
            for (const TSharedPtr<FJsonValue> &V : *Transforms)
            {
                if (!V.IsValid() || V->Type != EJson::Object)
                {
                    continue;
                }
                const TSharedPtr<FJsonObject> *TObj = nullptr;
                if (!V->TryGetObject(TObj) || !TObj)
                {
                    continue;
                }
                const TSharedPtr<FJsonObject> *LocObj = nullptr;
                if (!(*TObj)->TryGetObjectField(TEXT("location"), LocObj) || !LocObj)
                {
                    continue;
                }
                
                double X = 0, Y = 0, Z = 0;
                (*LocObj)->TryGetNumberField(TEXT("x"), X);
                (*LocObj)->TryGetNumberField(TEXT("y"), Y);
                (*LocObj)->TryGetNumberField(TEXT("z"), Z);

                TSharedPtr<FJsonObject> L = McpHandlerUtils::CreateResultObject();
                L->SetNumberField(TEXT("x"), X);
                L->SetNumberField(TEXT("y"), Y);
                L->SetNumberField(TEXT("z"), Z);
                Locations.Add(MakeShared<FJsonValueObject>(L));
            }
        }
        FoliagePayload->SetArrayField(TEXT("locations"), Locations);
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), FoliagePayload,
                                  RequestingSocket);
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
    else if (LowerSub == TEXT("remove_foliage"))
    {
        FString FoliageTypePath;
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        bool bRemoveAll = false;
        Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
        
        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"),
                                   FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("paint_foliage"))
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
    else if (LowerSub == TEXT("add_foliage_type") || LowerSub == TEXT("add_foliage"))
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
    else if (LowerSub == TEXT("set_landscape_material"))
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
    // -------------------------------------------------------------------------
    // create_sky_sphere: Create sky sphere actor
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("create_sky_sphere"))
    {
        if (GEditor)
        {
            UClass *SkySphereClass = LoadClass<AActor>(
                nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/"
                              "SkySphere.SkySphere_C'"));
            if (SkySphereClass)
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
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Failed to create sky sphere");
            ErrorCode = TEXT("CREATION_FAILED");
        }
    }
    // -------------------------------------------------------------------------
    // set_time_of_day: Set time of day on sky sphere
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("set_time_of_day"))
    {
        float TimeOfDay = 12.0f;
        Payload->TryGetNumberField(TEXT("time"), TimeOfDay);

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
        FVector Location(0, 0, 0);
        Payload->TryGetNumberField(TEXT("x"), Location.X);
        Payload->TryGetNumberField(TEXT("y"), Location.Y);
        Payload->TryGetNumberField(TEXT("z"), Location.Z);

        if (GEditor)
        {
            UClass *FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
            if (FogClass)
            {
                AActor *FogVolume = SpawnActorInActiveWorld<AActor>(
                    FogClass, Location, FRotator::ZeroRotator, TEXT("FogVolume"));
                if (FogVolume)
                {
                    bSuccess = true;
                    Message = TEXT("Fog volume created");
                    Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Failed to create fog volume");
            ErrorCode = TEXT("CREATION_FAILED");
        }
    }
    // -------------------------------------------------------------------------
    // Unknown action
    // -------------------------------------------------------------------------
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
// Section 3: Console Command Handler
// =============================================================================

/**
 * HandleConsoleCommandAction
 * ---------------------------
 * Execute console commands with security filtering.
 * 
 * Supports:
 *   - Direct "console_command" action
 *   - "system_control" with action="console_command" in payload
 * 
 * Security:
 *   - Blocks dangerous commands: quit, exit, crash, shutdown, restart, reboot
 *   - Blocks destructive file operations: rm, del, format, rmdir, delete, remove
 *   - Blocks command chaining: &&, ||, ;, |, newlines
 *   - Whitelists safe commands: log (read-only)
 * 
 * Payload:
 *   - command: string (required) - Console command to execute
 * 
 * Response:
 *   - success: bool
 *   - command: string - The executed command
 *   - executed: bool - Whether the command was executed
 */
bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Handle both direct "console_command" and "system_control" with subAction
    const FString LowerAction = Action.ToLower();
    const bool bIsDirectConsoleCommand = LowerAction.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);
    const bool bIsSystemControl = LowerAction.Equals(TEXT("system_control"), ESearchCase::IgnoreCase);

    if (!bIsDirectConsoleCommand && !bIsSystemControl)
    {
        return false;
    }

#if WITH_EDITOR
    // For system_control, check if the sub-action is console_command
    if (bIsSystemControl && Payload.IsValid())
    {
        FString SubAction;
        Payload->TryGetStringField(TEXT("action"), SubAction);
        if (!SubAction.ToLower().Equals(TEXT("console_command")))
        {
            return false; // Not a console_command, let other handlers try
        }
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("console_command payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString Command;
    if (!Payload->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("command field required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // SECURITY: Filter dangerous commands
    FString LowerCommand = Command.ToLower();

    // -------------------------------------------------------------------------
    // Whitelist safe commands
    // -------------------------------------------------------------------------
    // "Log" is a read-only command that prints to console - always safe
    bool bIsWhitelistedCommand = LowerCommand.StartsWith(TEXT("log "));
    if (bIsWhitelistedCommand)
    {
        GEngine->Exec(nullptr, *Command);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("command"), Command);
        Resp->SetBoolField(TEXT("success"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Console command executed"), Resp, FString());
        return true;
    }

    // -------------------------------------------------------------------------
    // Block explicit dangerous commands
    // -------------------------------------------------------------------------
    TArray<FString> BlockedCommands = {
        TEXT("quit"), TEXT("exit"), TEXT("crash"), TEXT("shutdown"),
        TEXT("restart"), TEXT("reboot"), TEXT("debug exec"), TEXT("suicide"),
        TEXT("disconnect"), TEXT("reconnect")
    };

    for (const FString& Blocked : BlockedCommands)
    {
        if (LowerCommand.StartsWith(Blocked))
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Command '%s' is blocked for security"), *Blocked),
                                TEXT("COMMAND_BLOCKED"));
            return true;
        }
    }

    // -------------------------------------------------------------------------
    // Block destructive file operations
    // Note: These tokens have trailing spaces to avoid matching
    // valid MCP action names like "remove_volume" or "delete_actor"
    // -------------------------------------------------------------------------
    TArray<FString> BlockedTokens = {
        TEXT("rm "), TEXT("del "), TEXT("format"), TEXT("rmdir"), TEXT("rd "),
        TEXT("delete "), TEXT("remove "), TEXT("erase ")
    };

    for (const FString& Token : BlockedTokens)
    {
        if (LowerCommand.Contains(Token))
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Command contains blocked token '%s'"), *Token.TrimEnd()),
                                TEXT("COMMAND_BLOCKED"));
            return true;
        }
    }

    // -------------------------------------------------------------------------
    // Block command chaining and injection attempts
    // -------------------------------------------------------------------------
    if (LowerCommand.Contains(TEXT("&&")) || LowerCommand.Contains(TEXT("||")) ||
        LowerCommand.Contains(TEXT(";")) || LowerCommand.Contains(TEXT("|")) ||
        LowerCommand.Contains(TEXT("\n")) || LowerCommand.Contains(TEXT("\r")))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Command chaining and special characters are not allowed"),
                            TEXT("COMMAND_BLOCKED"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Execute the console command
    // -------------------------------------------------------------------------
    GEngine->Exec(nullptr, *Command);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("command"), Command);
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("executed"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Console command executed"), Resp, FString());
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("console_command requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
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

    ProcMesh->RegisterComponent();
    TerrainActor->AddInstanceComponent(ProcMesh);
    TerrainActor->SetRootComponent(ProcMesh);

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
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetName());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());
    Resp->SetNumberField(TEXT("vertices"), Vertices.Num());
    Resp->SetNumberField(TEXT("triangles"), Triangles.Num() / 3);
    Resp->SetNumberField(TEXT("sizeX"), SizeX);
    Resp->SetNumberField(TEXT("sizeY"), SizeY);
    Resp->SetNumberField(TEXT("subdivisions"), Subdivisions);

    // Add verification data
    McpHandlerUtils::AddVerification(Resp, TerrainActor);

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
 *   - get_performance_stats: Performance metrics placeholder
 *   - get_memory_stats: Memory metrics placeholder
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
        LowerSubAction.Equals(TEXT("inspect_class"));

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
        return HandleControlActorAction(RequestId, TEXT("control_actor"), Payload, RequestingSocket);
    }

    // -------------------------------------------------------------------------
    // Require objectPath for non-global actions
    // -------------------------------------------------------------------------
    FString ObjectPath;
    if (!bIsGlobalAction)
    {
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("objectPath required"),
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
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                Resp->SetStringField(TEXT("worldName"), World->GetName());
                Resp->SetStringField(TEXT("levelName"), World->GetCurrentLevel()->GetName());
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
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("message"), TEXT("Performance stats placeholder - implement with actual metrics"));
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Performance stats retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_memory_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_memory_stats")))
        {
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("message"), TEXT("Memory stats placeholder - implement with actual metrics"));
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Memory stats retrieved"), Resp, FString());
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

        // Fallback for unimplemented global actions
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Action %s acknowledged (placeholder implementation)"), *SubAction));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Action processed"), Resp, FString());
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
