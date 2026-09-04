#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"

#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Modules/ModuleManager.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
namespace
{
bool ValidateInputAssetNameAndPath(
    UMcpAutomationBridgeSubsystem& Bridge,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& RequestId,
    const FString& Name,
    const FString& Path,
    FString& SanitizedPath)
{
    if (Name.IsEmpty() || Path.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Name and path are required."), TEXT("INVALID_ARGUMENT"));
        return false;
    }

    SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid path: '%s' contains traversal or invalid characters."), *Path),
            TEXT("INVALID_PATH"));
        return false;
    }

    if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid asset name '%s': contains path separators or traversal sequences"), *Name),
            TEXT("INVALID_NAME"));
        return false;
    }

    return true;
}

bool SendExistingInputAssetResponse(
    UMcpAutomationBridgeSubsystem& Bridge,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& RequestId,
    UObject* ExistingAsset,
    const TCHAR* Message)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), ExistingAsset->GetPathName());
    McpHandlerUtils::AddVerification(Result, ExistingAsset);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
    return true;
}

bool SaveNewInputAssetResponse(
    UMcpAutomationBridgeSubsystem& Bridge,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& RequestId,
    UObject* NewAsset,
    const TCHAR* Message,
    const TCHAR* FailureMessage)
{
    if (!NewAsset)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, FailureMessage, TEXT("CREATION_FAILED"));
        return true;
    }

    SaveLoadedAssetThrottled(NewAsset, -1.0, true);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    McpHandlerUtils::AddVerification(Result, NewAsset);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
    return true;
}
}

bool HandleCreateInputAction(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    FString SanitizedPath;
    if (!ValidateInputAssetNameAndPath(Bridge, RequestingSocket, RequestId, Name, Path, SanitizedPath))
    {
        return true;
    }

    // Optional value type: "digital" (default), "axis1d", "axis2d", "axis3d".
    // Movement/look actions need axes; without this param every InputAction was
    // born Digital (bool) and could not carry 2D/3D axis values.
    FString ValueType;
    Payload->TryGetStringField(TEXT("valueType"), ValueType);
    ValueType = ValueType.ToLower();
    const bool bValidValueType =
        ValueType.IsEmpty() ||
        ValueType == TEXT("digital") || ValueType == TEXT("0") ||
        ValueType == TEXT("axis1d") || ValueType == TEXT("1") ||
        ValueType == TEXT("axis2d") || ValueType == TEXT("2") ||
        ValueType == TEXT("axis3d") || ValueType == TEXT("3");
    if (!bValidValueType)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid valueType '%s'. Use digital, axis1d, axis2d, or axis3d."), *ValueType),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        UInputAction* ExistingAction = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(FullPath));
        if (!ExistingAction)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s but is not an InputAction"), *FullPath),
                TEXT("ASSET_TYPE_MISMATCH"));
            return true;
        }

        // Existing asset: apply a newly supplied valueType so an action created
        // before this param existed can be upgraded in place.
        if (!ValueType.IsEmpty())
        {
            const int32 RequestedValueType =
                ValueType == TEXT("axis1d") || ValueType == TEXT("1") ? 1 :
                ValueType == TEXT("axis2d") || ValueType == TEXT("2") ? 2 :
                ValueType == TEXT("axis3d") || ValueType == TEXT("3") ? 3 : 0;
            if (ExistingAction->ValueType != static_cast<EInputActionValueType>(RequestedValueType))
            {
                ExistingAction->Modify();
                ExistingAction->ValueType = static_cast<EInputActionValueType>(RequestedValueType);
                SaveLoadedAssetThrottled(ExistingAction, -1.0, true);
            }
        }

        return SendExistingInputAssetResponse(
            Bridge, RequestingSocket, RequestId, ExistingAction, TEXT("Input Action already exists."));
    }

    IAssetTools& AssetTools = FModuleManager::Get()
        .LoadModuleChecked<FAssetToolsModule>("AssetTools")
        .Get();
    UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, UInputAction::StaticClass(), nullptr);
    UInputAction* NewAction = Cast<UInputAction>(NewAsset);
    if (NewAction && !ValueType.IsEmpty())
    {
        const int32 RequestedValueType =
            ValueType == TEXT("axis1d") || ValueType == TEXT("1") ? 1 :
            ValueType == TEXT("axis2d") || ValueType == TEXT("2") ? 2 :
            ValueType == TEXT("axis3d") || ValueType == TEXT("3") ? 3 : 0;
        NewAction->ValueType = static_cast<EInputActionValueType>(RequestedValueType);
    }
    return SaveNewInputAssetResponse(
        Bridge, RequestingSocket, RequestId, NewAsset,
        TEXT("Input Action created."), TEXT("Failed to create Input Action."));
}

bool HandleCreateInputMappingContext(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    FString SanitizedPath;
    if (!ValidateInputAssetNameAndPath(Bridge, RequestingSocket, RequestId, Name, Path, SanitizedPath))
    {
        return true;
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        UInputMappingContext* ExistingContext = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(FullPath));
        if (!ExistingContext)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s but is not an InputMappingContext"), *FullPath),
                TEXT("ASSET_TYPE_MISMATCH"));
            return true;
        }

        return SendExistingInputAssetResponse(
            Bridge, RequestingSocket, RequestId, ExistingContext, TEXT("Input Mapping Context already exists."));
    }

    IAssetTools& AssetTools = FModuleManager::Get()
        .LoadModuleChecked<FAssetToolsModule>("AssetTools")
        .Get();
    UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, UInputMappingContext::StaticClass(), nullptr);
    return SaveNewInputAssetResponse(
        Bridge, RequestingSocket, RequestId, NewAsset,
        TEXT("Input Mapping Context created."), TEXT("Failed to create Input Mapping Context."));
}
#endif
}
