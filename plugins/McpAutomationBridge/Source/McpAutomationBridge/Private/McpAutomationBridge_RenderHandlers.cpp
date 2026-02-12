#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// Helper macros for JSON field access
#define GetStringFieldRend GetJsonStringField
#define GetNumberFieldRend GetJsonNumberField
#define GetBoolFieldRend GetJsonBoolField

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

bool UMcpAutomationBridgeSubsystem::HandleRenderAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_render"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldRend(Payload, TEXT("subAction"));

    if (SubAction == TEXT("create_render_target"))
    {
        FString Name;
        Payload->TryGetStringField(TEXT("name"), Name);
        int32 Width = 256;
        int32 Height = 256;
        Payload->TryGetNumberField(TEXT("width"), Width);
        Payload->TryGetNumberField(TEXT("height"), Height);
        FString FormatStr;
        Payload->TryGetStringField(TEXT("format"), FormatStr);

        FString PackagePath = TEXT("/Game/RenderTargets");
        Payload->TryGetStringField(TEXT("packagePath"), PackagePath);

        FString AssetName = Name.IsEmpty() ? TEXT("NewRenderTarget") : Name;
        FString FullPath = PackagePath / AssetName;

        UPackage* Package = CreatePackage(*FullPath);
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Package, UTextureRenderTarget2D::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone);
        
        if (RT)
        {
            RT->InitAutoFormat(Width, Height);
            if (!FormatStr.IsEmpty())
            {
                // Map format string to EPixelFormat if needed
            }
            RT->UpdateResourceImmediate(true);
            RT->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(RT);

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("assetPath"), RT->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Render target created."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create render target."), TEXT("CREATE_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("attach_render_target_to_volume"))
    {
        FString VolumePath;
        Payload->TryGetStringField(TEXT("volumePath"), VolumePath);
        FString TargetPath;
        Payload->TryGetStringField(TEXT("targetPath"), TargetPath);

        APostProcessVolume* Volume = Cast<APostProcessVolume>(FindObject<AActor>(nullptr, *VolumePath)); // Might need to search world actors
        if (!Volume)
        {
             // Try to find actor by label in world
             // For now, assume VolumePath is object path if it's an asset, but Volumes are actors.
             // User should provide actor path or name.
             SendAutomationError(RequestingSocket, RequestId, TEXT("Volume not found."), TEXT("ACTOR_NOT_FOUND"));
             return true;
        }

        UTextureRenderTarget2D* RT = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
        if (!RT)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Render target not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Logic to add to blendables...
        // We need a material to wrap the RT.
        FString MaterialPath;
        Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
        FString ParamName;
        Payload->TryGetStringField(TEXT("parameterName"), ParamName);

        if (MaterialPath.IsEmpty() || ParamName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath and parameterName required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (!BaseMat)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Base material not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Volume);
        if (MID)
        {
            MID->SetTextureParameterValue(FName(*ParamName), RT);
            Volume->Settings.AddBlendable(MID, 1.0f);
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("renderTarget"), TargetPath);
            Result->SetStringField(TEXT("materialPath"), MaterialPath);
            Result->SetStringField(TEXT("parameterName"), ParamName);
            Result->SetBoolField(TEXT("attached"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Render target attached to volume via material."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create MID."), TEXT("CREATE_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("nanite_rebuild_mesh"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
        if (!StaticMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("StaticMesh not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Enable Nanite and rebuild
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
        Settings.bEnabled = true;
        StaticMesh->SetNaniteSettings(Settings);
#else
        StaticMesh->NaniteSettings.bEnabled = true;
#endif
        
        if (UPackage* Package = StaticMesh->GetOutermost())
        {
            Package->MarkPackageDirty();
        }

        StaticMesh->Build(true);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetBoolField(TEXT("naniteEnabled"), true);
        Result->SetBoolField(TEXT("rebuilt"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nanite enabled and mesh rebuilt."), Result);
        return true;
    }
    else if (SubAction == TEXT("lumen_update_scene"))
    {
        // Flush Lumen scene via console command
        // r.Lumen.Scene.Recapture
        if (GEditor)
        {
            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (World)
            {
                GEngine->Exec(World, TEXT("r.Lumen.Scene.Recapture"));
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("action"), TEXT("lumen_update_scene"));
                Result->SetStringField(TEXT("command"), TEXT("r.Lumen.Scene.Recapture"));
                Result->SetBoolField(TEXT("executed"), true);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lumen scene recapture triggered."), Result);
                return true;
            }
        }
        SendAutomationError(RequestingSocket, RequestId, TEXT("Could not execute command (no world context)."), TEXT("EXECUTION_FAILED"));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Render management requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

#undef GetStringFieldRend
#undef GetNumberFieldRend
#undef GetBoolFieldRend

