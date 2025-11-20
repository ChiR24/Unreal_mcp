#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"

#include "Misc/Paths.h"

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (Lower.IsEmpty())
    {
        return false;
    }

    return false;
}

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "AssetViewUtils.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/ObjectRedirector.h"
#endif

// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase))
    {
        // Not our action — allow other handlers to try
        return false;
    }

    // Implementation of redirector fixup functionality
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("fixup_redirectors payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Get optional directory path (if empty, fix all redirectors)
    FString DirectoryPath;
    Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
    
    bool bCheckoutFiles = false;
    Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, DirectoryPath, bCheckoutFiles, RequestingSocket]()
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        // Find all redirectors
        FARFilter Filter;
        Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
        
        if (!DirectoryPath.IsEmpty())
        {
            FString NormalizedPath = DirectoryPath;
            if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase))
            {
                NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
            }
            Filter.PackagePaths.Add(FName(*NormalizedPath));
            Filter.bRecursivePaths = true;
        }

        TArray<FAssetData> RedirectorAssets;
        AssetRegistry.GetAssets(Filter, RedirectorAssets);

        if (RedirectorAssets.Num() == 0)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            Result->SetNumberField(TEXT("redirectorsFound"), 0);
            Result->SetNumberField(TEXT("redirectorsFixed"), 0);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No redirectors found"), Result, FString());
            return;
        }

        // Convert to string paths for AssetTools
        TArray<FString> RedirectorPaths;
        for (const FAssetData& Asset : RedirectorAssets)
        {
            RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
        }

        // Checkout files if source control is enabled
        if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled())
        {
            ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
            TArray<FString> PackageNames;
            for (const FAssetData& Asset : RedirectorAssets)
            {
                PackageNames.Add(Asset.PackageName.ToString());
            }
            SourceControlHelpers::CheckOutFiles(PackageNames, true);
        }

        // Convert FAssetData to UObjectRedirector* for AssetTools
        TArray<UObjectRedirector*> Redirectors;
        for (const FAssetData& Asset : RedirectorAssets)
        {
            if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset()))
            {
                Redirectors.Add(Redirector);
            }
        }
        
        // Fixup redirectors using AssetTools
        if (Redirectors.Num() > 0)
        {
            IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
            AssetTools.FixupReferencers(Redirectors);
        }

        // Delete the now-unused redirectors
        int32 DeletedCount = 0;
        TArray<UObject*> ObjectsToDelete;
        for (const FAssetData& Asset : RedirectorAssets)
        {
            if (UObject* Obj = Asset.GetAsset())
            {
                ObjectsToDelete.Add(Obj);
            }
        }
        
        if (ObjectsToDelete.Num() > 0)
        {
            DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
        Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result, FString());
    });

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("fixup_redirectors requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) && !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase))
    {
        return false;
    }
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("source_control_checkout payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) || !AssetPathsArray || AssetPathsArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("assetPaths array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<FString> AssetPaths;
    for (const TSharedPtr<FJsonValue>& Val : *AssetPathsArray)
    {
        if (Val.IsValid() && Val->Type == EJson::String)
        {
            AssetPaths.Add(Val->AsString());
        }
    }

    if (!ISourceControlModule::Get().IsEnabled())
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Source control is not enabled"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Source control disabled"), Result, TEXT("SOURCE_CONTROL_DISABLED"));
        return true;
    }

    ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

    TArray<FString> PackageNames;
    TArray<FString> ValidPaths;
    for (const FString& Path : AssetPaths)
    {
        if (UEditorAssetLibrary::DoesAssetExist(Path))
        {
            ValidPaths.Add(Path);
            FString PackageName = FPackageName::ObjectPathToPackageName(Path);
            PackageNames.Add(PackageName);
        }
    }

    if (PackageNames.Num() == 0)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No valid assets"), Result, TEXT("NO_VALID_ASSETS"));
        return true;
    }

    bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

    TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
    for (const FString& Path : ValidPaths)
    {
        CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
    Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
        bSuccess ? TEXT("Assets checked out successfully") : TEXT("Checkout failed"),
        Result, bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("source_control_checkout requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) && !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase))
    {
        return false;
    }
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("source_control_submit payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) || !AssetPathsArray || AssetPathsArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("assetPaths array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Description;
    if (!Payload->TryGetStringField(TEXT("description"), Description) || Description.IsEmpty())
    {
        Description = TEXT("Automated submission via MCP Automation Bridge");
    }

    TArray<FString> AssetPaths;
    for (const TSharedPtr<FJsonValue>& Val : *AssetPathsArray)
    {
        if (Val.IsValid() && Val->Type == EJson::String)
        {
            AssetPaths.Add(Val->AsString());
        }
    }

    if (!ISourceControlModule::Get().IsEnabled())
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Source control is not enabled"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Source control disabled"), Result, TEXT("SOURCE_CONTROL_DISABLED"));
        return true;
    }

    ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

    TArray<FString> PackageNames;
    for (const FString& Path : AssetPaths)
    {
        if (UEditorAssetLibrary::DoesAssetExist(Path))
        {
            FString PackageName = FPackageName::ObjectPathToPackageName(Path);
            PackageNames.Add(PackageName);
        }
    }

    if (PackageNames.Num() == 0)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No valid assets"), Result, TEXT("NO_VALID_ASSETS"));
        return true;
    }

    TArray<FString> FilePaths;
    for (const FString& PackageName : PackageNames)
    {
        FString FilePath;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, FilePath, FPackageName::GetAssetPackageExtension()))
        {
            FilePaths.Add(FilePath);
        }
    }

    TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
    CheckInOperation->SetDescription(FText::FromString(Description));

    ECommandResult::Type Result = SourceControlProvider.Execute(CheckInOperation, FilePaths);
    bool bSuccess = (Result == ECommandResult::Succeeded);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetNumberField(TEXT("submitted"), bSuccess ? PackageNames.Num() : 0);
    ResultObj->SetStringField(TEXT("description"), Description);

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
        bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
        ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("source_control_submit requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// 4. BULK RENAME ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) && !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase))
    {
        return false;
    }
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bulk_rename payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) || !AssetPathsArray || AssetPathsArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("assetPaths array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Get rename options
    FString Prefix, Suffix, SearchText, ReplaceText;
    Payload->TryGetStringField(TEXT("prefix"), Prefix);
    Payload->TryGetStringField(TEXT("suffix"), Suffix);
    Payload->TryGetStringField(TEXT("searchText"), SearchText);
    Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

    bool bCheckoutFiles = false;
    Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

    TArray<FString> AssetPaths;
    for (const TSharedPtr<FJsonValue>& Val : *AssetPathsArray)
    {
        if (Val.IsValid() && Val->Type == EJson::String)
        {
            AssetPaths.Add(Val->AsString());
        }
    }

    TArray<FAssetRenameData> RenameData;

    for (const FString& AssetPath : AssetPaths)
    {
        if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            continue;
        }

        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset)
        {
            continue;
        }

        FString CurrentName = Asset->GetName();
        FString NewName = CurrentName;

        if (!SearchText.IsEmpty())
        {
            NewName = NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
        }

        if (!Prefix.IsEmpty())
        {
            NewName = Prefix + NewName;
        }
        if (!Suffix.IsEmpty())
        {
            NewName = NewName + Suffix;
        }

        if (NewName == CurrentName)
        {
            continue;
        }

        FString PackagePath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
        FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
        RenameData.Add(RenameEntry);
    }

    if (RenameData.Num() == 0)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("renamed"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets required renaming"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No renames needed"), Result, FString());
        return true;
    }

    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled())
    {
        TArray<FString> PackageNames;
        for (const FAssetRenameData& Data : RenameData)
        {
            PackageNames.Add(Data.Asset->GetOutermost()->GetName());
        }
        SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    bool bSuccess = AssetTools.RenameAssets(RenameData);

    TArray<TSharedPtr<FJsonValue>> RenamedAssets;
    for (const FAssetRenameData& Data : RenameData)
    {
        TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
        AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
        AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
        RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("renamed"), RenameData.Num());
    Result->SetArrayField(TEXT("assets"), RenamedAssets);

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
        bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num()) : TEXT("Bulk rename failed"),
        Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("bulk_rename requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) && !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase))
    {
        return false;
    }
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bulk_delete payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) || !AssetPathsArray || AssetPathsArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("assetPaths array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    bool bShowConfirmation = false;
    Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);
    
    bool bFixupRedirectors = true;
    Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

    TArray<FString> AssetPaths;
    for (const TSharedPtr<FJsonValue>& Val : *AssetPathsArray)
    {
        if (Val.IsValid() && Val->Type == EJson::String)
        {
            AssetPaths.Add(Val->AsString());
        }
    }

    TArray<UObject*> ObjectsToDelete;
    TArray<FString> ValidPaths;

    for (const FString& AssetPath : AssetPaths)
    {
        if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            if (UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath))
            {
                ObjectsToDelete.Add(Asset);
                ValidPaths.Add(AssetPath);
            }
        }
    }

    if (ObjectsToDelete.Num() == 0)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No valid assets"), Result, TEXT("NO_VALID_ASSETS"));
        return true;
    }

    int32 DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

    if (bFixupRedirectors && DeletedCount > 0)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        FARFilter Filter;
        Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));

        TArray<FAssetData> RedirectorAssets;
        AssetRegistry.GetAssets(Filter, RedirectorAssets);

        if (RedirectorAssets.Num() > 0)
        {
            TArray<UObjectRedirector*> Redirectors;
            for (const FAssetData& Asset : RedirectorAssets)
            {
                if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset()))
                {
                    Redirectors.Add(Redirector);
                }
            }

            if (Redirectors.Num() > 0)
            {
                IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
                AssetTools.FixupReferencers(Redirectors);
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> DeletedAssets;
    for (const FString& Path : ValidPaths)
    {
        DeletedAssets.Add(MakeShared<FJsonValueString>(Path));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), DeletedCount > 0);
    Result->SetNumberField(TEXT("deleted"), DeletedCount);
    Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());
    Result->SetArrayField(TEXT("assets"), DeletedAssets);

    SendAutomationResponse(RequestingSocket, RequestId, DeletedCount > 0,
        FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount, ObjectsToDelete.Num()),
        Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("bulk_delete requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("generate_thumbnail"), ESearchCase::IgnoreCase))
    {
        return false;
    }
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("generate_thumbnail payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    int32 Width = 512;
    int32 Height = 512;
    
    double TempWidth = 0, TempHeight = 0;
    if (Payload->TryGetNumberField(TEXT("width"), TempWidth)) Width = static_cast<int32>(TempWidth);
    if (Payload->TryGetNumberField(TEXT("height"), TempHeight)) Height = static_cast<int32>(TempHeight);

    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Asset not found"), nullptr, TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load asset"), nullptr, TEXT("LOAD_FAILED"));
        return true;
    }

    FObjectThumbnail ObjectThumbnail;
    ThumbnailTools::RenderThumbnail(Asset, Width, Height, ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &ObjectThumbnail);

    bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 && ObjectThumbnail.GetImageHeight() > 0;

    if (bSuccess && !OutputPath.IsEmpty())
    {
        const TArray<uint8>& ImageData = ObjectThumbnail.GetUncompressedImageData();

        if (ImageData.Num() > 0)
        {
            TArray<FColor> ColorData;
            ColorData.Reserve(Width * Height);

            for (int32 i = 0; i < ImageData.Num(); i += 4)
            {
                FColor Color;
                Color.B = ImageData[i + 0];
                Color.G = ImageData[i + 1];
                Color.R = ImageData[i + 2];
                Color.A = ImageData[i + 3];
                ColorData.Add(Color);
            }

            FString AbsolutePath = OutputPath;
            if (FPaths::IsRelative(OutputPath))
            {
                AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputPath);
            }

            TArray<uint8> CompressedData;
            FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData, CompressedData);
            bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
        }
    }

    if (Asset->GetOutermost())
    {
        Asset->GetOutermost()->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);

    if (!OutputPath.IsEmpty())
    {
        Result->SetStringField(TEXT("outputPath"), OutputPath);
    }

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
        bSuccess ? TEXT("Thumbnail generated successfully") : TEXT("Thumbnail generation failed"),
        Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("generate_thumbnail requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
