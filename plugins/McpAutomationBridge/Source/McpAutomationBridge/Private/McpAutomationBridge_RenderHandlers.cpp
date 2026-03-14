// =============================================================================
// McpAutomationBridge_RenderHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Render Target & Advanced Rendering Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_render (Editor Only)
//   - create_render_target: Create UTextureRenderTarget2D asset
//   - attach_render_target_to_volume: Attach RT to PostProcessVolume via MID
//   - nanite_rebuild_mesh: Enable Nanite and rebuild static mesh
//   - lumen_update_scene: Trigger Lumen scene recapture
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: TextureRenderTarget2D, PostProcessVolume, MaterialInstanceDynamic
//   - Editor: EditorAssetLibrary, AssetRegistryModule
// 
// Version Compatibility Notes:
//   - UE 5.7+: Nanite settings accessed via GetNaniteSettings()/SetNaniteSettings()
//   - UE 5.0-5.6: Direct NaniteSettings member access
// 
// Safety:
//   - Uses DoesAssetDirectoryExistOnDisk() to verify parent folder exists
//   - Checks for existing assets before creation to prevent crash
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "UObject/Package.h"
#include "Runtime/Launch/Resources/Version.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleRenderAction(
    const FString& RequestId, 
    const FString& Action, 
    const TSharedPtr<FJsonObject>& Payload, 
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_render"))
    {
        return false;
    }

