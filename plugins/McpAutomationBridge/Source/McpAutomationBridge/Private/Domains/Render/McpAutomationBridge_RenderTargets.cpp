#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Foundation/Render/McpRenderTargetFormat.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace McpRenderHandlers
{
bool HandleCreateRenderTarget(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("name parameter is required for create_render_target"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const double WidthValue = GetJsonNumberField(Payload, TEXT("width"), 256.0);
    const double HeightValue = GetJsonNumberField(Payload, TEXT("height"), 256.0);
    if (!FMath::IsFinite(WidthValue) || !FMath::IsFinite(HeightValue) ||
        WidthValue != FMath::FloorToDouble(WidthValue) || HeightValue != FMath::FloorToDouble(HeightValue) ||
        WidthValue < 1.0 || HeightValue < 1.0 || WidthValue > 8192.0 || HeightValue > 8192.0)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("width and height must be finite whole numbers between 1 and 8192."),
            TEXT("INVALID_RESOLUTION"));
        return true;
    }
    const int32 Width = static_cast<int32>(WidthValue);
    const int32 Height = static_cast<int32>(HeightValue);
    const FString FormatStr = GetJsonStringField(Payload, TEXT("format"));
    FString PackagePath = GetJsonStringField(Payload, TEXT("packagePath"), TEXT("/Game/RenderTargets"));
    const FString PathAlias = GetJsonStringField(Payload, TEXT("path"));
    if (!PathAlias.IsEmpty())
    {
        PackagePath = PathAlias;
    }

    if (!DoesAssetDirectoryExistOnDisk(PackagePath))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *PackagePath),
            TEXT("PARENT_FOLDER_NOT_FOUND"));
        return true;
    }

    const FString FullPath = PackagePath / Name;
    if (McpAssetExists(FullPath))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Asset already exists at path: %s. Delete it first or use a different name."), *FullPath),
            TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    EPixelFormat Format = PF_B8G8R8A8;
    if (!FormatStr.IsEmpty())
    {
        Format = McpParseRenderTargetPixelFormat(FormatStr);
        if (Format == PF_Unknown)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                FString::Printf(TEXT("Unsupported render target format: %s"), *FormatStr),
                TEXT("INVALID_FORMAT"));
            return true;
        }
    }

    const int32 BytesPerPixel = GPixelFormats[Format].BlockBytes;
    const int64 PixelCount = static_cast<int64>(Width) * static_cast<int64>(Height);
    const int64 MaxAllocationBytes = 512ll * 1024ll * 1024ll;
    if (BytesPerPixel <= 0 || PixelCount > MaxAllocationBytes / static_cast<int64>(BytesPerPixel))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Render target dimensions exceed the safe allocation limit."),
            TEXT("INVALID_RESOLUTION"));
        return true;
    }

    UPackage* Package = CreatePackage(*FullPath);
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
        Package, UTextureRenderTarget2D::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!RT)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to create render target."), TEXT("CREATE_FAILED"));
        return true;
    }

    if (!FormatStr.IsEmpty())
    {
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
    Result->SetStringField(TEXT("action"), TEXT("manage_render"));
    Result->SetStringField(TEXT("subAction"), TEXT("create_render_target"));
    Result->SetStringField(TEXT("assetPath"), RT->GetPathName());
    Result->SetBoolField(TEXT("applied"), true);
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Render target created."), Result);
    return true;
}
}
