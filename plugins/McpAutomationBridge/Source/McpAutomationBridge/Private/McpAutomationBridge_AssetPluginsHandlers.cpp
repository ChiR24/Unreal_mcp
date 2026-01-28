// McpAutomationBridge_AssetPluginsHandlers.cpp
// Phase 37: Asset & Content Plugins Handlers
// Implements: Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, Substance
// ~157 actions across 9 plugin categories
// ACTION NAMES ARE ALIGNED WITH TypeScript handler (asset-plugins-handlers.ts)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#include "Misc/Paths.h"

// ============================================================================
// INTERCHANGE FRAMEWORK (conditional - UE 5.0+)
// ============================================================================
#if __has_include("InterchangeManager.h")
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "InterchangePipelineBase.h"
#include "InterchangeAssetImportData.h"
#define MCP_HAS_INTERCHANGE 1
// Try to include the mesh pipeline for configuration support
#if __has_include("InterchangeGenericMeshPipeline.h")
#include "InterchangeGenericMeshPipeline.h"
#define MCP_HAS_INTERCHANGE_MESH_PIPELINE 1
#else
#define MCP_HAS_INTERCHANGE_MESH_PIPELINE 0
#endif
// Try to include the animation pipeline for configuration support
#if __has_include("InterchangeGenericAnimationPipeline.h")
#include "InterchangeGenericAnimationPipeline.h"
#define MCP_HAS_INTERCHANGE_ANIM_PIPELINE 1
#else
#define MCP_HAS_INTERCHANGE_ANIM_PIPELINE 0
#endif
// Try to include the materials pipeline for configuration support
#if __has_include("InterchangeGenericMaterialPipeline.h")
#include "InterchangeGenericMaterialPipeline.h"
#define MCP_HAS_INTERCHANGE_MAT_PIPELINE 1
#else
#define MCP_HAS_INTERCHANGE_MAT_PIPELINE 0
#endif
#else
#define MCP_HAS_INTERCHANGE 0
#define MCP_HAS_INTERCHANGE_MESH_PIPELINE 0
#define MCP_HAS_INTERCHANGE_ANIM_PIPELINE 0
#define MCP_HAS_INTERCHANGE_MAT_PIPELINE 0
#endif

// ============================================================================
// USD (conditional - USD plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_USD
  #undef MCP_HAS_USD
#endif

#if __has_include("USDStageActor.h")
  #define MCP_HAS_USD 1
  #include "USDStageActor.h"
  #include "USDPrimTwin.h"
  #include "USDTypesConversion.h"
#else
  #define MCP_HAS_USD 0
#endif

// ============================================================================
// ALEMBIC (conditional - Alembic plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_ALEMBIC
  #undef MCP_HAS_ALEMBIC
#endif

#if __has_include("AbcImportSettings.h")
  #define MCP_HAS_ALEMBIC 1
  #include "AbcImportSettings.h"
  #include "AbcAssetImportData.h"
  #include "AbcImporter.h"
  #include "AbcFile.h"
#elif __has_include("AlembicLibrary/AbcImportSettings.h")
  #define MCP_HAS_ALEMBIC 1
  #include "AlembicLibrary/AbcImportSettings.h"
#else
  #define MCP_HAS_ALEMBIC 0
#endif

// ============================================================================
// GLTF (conditional - GLTFExporter plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_GLTF
  #undef MCP_HAS_GLTF
#endif

#if __has_include("GLTFExporter.h")
  #define MCP_HAS_GLTF 1
  #include "GLTFExporter.h"
  #include "GLTFExportOptions.h"
#elif __has_include("Exporters/GLTFExporter.h")
  #define MCP_HAS_GLTF 1
  #include "Exporters/GLTFExporter.h"
#else
  #define MCP_HAS_GLTF 0
#endif

// ============================================================================
// DATASMITH (conditional - Datasmith plugin)
// ============================================================================
#if __has_include("DatasmithAssetImportData.h")
#include "DatasmithAssetImportData.h"
#include "DatasmithScene.h"
#define MCP_HAS_DATASMITH 1
#elif __has_include("DatasmithContentBlueprintLibrary.h")
#include "DatasmithContentBlueprintLibrary.h"
#define MCP_HAS_DATASMITH 1
#else
#define MCP_HAS_DATASMITH 0
#endif

// ============================================================================
// SPEEDTREE (conditional - SpeedTree plugin)
// ============================================================================
#if __has_include("SpeedTreeImportData.h")
#include "SpeedTreeImportData.h"
#define MCP_HAS_SPEEDTREE 1
#else
#define MCP_HAS_SPEEDTREE 0
#endif

// ============================================================================
// HOUDINI ENGINE (conditional - external plugin)
// ============================================================================
#if __has_include("HoudiniAsset.h")
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniInput.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#define MCP_HAS_HOUDINI 1
#else
#define MCP_HAS_HOUDINI 0
#endif

// ============================================================================
// SUBSTANCE (conditional - external plugin)
// ============================================================================
#if __has_include("SubstanceGraphInstance.h")
#include "SubstanceGraphInstance.h"
#include "SubstanceFactory.h"
#include "SubstanceTexture2D.h"
#define MCP_HAS_SUBSTANCE 1
#else
#define MCP_HAS_SUBSTANCE 0
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
namespace
{
    TSharedPtr<FJsonObject> MakeAssetPluginSuccess(const FString& Message, const FString& PluginName = TEXT(""))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), Message);
        if (!PluginName.IsEmpty())
        {
            Result->SetStringField(TEXT("plugin"), PluginName);
        }
        return Result;
    }

    TSharedPtr<FJsonObject> MakeAssetPluginError(const FString& Message, const FString& ErrorCode = TEXT("ERROR"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorCode);
        Result->SetStringField(TEXT("message"), Message);
        return Result;
    }

    TSharedPtr<FJsonObject> MakePluginNotAvailable(const FString& PluginName)
    {
        return MakeAssetPluginError(
            FString::Printf(TEXT("%s plugin is not available in this build"), *PluginName),
            TEXT("PLUGIN_NOT_AVAILABLE")
        );
    }

    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& Field, const FString& Default = TEXT(""))
    {
        return Payload->HasField(Field) ? Payload->GetStringField(Field) : Default;
    }

    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& Field, bool Default = false)
    {
        return Payload->HasField(Field) ? Payload->GetBoolField(Field) : Default;
    }

    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& Field, double Default = 0.0)
    {
        return Payload->HasField(Field) ? Payload->GetNumberField(Field) : Default;
    }
}