#if WITH_EDITOR
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    // -------------------------------------------------------------------------
    // create_render_target: Create UTextureRenderTarget2D asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("create_render_target"))
    {
        FString Name;
        Payload->TryGetStringField(TEXT("name"), Name);

        // Validate required name parameter
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("name parameter is required for create_render_target"), 
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Extract optional parameters
        int32 Width = 256;
        int32 Height = 256;
        Payload->TryGetNumberField(TEXT("width"), Width);
        Payload->TryGetNumberField(TEXT("height"), Height);

        FString FormatStr;
        Payload->TryGetStringField(TEXT("format"), FormatStr);

        // Determine package path (supports both 'packagePath' and 'path' aliases)
        FString PackagePath = TEXT("/Game/RenderTargets");
        Payload->TryGetStringField(TEXT("packagePath"), PackagePath);

        // Check for 'path' as alias
        FString PathAlias;
        if (Payload->TryGetStringField(TEXT("path"), PathAlias) && !PathAlias.IsEmpty())
        {
            PackagePath = PathAlias;
        }

        // CRITICAL: Verify parent folder exists on disk
        // DoesDirectoryExist() uses AssetRegistry cache which may be stale
        if (!DoesAssetDirectoryExistOnDisk(PackagePath))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *PackagePath), 
                TEXT("PARENT_FOLDER_NOT_FOUND"));
            return true;
        }

        const FString AssetName = Name;
        const FString FullPath = PackagePath / AssetName;

        // CRITICAL: Check for existing asset to prevent crash
        if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Asset already exists at path: %s. Delete it first or use a different name."), *FullPath), 
                TEXT("ASSET_ALREADY_EXISTS"));
            return true;
        }

        // Create render target
        UPackage* Package = CreatePackage(*FullPath);
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
            Package, 
            UTextureRenderTarget2D::StaticClass(), 
            FName(*AssetName), 
            RF_Public | RF_Standalone);

        if (RT)
        {
            // Map format string to EPixelFormat
            EPixelFormat Format = PF_B8G8R8A8; // Default
            if (!FormatStr.IsEmpty())
            {
                if (FormatStr.Equals(TEXT("RGBA8"), ESearchCase::IgnoreCase) || FormatStr.Equals(TEXT("BGRA8"), ESearchCase::IgnoreCase))
                {
                    Format = PF_B8G8R8A8;
                }
                else if (FormatStr.Equals(TEXT("RGBA16F"), ESearchCase::IgnoreCase) || FormatStr.Equals(TEXT("FloatRGBA"), ESearchCase::IgnoreCase))
                {
                    Format = PF_FloatRGBA;
                }
                else if (FormatStr.Equals(TEXT("RGBA32F"), ESearchCase::IgnoreCase))
                {
                    Format = PF_A32B32G32R32F;
                }
                else if (FormatStr.Equals(TEXT("R8"), ESearchCase::IgnoreCase))
                {
                    Format = PF_R8;
                }
                else if (FormatStr.Equals(TEXT("RG8"), ESearchCase::IgnoreCase))
                {
                    Format = PF_G8; // Closest for grayscale
                }
                else if (FormatStr.Equals(TEXT("R16F"), ESearchCase::IgnoreCase))
                {
                    Format = PF_R16F;
                }
                else if (FormatStr.Equals(TEXT("R32F"), ESearchCase::IgnoreCase))
                {
                    Format = PF_R32_FLOAT;
                }
                else if (FormatStr.Equals(TEXT("A2B10G10R10"), ESearchCase::IgnoreCase))
                {
                    Format = PF_A2B10G10R10;
                }
                RT->InitCustomFormat(Width, Height, Format, false);
            }
            else
            {
                RT->InitAutoFormat(Width, Height);
            }
            RT->UpdateResourceImmediate(true);
            RT->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(RT);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), RT->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Render target created."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to create render target."), TEXT("CREATE_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // attach_render_target_to_volume: Attach RT to PostProcessVolume via MID
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("attach_render_target_to_volume"))
    {
        FString VolumePath;
        Payload->TryGetStringField(TEXT("volumePath"), VolumePath);

        FString TargetPath;
        Payload->TryGetStringField(TEXT("targetPath"), TargetPath);

        // Find the post process volume actor
        APostProcessVolume* Volume = Cast<APostProcessVolume>(FindObject<AActor>(nullptr, *VolumePath));
        if (!Volume)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Volume not found."), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        // Load the render target
        UTextureRenderTarget2D* RT = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
        if (!RT)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Render target not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Get material and parameter info
        FString MaterialPath;
        Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);

        FString ParamName;
        Payload->TryGetStringField(TEXT("parameterName"), ParamName);

        if (MaterialPath.IsEmpty() || ParamName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("materialPath and parameterName required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load base material and create MID
        UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (!BaseMat)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Base material not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Volume);
        if (MID)
        {
            MID->SetTextureParameterValue(FName(*ParamName), RT);
            Volume->Settings.AddBlendable(MID, 1.0f);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("renderTarget"), TargetPath);
            Result->SetStringField(TEXT("materialPath"), MaterialPath);
            Result->SetStringField(TEXT("parameterName"), ParamName);
            Result->SetBoolField(TEXT("attached"), true);
            McpHandlerUtils::AddVerification(Result, Volume);

            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Render target attached to volume via material."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to create MID."), TEXT("CREATE_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // nanite_rebuild_mesh: Enable Nanite and rebuild static mesh
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("nanite_rebuild_mesh"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("assetPath required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
        if (!StaticMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("StaticMesh not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Enable Nanite settings (version-aware)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
        Settings.bEnabled = true;
        StaticMesh->SetNaniteSettings(Settings);
#else
        StaticMesh->NaniteSettings.bEnabled = true;
#endif

        // Mark package dirty
        if (UPackage* Package = StaticMesh->GetOutermost())
        {
            Package->MarkPackageDirty();
        }

        // Rebuild mesh
        StaticMesh->Build(true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetBoolField(TEXT("naniteEnabled"), true);
        Result->SetBoolField(TEXT("rebuilt"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Nanite enabled and mesh rebuilt."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // lumen_update_scene: Trigger Lumen scene recapture
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("lumen_update_scene"))
    {
        // Execute via console command
        if (GEditor)
        {
            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (World)
            {
                GEngine->Exec(World, TEXT("r.Lumen.Scene.Recapture"));

                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                Result->SetStringField(TEXT("action"), TEXT("lumen_update_scene"));
                Result->SetStringField(TEXT("command"), TEXT("r.Lumen.Scene.Recapture"));
                Result->SetBoolField(TEXT("executed"), true);

                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    TEXT("Lumen scene recapture triggered."), Result);
                return true;
            }
        }

        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Could not execute command (no world context)."), TEXT("EXECUTION_FAILED"));
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;

#else
    // Non-editor build
    SendAutomationResponse(RequestingSocket, RequestId, false, 
        TEXT("Render management requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