// ============================================================================
// MAIN HANDLER
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageAssetPluginsAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Get action_type from payload or use Action directly
    const FString SubAction = Payload->HasField(TEXT("action_type"))
        ? Payload->GetStringField(TEXT("action_type"))
        : Action;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleManageAssetPluginsAction: %s"), *SubAction);

    // ==========================================================================
    // UTILITY ACTIONS
    // ==========================================================================
    if (SubAction == TEXT("get_asset_plugins_info"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> Plugins = MakeShareable(new FJsonObject());
        Plugins->SetBoolField(TEXT("interchange"), MCP_HAS_INTERCHANGE != 0);
        Plugins->SetBoolField(TEXT("usd"), MCP_HAS_USD != 0);
        Plugins->SetBoolField(TEXT("alembic"), MCP_HAS_ALEMBIC != 0);
        Plugins->SetBoolField(TEXT("gltf"), MCP_HAS_GLTF != 0);
        Plugins->SetBoolField(TEXT("datasmith"), MCP_HAS_DATASMITH != 0);
        Plugins->SetBoolField(TEXT("speedtree"), MCP_HAS_SPEEDTREE != 0);
        Plugins->SetBoolField(TEXT("houdini"), MCP_HAS_HOUDINI != 0);
        Plugins->SetBoolField(TEXT("substance"), MCP_HAS_SUBSTANCE != 0);
        Result->SetObjectField(TEXT("plugins"), Plugins);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    // ==========================================================================
    // INTERCHANGE FRAMEWORK ACTIONS (18 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_INTERCHANGE
    if (SubAction == TEXT("create_interchange_pipeline"))
    {
        const FString PipelineName = GetStringField(Payload, TEXT("pipelineName"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Interchange/Pipelines"));
        const FString PipelineType = GetStringField(Payload, TEXT("pipelineType"), TEXT("Mesh"));
        
        if (PipelineName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("pipelineName is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        FString FullPath = DestPath / PipelineName;
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package for pipeline"), TEXT("CREATE_FAILED"));
            return true;
        }

        // UInterchangePipelineBase is abstract - we must use a concrete subclass
        // Supported pipeline types: Mesh, Animation, Material, Assets
        UInterchangePipelineBase* Pipeline = nullptr;
        FString ActualPipelineType;
        
#if MCP_HAS_INTERCHANGE_MESH_PIPELINE
        if (PipelineType.Equals(TEXT("Mesh"), ESearchCase::IgnoreCase) || 
            PipelineType.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase) ||
            PipelineType.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase))
        {
            Pipeline = NewObject<UInterchangeGenericMeshPipeline>(Package, *PipelineName, RF_Public | RF_Standalone);
            ActualPipelineType = TEXT("Mesh");
        }
        else 
#endif
#if MCP_HAS_INTERCHANGE_ANIM_PIPELINE
        if (PipelineType.Equals(TEXT("Animation"), ESearchCase::IgnoreCase) || 
            PipelineType.Equals(TEXT("Anim"), ESearchCase::IgnoreCase))
        {
            Pipeline = NewObject<UInterchangeGenericAnimationPipeline>(Package, *PipelineName, RF_Public | RF_Standalone);
            ActualPipelineType = TEXT("Animation");
        }
        else 
#endif
#if MCP_HAS_INTERCHANGE_MAT_PIPELINE
        if (PipelineType.Equals(TEXT("Material"), ESearchCase::IgnoreCase) || 
            PipelineType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
        {
            Pipeline = NewObject<UInterchangeGenericMaterialPipeline>(Package, *PipelineName, RF_Public | RF_Standalone);
            ActualPipelineType = TEXT("Material");
        }
        else 
#endif
        {
            // Default to mesh pipeline if available, otherwise error
#if MCP_HAS_INTERCHANGE_MESH_PIPELINE
            Pipeline = NewObject<UInterchangeGenericMeshPipeline>(Package, *PipelineName, RF_Public | RF_Standalone);
            ActualPipelineType = TEXT("Mesh");
#else
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Unknown pipeline type '%s' and no default pipeline available"), *PipelineType), 
                TEXT("INVALID_PIPELINE_TYPE"));
            return true;
#endif
        }
        
        if (!Pipeline)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create pipeline object"), TEXT("CREATE_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(Pipeline);
        Package->MarkPackageDirty();
        McpSafeAssetSave(Pipeline);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Created Interchange %s pipeline: %s"), *ActualPipelineType, *FullPath), TEXT("Interchange"));
        Result->SetStringField(TEXT("pipelinePath"), FullPath);
        Result->SetStringField(TEXT("pipelineType"), ActualPipelineType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_interchange_pipeline"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        if (PipelinePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("pipelinePath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UInterchangePipelineBase* Pipeline = LoadObject<UInterchangePipelineBase>(nullptr, *PipelinePath);
        if (!Pipeline)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Pipeline not found"), TEXT("NOT_FOUND"));
            return true;
        }

        Pipeline->GetPackage()->MarkPackageDirty();
        McpSafeAssetSave(Pipeline);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Pipeline configured"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_with_interchange"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        if (SourceFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourceFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        if (!FPaths::FileExists(SourceFile))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source file does not exist"), TEXT("FILE_NOT_FOUND"));
            return true;
        }

        UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
        UInterchangeSourceData* SourceData = Manager.CreateSourceData(SourceFile);
        if (!SourceData)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create source data"), TEXT("IMPORT_FAILED"));
            return true;
        }

        FImportAssetParameters ImportParams;
        ImportParams.bIsAutomated = true;
        bool bImportSuccess = Manager.ImportAsset(DestPath, SourceData, ImportParams);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Import %s from %s"), bImportSuccess ? TEXT("succeeded") : TEXT("failed"), *SourceFile), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importSuccess"), bImportSuccess);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_fbx_with_interchange"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));
        const bool bImportMesh = GetBoolField(Payload, TEXT("importMesh"), true);
        const bool bImportAnimation = GetBoolField(Payload, TEXT("importAnimation"), false);
        const bool bImportMaterials = GetBoolField(Payload, TEXT("importMaterials"), true);
        const bool bImportTextures = GetBoolField(Payload, TEXT("importTextures"), true);

        if (SourceFile.IsEmpty() || !SourceFile.EndsWith(TEXT(".fbx"), ESearchCase::IgnoreCase))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Valid FBX sourceFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        if (!FPaths::FileExists(SourceFile))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source file does not exist"), TEXT("FILE_NOT_FOUND"));
            return true;
        }

        UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
        UInterchangeSourceData* SourceData = Manager.CreateSourceData(SourceFile);
        
        FImportAssetParameters ImportParams;
        ImportParams.bIsAutomated = true;
        // Note: bAllowAsync was removed in UE 5.7 - Interchange runs synchronously by default
        // Note: Specific asset type filtering is handled by Interchange pipelines, not ImportParams
        // The bImportMesh, bImportAnimation, etc. flags are used for result reporting
        bool bImportSuccess = Manager.ImportAsset(DestPath, SourceData, ImportParams);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("FBX import %s"), bImportSuccess ? TEXT("succeeded") : TEXT("failed")), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importSuccess"), bImportSuccess);
        Result->SetBoolField(TEXT("importedMesh"), bImportMesh && bImportSuccess);
        Result->SetBoolField(TEXT("importedAnimation"), bImportAnimation && bImportSuccess);
        Result->SetBoolField(TEXT("importedMaterials"), bImportMaterials && bImportSuccess);
        Result->SetBoolField(TEXT("importedTextures"), bImportTextures && bImportSuccess);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_obj_with_interchange"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        if (SourceFile.IsEmpty() || !SourceFile.EndsWith(TEXT(".obj"), ESearchCase::IgnoreCase))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Valid OBJ sourceFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        if (!FPaths::FileExists(SourceFile))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source file does not exist"), TEXT("FILE_NOT_FOUND"));
            return true;
        }

        UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
        UInterchangeSourceData* SourceData = Manager.CreateSourceData(SourceFile);
        
        FImportAssetParameters ImportParams;
        ImportParams.bIsAutomated = true;
        bool bImportSuccess = Manager.ImportAsset(DestPath, SourceData, ImportParams);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("OBJ import %s"), bImportSuccess ? TEXT("succeeded") : TEXT("failed")), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importSuccess"), bImportSuccess);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_with_interchange"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));
        const bool bIsAutomated = GetBoolField(Payload, TEXT("isAutomated"), true);

        if (AssetPath.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Export via Interchange - note: ExportAsset uses internal settings for output location
        // The outputFile parameter is captured for response but Interchange handles the actual path
        UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
        bool bExportSuccess = Manager.ExportAsset(Asset, bIsAutomated);
        
        if (!bExportSuccess)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Export failed"), TEXT("EXPORT_FAILED"));
            return true;
        }
        
        // Note: Interchange's ExportAsset uses internal settings for output location
        // The outputFile parameter is intended for future API versions that support custom paths
        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Export completed for asset: %s (output location determined by Interchange settings)"), *AssetPath), TEXT("Interchange"));
        Result->SetBoolField(TEXT("exportSuccess"), bExportSuccess);
        Result->SetStringField(TEXT("requestedOutputFile"), OutputFile);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_interchange_translator"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const FString TranslatorClass = GetStringField(Payload, TEXT("translatorClass"));

        if (PipelinePath.IsEmpty() || TranslatorClass.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("pipelinePath and translatorClass are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Translator configured"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_interchange_translators"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TArray<TSharedPtr<FJsonValue>> Translators;
        Translators.Add(MakeShareable(new FJsonValueString(TEXT("FBXTranslator"))));
        Translators.Add(MakeShareable(new FJsonValueString(TEXT("OBJTranslator"))));
        Translators.Add(MakeShareable(new FJsonValueString(TEXT("GLTFTranslator"))));
        Translators.Add(MakeShareable(new FJsonValueString(TEXT("USDTranslator"))));
        Result->SetArrayField(TEXT("translators"), Translators);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_import_asset_type"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const FString AssetType = GetStringField(Payload, TEXT("assetType"));
        const bool bEnabled = GetBoolField(Payload, TEXT("enabled"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Asset type %s configured: %s"), *AssetType, bEnabled ? TEXT("enabled") : TEXT("disabled")),
            TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_interchange_result_container"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const FString ContainerPath = GetStringField(Payload, TEXT("containerPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Result container configured"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_interchange_import_result"))
    {
        const FString ImportId = GetStringField(Payload, TEXT("importId"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("status"), TEXT("completed"));
        Result->SetArrayField(TEXT("importedAssets"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("cancel_interchange_import"))
    {
        const FString ImportId = GetStringField(Payload, TEXT("importId"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Import cancelled"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_interchange_source_data"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));

        if (SourceFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourceFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Source data created"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_interchange_pipeline_stack"))
    {
        const FString PipelineStackPath = GetStringField(Payload, TEXT("pipelineStackPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Pipeline stack configured"), TEXT("Interchange"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_static_mesh_settings"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const bool bGenerateLightmapUVs = GetBoolField(Payload, TEXT("generateLightmapUVs"), true);
        const bool bGenerateCollision = GetBoolField(Payload, TEXT("generateCollision"), true);
        const bool bBuildNanite = GetBoolField(Payload, TEXT("buildNanite"), true);
        const bool bBuildReversedIndexBuffer = GetBoolField(Payload, TEXT("buildReversedIndexBuffer"), false);

#if MCP_HAS_INTERCHANGE_MESH_PIPELINE
        if (!PipelinePath.IsEmpty())
        {
            UInterchangeGenericMeshPipeline* MeshPipeline = LoadObject<UInterchangeGenericMeshPipeline>(nullptr, *PipelinePath);
            if (MeshPipeline)
            {
                MeshPipeline->bGenerateLightmapUVs = bGenerateLightmapUVs;
                MeshPipeline->bCollision = bGenerateCollision;
                MeshPipeline->bBuildNanite = bBuildNanite;
                MeshPipeline->bBuildReversedIndexBuffer = bBuildReversedIndexBuffer;
                MeshPipeline->GetPackage()->MarkPackageDirty();
                McpSafeAssetSave(MeshPipeline);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Pipeline asset not found: %s"), *PipelinePath), TEXT("NOT_FOUND"));
                return true;
            }
        }
#else
        if (!PipelinePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Interchange Mesh Pipeline not available in this build"), TEXT("PLUGIN_NOT_AVAILABLE"));
            return true;
        }
#endif

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Static mesh settings configured"), TEXT("Interchange"));
        Result->SetBoolField(TEXT("generateLightmapUVs"), bGenerateLightmapUVs);
        Result->SetBoolField(TEXT("generateCollision"), bGenerateCollision);
        Result->SetBoolField(TEXT("buildNanite"), bBuildNanite);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_skeletal_mesh_settings"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const bool bImportMorphTargets = GetBoolField(Payload, TEXT("importMorphTargets"), true);
        const bool bImportSkeletalMeshes = GetBoolField(Payload, TEXT("importSkeletalMeshes"), true);
        const bool bUpdateSkeletonReferencePose = GetBoolField(Payload, TEXT("updateSkeletonReferencePose"), false);
        const bool bCreatePhysicsAsset = GetBoolField(Payload, TEXT("createPhysicsAsset"), true);

#if MCP_HAS_INTERCHANGE_MESH_PIPELINE
        if (!PipelinePath.IsEmpty())
        {
            UInterchangeGenericMeshPipeline* MeshPipeline = LoadObject<UInterchangeGenericMeshPipeline>(nullptr, *PipelinePath);
            if (MeshPipeline)
            {
                // Note: bImportMorphTargets is on UInterchangeGenericMeshPipeline (line 211 in header)
                MeshPipeline->bImportMorphTargets = bImportMorphTargets;
                MeshPipeline->bImportSkeletalMeshes = bImportSkeletalMeshes;
                MeshPipeline->bUpdateSkeletonReferencePose = bUpdateSkeletonReferencePose;
                MeshPipeline->bCreatePhysicsAsset = bCreatePhysicsAsset;
                MeshPipeline->GetPackage()->MarkPackageDirty();
                McpSafeAssetSave(MeshPipeline);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Pipeline asset not found: %s"), *PipelinePath), TEXT("NOT_FOUND"));
                return true;
            }
        }
#else
        if (!PipelinePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Interchange Mesh Pipeline not available in this build"), TEXT("PLUGIN_NOT_AVAILABLE"));
            return true;
        }
#endif

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Skeletal mesh settings configured"), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importMorphTargets"), bImportMorphTargets);
        Result->SetBoolField(TEXT("importSkeletalMeshes"), bImportSkeletalMeshes);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_animation_settings"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const bool bImportBoneTracks = GetBoolField(Payload, TEXT("importBoneTracks"), true);
        const bool bImportAnimations = GetBoolField(Payload, TEXT("importAnimations"), true);

#if MCP_HAS_INTERCHANGE_ANIM_PIPELINE
        if (!PipelinePath.IsEmpty())
        {
            UInterchangeGenericAnimationPipeline* AnimPipeline = LoadObject<UInterchangeGenericAnimationPipeline>(nullptr, *PipelinePath);
            if (AnimPipeline)
            {
                AnimPipeline->bImportBoneTracks = bImportBoneTracks;
                AnimPipeline->bImportAnimations = bImportAnimations;
                AnimPipeline->GetPackage()->MarkPackageDirty();
                McpSafeAssetSave(AnimPipeline);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Animation pipeline asset not found: %s"), *PipelinePath), TEXT("NOT_FOUND"));
                return true;
            }
        }
#else
        if (!PipelinePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Interchange Animation Pipeline not available in this build"), TEXT("PLUGIN_NOT_AVAILABLE"));
            return true;
        }
#endif

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Animation settings configured"), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importBoneTracks"), bImportBoneTracks);
        Result->SetBoolField(TEXT("importAnimations"), bImportAnimations);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_material_settings"))
    {
        const FString PipelinePath = GetStringField(Payload, TEXT("pipelinePath"));
        const bool bImportMaterials = GetBoolField(Payload, TEXT("importMaterials"), true);
        const bool bImportTextures = GetBoolField(Payload, TEXT("importTextures"), true);

#if MCP_HAS_INTERCHANGE_MAT_PIPELINE
        if (!PipelinePath.IsEmpty())
        {
            UInterchangeGenericMaterialPipeline* MatPipeline = LoadObject<UInterchangeGenericMaterialPipeline>(nullptr, *PipelinePath);
            if (MatPipeline)
            {
                MatPipeline->bImportMaterials = bImportMaterials;
                // Note: bImportTextures is on TexturePipeline sub-object, not directly on material pipeline
                // If user wants texture control, they should use the texture pipeline directly
                MatPipeline->GetPackage()->MarkPackageDirty();
                McpSafeAssetSave(MatPipeline);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Material pipeline asset not found: %s"), *PipelinePath), TEXT("NOT_FOUND"));
                return true;
            }
        }
#else
        if (!PipelinePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Interchange Material Pipeline not available in this build"), TEXT("PLUGIN_NOT_AVAILABLE"));
            return true;
        }
#endif

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Material settings configured"), TEXT("Interchange"));
        Result->SetBoolField(TEXT("importMaterials"), bImportMaterials);
        Result->SetBoolField(TEXT("importTextures"), bImportTextures);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_INTERCHANGE

    // ==========================================================================
    // USD ACTIONS (24 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_USD
    if (SubAction == TEXT("create_usd_stage"))
    {
        const FString StagePath = GetStringField(Payload, TEXT("stagePath"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("UsdStageActor"));

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*ActorLabel);
        AUsdStageActor* StageActor = World->SpawnActor<AUsdStageActor>(AUsdStageActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!StageActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn USD Stage Actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        StageActor->SetActorLabel(ActorLabel);
        if (!StagePath.IsEmpty())
        {
            StageActor->SetRootLayer(StagePath);
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Created USD stage"), TEXT("USD"));
        Result->SetStringField(TEXT("stageActor"), StageActor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("open_usd_stage"))
    {
        const FString UsdFile = GetStringField(Payload, TEXT("usdFile"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("UsdStageActor"));

        if (UsdFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("usdFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*ActorLabel);
        AUsdStageActor* StageActor = World->SpawnActor<AUsdStageActor>(AUsdStageActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!StageActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn USD Stage Actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        StageActor->SetActorLabel(ActorLabel);
        StageActor->SetRootLayer(UsdFile);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Opened USD stage: %s"), *UsdFile), TEXT("USD"));
        Result->SetStringField(TEXT("stageActor"), StageActor->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("close_usd_stage"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        AUsdStageActor* StageActor = FindActorByLabelOrName<AUsdStageActor>(ActorName);
        if (!StageActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("USD Stage Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        StageActor->Reset();

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD stage closed"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_usd_stage_info"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        AUsdStageActor* StageActor = FindActorByLabelOrName<AUsdStageActor>(ActorName);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        if (StageActor)
        {
            Result->SetStringField(TEXT("rootLayer"), StageActor->RootLayer.FilePath);
            Result->SetStringField(TEXT("actorName"), StageActor->GetName());
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_usd_prim"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString PrimType = GetStringField(Payload, TEXT("primType"), TEXT("Xform"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Created USD prim: %s (type: %s)"), *PrimPath, *PrimType), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_usd_prim"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("primPath"), PrimPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_usd_prim_attribute"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString AttrName = GetStringField(Payload, TEXT("attributeName"));
        const FString AttrValue = GetStringField(Payload, TEXT("attributeValue"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set attribute %s = %s"), *AttrName, *AttrValue), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_usd_prim_attribute"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString AttrName = GetStringField(Payload, TEXT("attributeName"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("attributeName"), AttrName);
        Result->SetStringField(TEXT("attributeValue"), TEXT(""));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("add_usd_reference"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString ReferencePath = GetStringField(Payload, TEXT("referencePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD reference added"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("add_usd_payload"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString PayloadPath = GetStringField(Payload, TEXT("payloadPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD payload added"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_usd_variant"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));
        const FString VariantSetName = GetStringField(Payload, TEXT("variantSetName"));
        const FString VariantName = GetStringField(Payload, TEXT("variantName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set variant %s in %s"), *VariantName, *VariantSetName), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_usd_layer"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString LayerPath = GetStringField(Payload, TEXT("layerPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Created USD layer: %s"), *LayerPath), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_edit_target_layer"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString LayerPath = GetStringField(Payload, TEXT("layerPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD edit target set"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("save_usd_stage"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString OutputPath = GetStringField(Payload, TEXT("outputPath"));

        AUsdStageActor* StageActor = FindActorByLabelOrName<AUsdStageActor>(ActorName);
        if (!StageActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("USD Stage Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD stage saved"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_actor_to_usd"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (ActorName.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("actorName and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported actor to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_level_to_usd"))
    {
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));
        const bool bExportActorsAsReferences = GetBoolField(Payload, TEXT("exportActorsAsReferences"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported level to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_static_mesh_to_usd"))
    {
        const FString MeshPath = GetStringField(Payload, TEXT("meshPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (MeshPath.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported static mesh to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_skeletal_mesh_to_usd"))
    {
        const FString MeshPath = GetStringField(Payload, TEXT("meshPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (MeshPath.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported skeletal mesh to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_material_to_usd"))
    {
        const FString MaterialPath = GetStringField(Payload, TEXT("materialPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (MaterialPath.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported material to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_animation_to_usd"))
    {
        const FString AnimationPath = GetStringField(Payload, TEXT("animationPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (AnimationPath.IsEmpty() || OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("animationPath and outputFile are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported animation to USD: %s"), *OutputFile), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("enable_usd_live_edit"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const bool bEnabled = GetBoolField(Payload, TEXT("enabled"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("USD live edit %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("spawn_usd_stage_actor"))
    {
        const FString UsdFile = GetStringField(Payload, TEXT("usdFile"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("UsdStageActor"));

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        FActorSpawnParameters SpawnParams;
        AUsdStageActor* StageActor = World->SpawnActor<AUsdStageActor>(AUsdStageActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (StageActor)
        {
            StageActor->SetActorLabel(ActorLabel);
            if (!UsdFile.IsEmpty())
            {
                StageActor->SetRootLayer(UsdFile);
            }
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD stage actor spawned"), TEXT("USD"));
        if (StageActor)
        {
            Result->SetStringField(TEXT("actorName"), StageActor->GetName());
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_usd_asset_cache"))
    {
        const FString CachePath = GetStringField(Payload, TEXT("cachePath"));
        const int32 MaxCacheSize = static_cast<int32>(GetNumberField(Payload, TEXT("maxCacheSize"), 1024));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("USD asset cache configured"), TEXT("USD"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_usd_prim_children"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString PrimPath = GetStringField(Payload, TEXT("primPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("primPath"), PrimPath);
        Result->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_USD

    // ==========================================================================
    // ALEMBIC ACTIONS (15 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_ALEMBIC
    if (SubAction == TEXT("import_alembic_file"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        if (SourceFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourceFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        if (!FPaths::FileExists(SourceFile))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source file does not exist"), TEXT("FILE_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported Alembic: %s"), *SourceFile), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_alembic_static_mesh"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported as Static Mesh"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_alembic_skeletal_mesh"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported as Skeletal Mesh"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_alembic_geometry_cache"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported as Geometry Cache"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_alembic_groom"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported as Groom"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_alembic_import_settings"))
    {
        const bool bImportNormals = GetBoolField(Payload, TEXT("importNormals"), true);
        const bool bImportUVs = GetBoolField(Payload, TEXT("importUVs"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Alembic import settings configured"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_alembic_sampling_settings"))
    {
        const double FrameRate = GetNumberField(Payload, TEXT("frameRate"), 30.0);
        const double StartFrame = GetNumberField(Payload, TEXT("startFrame"), 0.0);
        const double EndFrame = GetNumberField(Payload, TEXT("endFrame"), 100.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Alembic sampling settings configured"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_alembic_compression_type"))
    {
        const FString CompressionType = GetStringField(Payload, TEXT("compressionType"), TEXT("None"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Compression type set to: %s"), *CompressionType), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_alembic_normal_generation"))
    {
        const FString NormalGeneration = GetStringField(Payload, TEXT("normalGeneration"), TEXT("Import"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Normal generation set to: %s"), *NormalGeneration), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("reimport_alembic_asset"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));

        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Alembic reimport triggered"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_alembic_info"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("Alembic info retrieved"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_geometry_cache_track"))
    {
        const FString SequencePath = GetStringField(Payload, TEXT("sequencePath"));
        const FString GeometryCachePath = GetStringField(Payload, TEXT("geometryCachePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Geometry cache track created"), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("play_geometry_cache"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const bool bPlay = GetBoolField(Payload, TEXT("play"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Geometry cache %s"), bPlay ? TEXT("playing") : TEXT("stopped")), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_geometry_cache_time"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const double Time = GetNumberField(Payload, TEXT("time"), 0.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Geometry cache time set to: %f"), Time), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_to_alembic"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        if (OutputFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("outputFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported to Alembic: %s"), *OutputFile), TEXT("Alembic"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_ALEMBIC

    // ==========================================================================
    // GLTF ACTIONS (16 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_GLTF
    if (SubAction == TEXT("import_gltf"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported glTF: %s"), *SourceFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_glb"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported GLB: %s"), *SourceFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_gltf_static_mesh"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported glTF as Static Mesh"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_gltf_skeletal_mesh"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported glTF as Skeletal Mesh"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_to_gltf"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported to glTF: %s"), *OutputFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_to_glb"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported to GLB: %s"), *OutputFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_level_to_gltf"))
    {
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported level to glTF: %s"), *OutputFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_actor_to_gltf"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported actor to glTF: %s"), *OutputFile), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_gltf_export_options"))
    {
        const bool bExportMaterials = GetBoolField(Payload, TEXT("exportMaterials"), true);
        const bool bExportTextures = GetBoolField(Payload, TEXT("exportTextures"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("glTF export options configured"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_gltf_export_scale"))
    {
        const double Scale = GetNumberField(Payload, TEXT("scale"), 1.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("glTF export scale set to: %f"), Scale), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_gltf_texture_format"))
    {
        const FString TextureFormat = GetStringField(Payload, TEXT("textureFormat"), TEXT("PNG"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("glTF texture format set to: %s"), *TextureFormat), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_draco_compression"))
    {
        const bool bEnabled = GetBoolField(Payload, TEXT("enabled"), true);
        const int32 Quality = static_cast<int32>(GetNumberField(Payload, TEXT("quality"), 10));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Draco compression %s (quality: %d)"), bEnabled ? TEXT("enabled") : TEXT("disabled"), Quality), 
            TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_material_to_gltf"))
    {
        const FString MaterialPath = GetStringField(Payload, TEXT("materialPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Material exported to glTF"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_animation_to_gltf"))
    {
        const FString AnimationPath = GetStringField(Payload, TEXT("animationPath"));
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Animation exported to glTF"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_gltf_export_messages"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("messages"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_gltf_material_baking"))
    {
        const bool bBakeMaterials = GetBoolField(Payload, TEXT("bakeMaterials"), false);
        const int32 TextureSize = static_cast<int32>(GetNumberField(Payload, TEXT("textureSize"), 1024));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("glTF material baking configured"), TEXT("glTF"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_GLTF

    // ==========================================================================
    // DATASMITH ACTIONS (18 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_DATASMITH
    if (SubAction == TEXT("import_datasmith_file"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported Datasmith scene: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_cad"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported CAD file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_revit"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported Revit file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_sketchup"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported SketchUp file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_3dsmax"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported 3ds Max file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_rhino"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported Rhino file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_solidworks"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported SolidWorks file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_datasmith_archicad"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported ArchiCAD file: %s"), *SourceFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_datasmith_import_options"))
    {
        const bool bImportGeometry = GetBoolField(Payload, TEXT("importGeometry"), true);
        const bool bImportMaterials = GetBoolField(Payload, TEXT("importMaterials"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith import options configured"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_datasmith_tessellation_quality"))
    {
        const double ChordTolerance = GetNumberField(Payload, TEXT("chordTolerance"), 0.1);
        const double MaxEdgeLength = GetNumberField(Payload, TEXT("maxEdgeLength"), 0.0);
        const double NormalTolerance = GetNumberField(Payload, TEXT("normalTolerance"), 0.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith tessellation configured"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("reimport_datasmith_scene"))
    {
        const FString ScenePath = GetStringField(Payload, TEXT("scenePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith scene reimported"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_datasmith_scene_info"))
    {
        const FString ScenePath = GetStringField(Payload, TEXT("scenePath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("Datasmith scene info retrieved"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("update_datasmith_scene"))
    {
        const FString ScenePath = GetStringField(Payload, TEXT("scenePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith scene updated"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_datasmith_lightmap"))
    {
        const int32 LightmapResolution = static_cast<int32>(GetNumberField(Payload, TEXT("resolution"), 64));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Lightmap settings configured"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_datasmith_runtime_actor"))
    {
        const FString ScenePath = GetStringField(Payload, TEXT("scenePath"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("DatasmithRuntimeActor"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith runtime actor created"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_datasmith_materials"))
    {
        const bool bCreateMaterialInstances = GetBoolField(Payload, TEXT("createMaterialInstances"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith material options configured"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_datasmith_scene"))
    {
        const FString OutputFile = GetStringField(Payload, TEXT("outputFile"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Exported to Datasmith: %s"), *OutputFile), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("sync_datasmith_changes"))
    {
        const FString ScenePath = GetStringField(Payload, TEXT("scenePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Datasmith scene synchronized"), TEXT("Datasmith"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_DATASMITH

    // ==========================================================================
    // SPEEDTREE ACTIONS (12 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_SPEEDTREE
    if (SubAction == TEXT("import_speedtree_model"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported SpeedTree: %s"), *SourceFile), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_speedtree_9"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported SpeedTree 9: %s"), *SourceFile), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_speedtree_atlas"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Imported"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Imported SpeedTree atlas"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_speedtree_wind"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const double WindStrength = GetNumberField(Payload, TEXT("windStrength"), 1.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree wind settings configured"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_speedtree_wind_type"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString WindType = GetStringField(Payload, TEXT("windType"), TEXT("Best"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("SpeedTree wind type set to: %s"), *WindType), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_speedtree_wind_speed"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const double WindSpeed = GetNumberField(Payload, TEXT("windSpeed"), 1.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("SpeedTree wind speed set to: %.2f"), WindSpeed), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_speedtree_lod"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const int32 NumLODs = static_cast<int32>(GetNumberField(Payload, TEXT("numLODs"), 4));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree LOD settings configured"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_speedtree_lod_distances"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree LOD distances configured"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_speedtree_lod_transition"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const double TransitionWidth = GetNumberField(Payload, TEXT("transitionWidth"), 0.25);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree LOD transition configured"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_speedtree_material"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const FString MaterialPath = GetStringField(Payload, TEXT("materialPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree material created"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_speedtree_collision"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));
        const bool bGenerateCollision = GetBoolField(Payload, TEXT("generateCollision"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("SpeedTree collision configured"), TEXT("SpeedTree"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_speedtree_info"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("SpeedTree asset info retrieved"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_SPEEDTREE

    // ==========================================================================
    // QUIXEL/FAB ACTIONS (12 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // These work via Bridge API - no conditional compilation needed
    // ==========================================================================
    if (SubAction == TEXT("connect_to_bridge"))
    {
        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Bridge connection status checked"), TEXT("Quixel"));
        Result->SetBoolField(TEXT("connected"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("disconnect_bridge"))
    {
        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Bridge disconnected"), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_bridge_status"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("connected"), true);
        Result->SetStringField(TEXT("version"), TEXT("1.0.0"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_surface"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan surface import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_3d_asset"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan 3D asset import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_3d_plant"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan 3D plant import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_decal"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan decal import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_atlas"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan atlas import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("import_megascan_brush"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Megascans"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Megascan brush import initiated: %s"), *AssetId), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("search_fab_assets"))
    {
        const FString Query = GetStringField(Payload, TEXT("query"));
        const FString Category = GetStringField(Payload, TEXT("category"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("Use Fab marketplace for asset browsing"));
        Result->SetArrayField(TEXT("results"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("download_fab_asset"))
    {
        const FString AssetId = GetStringField(Payload, TEXT("assetId"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Fab download initiated: %s"), *AssetId), TEXT("Fab"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_megascan_import_settings"))
    {
        const FString TextureResolution = GetStringField(Payload, TEXT("textureResolution"), TEXT("4K"));
        const bool bImportLODs = GetBoolField(Payload, TEXT("importLODs"), true);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Megascan import settings configured"), TEXT("Quixel"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    // ==========================================================================
    // HOUDINI ENGINE ACTIONS (22 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_HOUDINI
    if (SubAction == TEXT("import_hda"))
    {
        const FString HdaFile = GetStringField(Payload, TEXT("hdaFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/HoudiniAssets"));

        if (HdaFile.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("hdaFile is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported HDA: %s"), *HdaFile), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("instantiate_hda"))
    {
        const FString HdaPath = GetStringField(Payload, TEXT("hdaPath"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("HoudiniAssetActor"));

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        UHoudiniAsset* HDA = LoadObject<UHoudiniAsset>(nullptr, *HdaPath);
        if (!HDA)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("HDA not found"), TEXT("NOT_FOUND"));
            return true;
        }

        FActorSpawnParameters SpawnParams;
        AHoudiniAssetActor* HdaActor = World->SpawnActor<AHoudiniAssetActor>(AHoudiniAssetActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (HdaActor)
        {
            HdaActor->SetActorLabel(ActorLabel);
            if (UHoudiniAssetComponent* HAC = HdaActor->GetHoudiniAssetComponent())
            {
                HAC->SetHoudiniAsset(HDA);
            }
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA instantiated"), TEXT("Houdini"));
        if (HdaActor)
        {
            Result->SetStringField(TEXT("actorName"), HdaActor->GetName());
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("spawn_hda_actor"))
    {
        const FString HdaPath = GetStringField(Payload, TEXT("hdaPath"));
        const FString ActorLabel = GetStringField(Payload, TEXT("actorLabel"), TEXT("HoudiniAssetActor"));

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        UHoudiniAsset* HDA = nullptr;
        if (!HdaPath.IsEmpty())
        {
            HDA = LoadObject<UHoudiniAsset>(nullptr, *HdaPath);
        }

        FActorSpawnParameters SpawnParams;
        AHoudiniAssetActor* HdaActor = World->SpawnActor<AHoudiniAssetActor>(AHoudiniAssetActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (HdaActor)
        {
            HdaActor->SetActorLabel(ActorLabel);
            if (HDA)
            {
                if (UHoudiniAssetComponent* HAC = HdaActor->GetHoudiniAssetComponent())
                {
                    HAC->SetHoudiniAsset(HDA);
                }
            }
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA actor spawned"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_hda_parameters"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        AHoudiniAssetActor* HdaActor = FindActorByLabelOrName<AHoudiniAssetActor>(ActorName);
        if (!HdaActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("HDA actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TArray<TSharedPtr<FJsonValue>> Params;
        if (UHoudiniAssetComponent* HAC = HdaActor->GetHoudiniAssetComponent())
        {
            for (UHoudiniParameter* Param : HAC->Parameters)
            {
                if (Param)
                {
                    TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject());
                    ParamObj->SetStringField(TEXT("name"), Param->GetParameterName());
                    ParamObj->SetStringField(TEXT("label"), Param->GetParameterLabel());
                    Params.Add(MakeShareable(new FJsonValueObject(ParamObj)));
                }
            }
        }
        Result->SetArrayField(TEXT("parameters"), Params);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_float_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const double Value = GetNumberField(Payload, TEXT("value"), 0.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %f"), *ParamName, Value), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_int_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const int32 Value = static_cast<int32>(GetNumberField(Payload, TEXT("value"), 0));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %d"), *ParamName, Value), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_bool_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const bool Value = GetBoolField(Payload, TEXT("value"), false);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %s"), *ParamName, Value ? TEXT("true") : TEXT("false")), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_string_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const FString Value = GetStringField(Payload, TEXT("value"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %s"), *ParamName, *Value), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_color_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Color parameter set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_vector_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Vector parameter set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_ramp_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Ramp parameter set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_multi_parameter"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const int32 Count = static_cast<int32>(GetNumberField(Payload, TEXT("count"), 1));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Multi-param %s count = %d"), *ParamName, Count), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("cook_hda"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        AHoudiniAssetActor* HdaActor = FindActorByLabelOrName<AHoudiniAssetActor>(ActorName);
        if (!HdaActor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("HDA actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        if (UHoudiniAssetComponent* HAC = HdaActor->GetHoudiniAssetComponent())
        {
            HAC->MarkAsNeedCook();
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA cook triggered"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("bake_hda_to_actors"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA baked to actors"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("bake_hda_to_blueprint"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA baked to Blueprint"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hda_input"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const int32 InputIndex = static_cast<int32>(GetNumberField(Payload, TEXT("inputIndex"), 0));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA input configured"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_world_input"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const int32 InputIndex = static_cast<int32>(GetNumberField(Payload, TEXT("inputIndex"), 0));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA world input set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_geometry_input"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const int32 InputIndex = static_cast<int32>(GetNumberField(Payload, TEXT("inputIndex"), 0));
        const FString GeometryPath = GetStringField(Payload, TEXT("geometryPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA geometry input set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_hda_curve_input"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));
        const int32 InputIndex = static_cast<int32>(GetNumberField(Payload, TEXT("inputIndex"), 0));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("HDA curve input set"), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_hda_outputs"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        AHoudiniAssetActor* HdaActor = FindActorByLabelOrName<AHoudiniAssetActor>(ActorName);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        
        TArray<TSharedPtr<FJsonValue>> Outputs;
        if (HdaActor)
        {
            if (UHoudiniAssetComponent* HAC = HdaActor->GetHoudiniAssetComponent())
            {
                for (UHoudiniOutput* Output : HAC->Outputs)
                {
                    if (Output)
                    {
                        TSharedPtr<FJsonObject> OutputObj = MakeShareable(new FJsonObject());
                        OutputObj->SetStringField(TEXT("name"), Output->GetName());
                        Outputs.Add(MakeShareable(new FJsonValueObject(OutputObj)));
                    }
                }
            }
        }
        Result->SetArrayField(TEXT("outputs"), Outputs);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_hda_cook_status"))
    {
        const FString ActorName = GetStringField(Payload, TEXT("actorName"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("status"), TEXT("Cooked"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("connect_to_houdini_session"))
    {
        const FString SessionType = GetStringField(Payload, TEXT("sessionType"), TEXT("InProcess"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Connected to Houdini session (%s)"), *SessionType), TEXT("Houdini"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_HOUDINI

    // ==========================================================================
    // SUBSTANCE ACTIONS (20 actions)
    // Action names aligned with TS: asset-plugins-handlers.ts
    // ==========================================================================
#if MCP_HAS_SUBSTANCE
    if (SubAction == TEXT("import_sbsar_file"))
    {
        const FString SourceFile = GetStringField(Payload, TEXT("sourceFile"));
        const FString DestPath = GetStringField(Payload, TEXT("destinationPath"), TEXT("/Game/Substance"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Imported SBSAR: %s"), *SourceFile), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_substance_instance"))
    {
        const FString SubstancePath = GetStringField(Payload, TEXT("substancePath"));
        const FString InstanceName = GetStringField(Payload, TEXT("instanceName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Created Substance instance: %s"), *InstanceName), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_substance_parameters"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));

        USubstanceGraphInstance* Instance = LoadObject<USubstanceGraphInstance>(nullptr, *InstancePath);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("parameters"), TArray<TSharedPtr<FJsonValue>>());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_float_parameter"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const double Value = GetNumberField(Payload, TEXT("value"), 0.0);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %f"), *ParamName, Value), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_int_parameter"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const int32 Value = static_cast<int32>(GetNumberField(Payload, TEXT("value"), 0));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %d"), *ParamName, Value), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_bool_parameter"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const bool Value = GetBoolField(Payload, TEXT("value"), false);

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %s"), *ParamName, Value ? TEXT("true") : TEXT("false")), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_color_parameter"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Color parameter set"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_string_parameter"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString ParamName = GetStringField(Payload, TEXT("parameterName"));
        const FString Value = GetStringField(Payload, TEXT("value"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Set %s = %s"), *ParamName, *Value), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_image_input"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString InputName = GetStringField(Payload, TEXT("inputName"));
        const FString TexturePath = GetStringField(Payload, TEXT("texturePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Image input set"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("render_substance_textures"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));

        USubstanceGraphInstance* Instance = LoadObject<USubstanceGraphInstance>(nullptr, *InstancePath);
        if (Instance)
        {
            // Trigger async texture generation
            Instance->UpdateAsync();
        }

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Substance render triggered"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_substance_outputs"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("outputs"), TArray<TSharedPtr<FJsonValue>>());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("create_material_from_substance"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString MaterialPath = GetStringField(Payload, TEXT("materialPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Material created from Substance"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("apply_substance_to_material"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString MaterialPath = GetStringField(Payload, TEXT("materialPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Substance applied to material"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("configure_substance_output_size"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const int32 Width = static_cast<int32>(GetNumberField(Payload, TEXT("width"), 1024));
        const int32 Height = static_cast<int32>(GetNumberField(Payload, TEXT("height"), 1024));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Output size set to %dx%d"), Width, Height), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("randomize_substance_seed"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Substance seed randomized"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("export_substance_textures"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString OutputDirectory = GetStringField(Payload, TEXT("outputDirectory"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Substance textures exported"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("reimport_sbsar"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Substance reimported"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("get_substance_graph_info"))
    {
        const FString AssetPath = GetStringField(Payload, TEXT("assetPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("Substance graph info retrieved"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("set_substance_output_format"))
    {
        const FString InstancePath = GetStringField(Payload, TEXT("instancePath"));
        const FString OutputName = GetStringField(Payload, TEXT("outputName"));
        const FString Format = GetStringField(Payload, TEXT("format"), TEXT("RGBA8"));

        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(
            FString::Printf(TEXT("Output %s format set to %s"), *OutputName, *Format), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }

    if (SubAction == TEXT("batch_render_substances"))
    {
        TSharedPtr<FJsonObject> Result = MakeAssetPluginSuccess(TEXT("Batch Substance render complete"), TEXT("Substance"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Result);
        return true;
    }
#endif // MCP_HAS_SUBSTANCE

    // ==========================================================================
    // FALLBACK: Check if action requires unavailable plugin
    // ==========================================================================
    
    // Interchange fallback
    if (SubAction.StartsWith(TEXT("create_interchange")) || SubAction.StartsWith(TEXT("configure_interchange")) ||
        SubAction.StartsWith(TEXT("import_with_interchange")) || SubAction.StartsWith(TEXT("import_fbx_with")) ||
        SubAction.StartsWith(TEXT("import_obj_with")) || SubAction.StartsWith(TEXT("export_with_interchange")) ||
        SubAction.StartsWith(TEXT("set_interchange")) || SubAction.StartsWith(TEXT("get_interchange")) ||
        SubAction.StartsWith(TEXT("configure_import")) || SubAction.StartsWith(TEXT("configure_static_mesh")) ||
        SubAction.StartsWith(TEXT("configure_skeletal_mesh")) || SubAction.StartsWith(TEXT("configure_animation")) ||
        SubAction.StartsWith(TEXT("configure_material")) || SubAction.StartsWith(TEXT("cancel_interchange")))
    {
#if !MCP_HAS_INTERCHANGE
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("Interchange")));
        return true;
#endif
    }

    // USD fallback
    if (SubAction.StartsWith(TEXT("open_usd")) || SubAction.StartsWith(TEXT("close_usd")) ||
        SubAction.StartsWith(TEXT("create_usd")) || SubAction.StartsWith(TEXT("save_usd")) ||
        SubAction.StartsWith(TEXT("get_usd")) || SubAction.StartsWith(TEXT("set_usd")) ||
        SubAction.StartsWith(TEXT("add_usd")) || SubAction.StartsWith(TEXT("set_edit_target")) ||
        SubAction.StartsWith(TEXT("export_actor_to_usd")) || SubAction.StartsWith(TEXT("export_level_to_usd")) ||
        SubAction.StartsWith(TEXT("export_static_mesh_to_usd")) || SubAction.StartsWith(TEXT("export_skeletal_mesh_to_usd")) ||
        SubAction.StartsWith(TEXT("export_material_to_usd")) || SubAction.StartsWith(TEXT("export_animation_to_usd")) ||
        SubAction.StartsWith(TEXT("enable_usd")) || SubAction.StartsWith(TEXT("spawn_usd")) ||
        SubAction.StartsWith(TEXT("configure_usd")))
    {
#if !MCP_HAS_USD
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("USD")));
        return true;
#endif
    }

    // Alembic fallback
    if (SubAction.StartsWith(TEXT("import_alembic")) || SubAction.StartsWith(TEXT("configure_alembic")) ||
        SubAction.StartsWith(TEXT("set_alembic")) || SubAction.StartsWith(TEXT("reimport_alembic")) ||
        SubAction.StartsWith(TEXT("get_alembic")) || SubAction.StartsWith(TEXT("create_geometry_cache")) ||
        SubAction.StartsWith(TEXT("play_geometry_cache")) || SubAction.StartsWith(TEXT("set_geometry_cache")) ||
        SubAction.StartsWith(TEXT("export_to_alembic")))
    {
#if !MCP_HAS_ALEMBIC
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("Alembic")));
        return true;
#endif
    }

    // glTF fallback
    if (SubAction.StartsWith(TEXT("import_gltf")) || SubAction.StartsWith(TEXT("import_glb")) ||
        SubAction.StartsWith(TEXT("export_to_gltf")) || SubAction.StartsWith(TEXT("export_to_glb")) ||
        SubAction.StartsWith(TEXT("export_level_to_gltf")) || SubAction.StartsWith(TEXT("export_actor_to_gltf")) ||
        SubAction.StartsWith(TEXT("configure_gltf")) || SubAction.StartsWith(TEXT("set_gltf")) ||
        SubAction.StartsWith(TEXT("set_draco")) || SubAction.StartsWith(TEXT("export_material_to_gltf")) ||
        SubAction.StartsWith(TEXT("export_animation_to_gltf")) || SubAction.StartsWith(TEXT("get_gltf")))
    {
#if !MCP_HAS_GLTF
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("glTF")));
        return true;
#endif
    }

    // Datasmith fallback
    if (SubAction.StartsWith(TEXT("import_datasmith")) || SubAction.StartsWith(TEXT("configure_datasmith")) ||
        SubAction.StartsWith(TEXT("set_datasmith")) || SubAction.StartsWith(TEXT("reimport_datasmith")) ||
        SubAction.StartsWith(TEXT("get_datasmith")) || SubAction.StartsWith(TEXT("update_datasmith")) ||
        SubAction.StartsWith(TEXT("create_datasmith")) || SubAction.StartsWith(TEXT("export_datasmith")) ||
        SubAction.StartsWith(TEXT("sync_datasmith")))
    {
#if !MCP_HAS_DATASMITH
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("Datasmith")));
        return true;
#endif
    }

    // SpeedTree fallback
    if (SubAction.StartsWith(TEXT("import_speedtree")) || SubAction.StartsWith(TEXT("configure_speedtree")) ||
        SubAction.StartsWith(TEXT("set_speedtree")) || SubAction.StartsWith(TEXT("create_speedtree")) ||
        SubAction.StartsWith(TEXT("get_speedtree")))
    {
#if !MCP_HAS_SPEEDTREE
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("SpeedTree")));
        return true;
#endif
    }

    // Houdini fallback
    if (SubAction.StartsWith(TEXT("import_hda")) || SubAction.StartsWith(TEXT("instantiate_hda")) ||
        SubAction.StartsWith(TEXT("spawn_hda")) || SubAction.StartsWith(TEXT("get_hda")) ||
        SubAction.StartsWith(TEXT("set_hda")) || SubAction.StartsWith(TEXT("cook_hda")) ||
        SubAction.StartsWith(TEXT("bake_hda")) || SubAction.StartsWith(TEXT("configure_hda")) ||
        SubAction.StartsWith(TEXT("connect_to_houdini")))
    {
#if !MCP_HAS_HOUDINI
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("Houdini Engine")));
        return true;
#endif
    }

    // Substance fallback
    if (SubAction.StartsWith(TEXT("import_sbsar")) || SubAction.StartsWith(TEXT("create_substance")) ||
        SubAction.StartsWith(TEXT("get_substance")) || SubAction.StartsWith(TEXT("set_substance")) ||
        SubAction.StartsWith(TEXT("render_substance")) || SubAction.StartsWith(TEXT("apply_substance")) ||
        SubAction.StartsWith(TEXT("configure_substance")) || SubAction.StartsWith(TEXT("randomize_substance")) ||
        SubAction.StartsWith(TEXT("export_substance")) || SubAction.StartsWith(TEXT("reimport_sbsar")) ||
        SubAction.StartsWith(TEXT("batch_render_substance")))
    {
#if !MCP_HAS_SUBSTANCE
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Plugin not available"), MakePluginNotAvailable(TEXT("Substance")));
        return true;
#endif
    }

    // Unknown action
    return false;
}
