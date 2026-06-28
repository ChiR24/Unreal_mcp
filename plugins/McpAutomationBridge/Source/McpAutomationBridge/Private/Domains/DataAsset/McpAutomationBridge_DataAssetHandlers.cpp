// McpAutomationBridge_DataAssetHandlers.cpp
// General-purpose UDataAsset / UPrimaryDataAsset management:
// - create_data_asset: Create an instance of a data asset class
// - create_data_asset_blueprint: Create a Blueprint subclass of UPrimaryDataAsset
// - get_data_asset_properties: Read all UPROPERTY values from a data asset
// - set_data_asset_properties: Write UPROPERTY values on a data asset
// - list_data_assets: List data assets by class or path
// - duplicate_data_asset: Duplicate an existing data asset

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "AssetToolsModule.h"
#include "JsonObjectConverter.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UnrealType.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpDataAssetHandlers, Log, All);

// ============================================================================
// Sub-Action Handlers
// ============================================================================

#if WITH_EDITOR

// ----------------------------------------------------------------------------
// create_data_asset
// Creates an instance of a data asset class (UDataAsset-derived).
// Uses UMcpGenericDataAsset if no className specified.
// ----------------------------------------------------------------------------
static bool HandleCreateDataAsset(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT("/Game/DataAssets"));
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString ClassName = GetJsonStringField(Payload, TEXT("className"), TEXT(""));

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Determine the class to instantiate
    UClass* AssetClass = nullptr;
    if (!ClassName.IsEmpty())
    {
        AssetClass = FindObject<UClass>(nullptr, *ClassName);
        if (!AssetClass)
        {
            AssetClass = LoadObject<UClass>(nullptr, *ClassName);
        }
        if (!AssetClass)
        {
            // Try common short names
            FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            AssetClass = FindObject<UClass>(nullptr, *FullPath);
        }
        if (!AssetClass)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Class not found: %s"), *ClassName),
                nullptr, TEXT("NOT_FOUND"));
            return true;
        }
        if (!AssetClass->IsChildOf(UDataAsset::StaticClass()))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Class '%s' does not derive from UDataAsset"), *ClassName),
                nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    else
    {
        // Default to UMcpGenericDataAsset
        AssetClass = UMcpGenericDataAsset::StaticClass();
    }

    // Validate the creation path
    FString FullPath, PathError;
    if (!ValidateAssetCreationPath(FolderPath, AssetName, FullPath, PathError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *PathError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Create via AssetTools - use folder path directly to support plugin mount points (e.g. /Canopy/)
    FString SanitizedFolder = FolderPath;
    if (SanitizedFolder.IsEmpty())
    {
        SanitizedFolder = TEXT("/Game/DataAssets");
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // Use DataAssetFactory if available, otherwise create directly
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>(GetTransientPackage());
    // DataAssetFactory in some UE versions may not set the class correctly
    // We'll create directly via NewObject for reliability
    FString SanitizedName = SanitizeAssetName(AssetName);
    UPackage* Package = CreateValidatedAssetPackage(SanitizedFolder, SanitizedName, PathError);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
            nullptr, TEXT("PACKAGE_CREATE_FAILED"));
        return true;
    }

    UDataAsset* NewAsset = NewObject<UDataAsset>(Package, AssetClass, FName(*SanitizedName),
        RF_Public | RF_Standalone);

    if (!NewAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create data asset"), nullptr);
        return true;
    }

    // Set properties if provided
    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj && (*PropsObj).IsValid())
    {
        for (const auto& Pair : (*PropsObj)->Values)
        {
            FProperty* Property = NewAsset->GetClass()->FindPropertyByName(FName(*Pair.Key));
            if (!Property)
            {
                UE_LOG(LogMcpDataAssetHandlers, Warning, TEXT("Property '%s' not found on class '%s'"),
                    *Pair.Key, *AssetClass->GetName());
                continue;
            }

            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NewAsset);
            FString ValueStr;
            if (Pair.Value->Type == EJson::String)
            {
                ValueStr = Pair.Value->AsString();
            }
            else if (Pair.Value->Type == EJson::Number)
            {
                ValueStr = FString::SanitizeFloat(Pair.Value->AsNumber());
            }
            else if (Pair.Value->Type == EJson::Boolean)
            {
                ValueStr = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
            }

            if (!ValueStr.IsEmpty())
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Property->ImportText_Direct(*ValueStr, ValuePtr, NewAsset, PPF_None);
#else
                Property->ImportText(*ValueStr, ValuePtr, PPF_None, NewAsset);
#endif
            }
        }
    }

    NewAsset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewAsset);
    McpSafeAssetSave(NewAsset);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    ResultJson->SetStringField(TEXT("assetName"), SanitizedName);
    ResultJson->SetStringField(TEXT("className"), AssetClass->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created data asset: %s"), *NewAsset->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// create_data_asset_blueprint
// Create a Blueprint subclass of UPrimaryDataAsset (or specified parent).
// ----------------------------------------------------------------------------
static bool HandleCreateDataAssetBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT("/Game/DataAssets"));
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString ParentClassName = GetJsonStringField(Payload, TEXT("parentClass"), TEXT(""));

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Determine parent class
    UClass* ParentClass = UPrimaryDataAsset::StaticClass();
    if (!ParentClassName.IsEmpty())
    {
        UClass* FoundClass = FindObject<UClass>(nullptr, *ParentClassName);
        if (!FoundClass)
        {
            FoundClass = LoadObject<UClass>(nullptr, *ParentClassName);
        }
        if (!FoundClass)
        {
            FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassName);
            FoundClass = FindObject<UClass>(nullptr, *FullPath);
        }
        if (FoundClass)
        {
            ParentClass = FoundClass;
        }
        else
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Parent class not found: %s"), *ParentClassName),
                nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    FString SanitizedFolder = SanitizeProjectRelativePath(FolderPath);
    if (SanitizedFolder.IsEmpty())
    {
        SanitizedFolder = TEXT("/Game/DataAssets");
    }

    // Create Blueprint via factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>(GetTransientPackage());
    Factory->ParentClass = ParentClass;

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, SanitizedFolder, UBlueprint::StaticClass(), Factory);

    if (!NewAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create data asset Blueprint"), nullptr);
        return true;
    }

    McpSafeAssetSave(NewAsset);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    ResultJson->SetStringField(TEXT("assetName"), AssetName);
    ResultJson->SetStringField(TEXT("parentClass"), ParentClass->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created data asset Blueprint: %s"), *NewAsset->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// get_data_asset_properties
// Read all UPROPERTY values from a data asset.
// ----------------------------------------------------------------------------
static bool HandleGetDataAssetProperties(
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

    // Use UEditorAssetLibrary::LoadAsset which handles plugin mount points (e.g. /Canopy/)
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset || !Asset->IsA(UDataAsset::StaticClass()))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Data asset not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Iterate all UPROPERTY fields and export their values
    TSharedPtr<FJsonObject> PropsJson = MakeShareable(new FJsonObject());
    for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;

        // Skip properties from UObject base class
        if (Property->GetOwnerClass() == UObject::StaticClass())
        {
            continue;
        }

        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);
        FString ValueStr;
        MCP_PROPERTY_EXPORT_TEXT(Property, ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
        PropsJson->SetStringField(Property->GetName(), ValueStr);
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("className"), Asset->GetClass()->GetPathName());
    ResultJson->SetObjectField(TEXT("properties"), PropsJson);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Retrieved properties for: %s"), *Asset->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// set_data_asset_properties
// Write UPROPERTY values on a data asset.
// ----------------------------------------------------------------------------
static bool HandleSetDataAssetProperties(
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

    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (!Payload->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj || !(*PropsObj).IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("properties object is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Use UEditorAssetLibrary::LoadAsset which handles plugin mount points (e.g. /Canopy/)
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset || !Asset->IsA(UDataAsset::StaticClass()))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Data asset not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    int32 PropertiesSet = 0;
    int32 PropertiesFailed = 0;
    TArray<FString> FailedNames;

    for (const auto& Pair : (*PropsObj)->Values)
    {
        FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*Pair.Key));
        if (!Property)
        {
            PropertiesFailed++;
            FailedNames.Add(Pair.Key);
            continue;
        }

        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);

        bool bSetSucceeded = false;

        // For array/struct properties with JSON array/object values, use FJsonObjectConverter
        // which handles nested structs, soft object references, and complex types properly.
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
        FStructProperty* StructProp = CastField<FStructProperty>(Property);

        if (ArrayProp && Pair.Value->Type == EJson::Array)
        {
            // Convert JSON array to UE array property using FJsonObjectConverter.
            // Wrap in a temporary JSON object since JsonObjectToUStruct expects an object.
            TSharedPtr<FJsonObject> WrapperObj = MakeShared<FJsonObject>();
            WrapperObj->SetField(Pair.Key, Pair.Value);
            bSetSucceeded = FJsonObjectConverter::JsonObjectToUStruct(WrapperObj.ToSharedRef(), Asset->GetClass(), Asset, 0, 0);
        }
        else if (StructProp && Pair.Value->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> StructJson = Pair.Value->AsObject();
            bSetSucceeded = FJsonObjectConverter::JsonObjectToUStruct(StructJson.ToSharedRef(), StructProp->Struct, ValuePtr, 0, 0);
        }
        else
        {
            // Simple types: use ImportText
            FString ValueStr;
            if (Pair.Value->Type == EJson::String)
            {
                ValueStr = Pair.Value->AsString();
            }
            else if (Pair.Value->Type == EJson::Number)
            {
                ValueStr = FString::SanitizeFloat(Pair.Value->AsNumber());
            }
            else if (Pair.Value->Type == EJson::Boolean)
            {
                ValueStr = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
            }
            else
            {
                TSharedRef<FJsonValue> ValRef = Pair.Value.ToSharedRef();
                FString JsonStr;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
                FJsonSerializer::Serialize(ValRef, TEXT(""), Writer);
                Writer->Close();
                ValueStr = JsonStr;
            }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            const TCHAR* ImportResult = Property->ImportText_Direct(*ValueStr, ValuePtr, Asset, PPF_None);
#else
            const TCHAR* ImportResult = Property->ImportText(*ValueStr, ValuePtr, PPF_None, Asset);
#endif
            bSetSucceeded = (ImportResult != nullptr);
        }

        if (bSetSucceeded)
        {
            PropertiesSet++;
        }
        else
        {
            PropertiesFailed++;
            FailedNames.Add(Pair.Key);
        }
    }

    if (PropertiesSet > 0)
    {
        Asset->MarkPackageDirty();
        McpSafeAssetSave(Asset);
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
    ResultJson->SetNumberField(TEXT("propertiesFailed"), PropertiesFailed);
    if (FailedNames.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> FailedArray;
        for (const FString& Name : FailedNames)
        {
            FailedArray.Add(MakeShareable(new FJsonValueString(Name)));
        }
        ResultJson->SetArrayField(TEXT("failedProperties"), FailedArray);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %d properties on %s (%d failed)"),
            PropertiesSet, *Asset->GetPathName(), PropertiesFailed), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_data_assets
// List data assets by class or path.
// ----------------------------------------------------------------------------
static bool HandleListDataAssets(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Filter = GetJsonStringField(Payload, TEXT("filter"), TEXT(""));
    FString SearchPath = GetJsonStringField(Payload, TEXT("searchPath"), TEXT("/Game"));

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Determine filter class
    UClass* FilterClass = UDataAsset::StaticClass();
    if (!Filter.IsEmpty())
    {
        UClass* FoundClass = FindObject<UClass>(nullptr, *Filter);
        if (!FoundClass)
        {
            FoundClass = LoadObject<UClass>(nullptr, *Filter);
        }
        if (FoundClass && FoundClass->IsChildOf(UDataAsset::StaticClass()))
        {
            FilterClass = FoundClass;
        }
    }

    // Accept plugin mount points (e.g. /Canopy/) directly - only fallback to /Game if empty
    FString ResolvedPath = SearchPath;
    if (ResolvedPath.IsEmpty())
    {
        ResolvedPath = TEXT("/Game");
    }

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssetsByPath(FName(*ResolvedPath), AssetList, true);

    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData& AssetData : AssetList)
    {
        // Filter to DataAsset-derived classes
        UClass* AssetDataClass = AssetData.GetClass();
        if (!AssetDataClass || !AssetDataClass->IsChildOf(FilterClass))
        {
            // Try loading the class for blueprint-generated assets
            if (AssetData.AssetClassPath.GetAssetName() != UDataAsset::StaticClass()->GetFName())
            {
                continue;
            }
        }

        TSharedPtr<FJsonObject> AssetJson = MakeShareable(new FJsonObject());
        AssetJson->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
        AssetJson->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        AssetJson->SetStringField(TEXT("className"), AssetData.AssetClassPath.ToString());
        AssetsArray.Add(MakeShareable(new FJsonValueObject(AssetJson)));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("assets"), AssetsArray);
    ResultJson->SetNumberField(TEXT("count"), AssetsArray.Num());
    ResultJson->SetStringField(TEXT("searchPath"), ResolvedPath);
    if (!Filter.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("filter"), Filter);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d data assets"), AssetsArray.Num()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// duplicate_data_asset
// Duplicate an existing data asset.
// ----------------------------------------------------------------------------
static bool HandleDuplicateDataAsset(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString NewName = GetJsonStringField(Payload, TEXT("newName"), TEXT(""));
    FString NewPath = GetJsonStringField(Payload, TEXT("newPath"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (NewName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("newName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Use UEditorAssetLibrary::LoadAsset which handles plugin mount points (e.g. /Canopy/)
    UObject* SourceAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!SourceAsset || !SourceAsset->IsA(UDataAsset::StaticClass()))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Source data asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Determine destination
    FString DestFolder = NewPath.IsEmpty()
        ? FPackageName::GetLongPackagePath(SourceAsset->GetPathName())
        : SanitizeProjectRelativePath(NewPath);

    if (DestFolder.IsEmpty())
    {
        DestFolder = TEXT("/Game");
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    TArray<FAssetRenameData> RenameData;
    // Use DuplicateAsset
    UObject* DuplicatedAsset = AssetTools.DuplicateAsset(NewName, DestFolder, SourceAsset);

    if (!DuplicatedAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to duplicate data asset"), nullptr);
        return true;
    }

    McpSafeAssetSave(DuplicatedAsset);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), DuplicatedAsset->GetPathName());
    ResultJson->SetStringField(TEXT("assetName"), NewName);
    ResultJson->SetStringField(TEXT("sourcePath"), SourceAsset->GetPathName());
    ResultJson->SetStringField(TEXT("className"), DuplicatedAsset->GetClass()->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Duplicated data asset: %s"), *DuplicatedAsset->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// get_curve_keys / set_curve_keys
// Read/write keys on UCurveFloat, UCurveVector, UCurveLinearColor assets.
// ----------------------------------------------------------------------------
static bool HandleGetCurveKeys(
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

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    UCurveFloat* CurveFloat = Cast<UCurveFloat>(Asset);
    if (!CurveFloat)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Curve asset not found or not a CurveFloat: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> KeysArray;
    for (const FRichCurveKey& Key : CurveFloat->FloatCurve.GetConstRefOfKeys())
    {
        TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
        KeyObj->SetNumberField(TEXT("time"), Key.Time);
        KeyObj->SetNumberField(TEXT("value"), Key.Value);
        KeyObj->SetNumberField(TEXT("arriveTangent"), Key.ArriveTangent);
        KeyObj->SetNumberField(TEXT("leaveTangent"), Key.LeaveTangent);
        FString InterpMode;
        switch (Key.InterpMode)
        {
            case RCIM_Linear: InterpMode = TEXT("Linear"); break;
            case RCIM_Constant: InterpMode = TEXT("Constant"); break;
            case RCIM_Cubic: InterpMode = TEXT("Cubic"); break;
            default: InterpMode = TEXT("Linear"); break;
        }
        KeyObj->SetStringField(TEXT("interpMode"), InterpMode);
        KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), CurveFloat->GetPathName());
    Result->SetArrayField(TEXT("keys"), KeysArray);
    Result->SetNumberField(TEXT("keyCount"), KeysArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Got %d curve keys"), KeysArray.Num()), Result, FString());
    return true;
}

static bool HandleSetCurveKeys(
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

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    UCurveFloat* CurveFloat = Cast<UCurveFloat>(Asset);
    if (!CurveFloat)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Curve asset not found or not a CurveFloat: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* KeysArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("keys"), KeysArray) || !KeysArray)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("keys array is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Check if we should append or replace
    bool bAppend = false;
    Payload->TryGetBoolField(TEXT("append"), bAppend);

    if (!bAppend)
    {
        CurveFloat->FloatCurve.Reset();
    }

    int32 KeysAdded = 0;
    for (const TSharedPtr<FJsonValue>& KeyVal : *KeysArray)
    {
        if (!KeyVal.IsValid() || KeyVal->Type != EJson::Object) continue;
        TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();

        double Time = 0.0, Value = 0.0;
        KeyObj->TryGetNumberField(TEXT("time"), Time);
        if (!KeyObj->TryGetNumberField(TEXT("value"), Value))
        {
            // Also accept "v" as shorthand
            KeyObj->TryGetNumberField(TEXT("v"), Value);
        }

        FKeyHandle Handle = CurveFloat->FloatCurve.AddKey(static_cast<float>(Time), static_cast<float>(Value));

        // Set interpolation mode if specified
        FString InterpMode = GetJsonStringField(KeyObj, TEXT("interpMode"), TEXT(""));
        if (!InterpMode.IsEmpty())
        {
            ERichCurveInterpMode Mode = RCIM_Linear;
            if (InterpMode.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
                Mode = RCIM_Constant;
            else if (InterpMode.Equals(TEXT("Cubic"), ESearchCase::IgnoreCase))
                Mode = RCIM_Cubic;
            CurveFloat->FloatCurve.SetKeyInterpMode(Handle, Mode);
        }

        // Set tangents if specified
        double ArriveTangent = 0.0, LeaveTangent = 0.0;
        if (KeyObj->TryGetNumberField(TEXT("arriveTangent"), ArriveTangent) ||
            KeyObj->TryGetNumberField(TEXT("leaveTangent"), LeaveTangent))
        {
            FRichCurveKey& AddedKey = CurveFloat->FloatCurve.GetKey(Handle);
            AddedKey.ArriveTangent = static_cast<float>(ArriveTangent);
            AddedKey.LeaveTangent = static_cast<float>(LeaveTangent);
        }

        KeysAdded++;
    }

    CurveFloat->MarkPackageDirty();
    McpSafeAssetSave(CurveFloat);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), CurveFloat->GetPathName());
    Result->SetNumberField(TEXT("keysAdded"), KeysAdded);
    Result->SetNumberField(TEXT("totalKeys"), CurveFloat->FloatCurve.GetNumKeys());
    Result->SetBoolField(TEXT("appended"), bAppend);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %d curve keys (total: %d)"), KeysAdded, CurveFloat->FloatCurve.GetNumKeys()),
        Result, FString());
    return true;
}

#endif // WITH_EDITOR

// ----------------------------------------------------------------------------
// Array mutation helpers (shared across append / insert / remove / update)
// ----------------------------------------------------------------------------

#if WITH_EDITOR

// Resolve a data asset and its TArray<...> property.
// Returns false and sends an error response on failure.
static bool ResolveArrayProperty(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload,
    UObject*& OutAsset,
    FArrayProperty*& OutArrayProp,
    void*& OutArrayContainer)
{
    OutAsset = nullptr;
    OutArrayProp = nullptr;
    OutArrayContainer = nullptr;

    const FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    const FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT(""));

    if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath and propertyName are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return false;
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset || !Asset->IsA(UDataAsset::StaticClass()))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Data asset not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return false;
    }

    FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *Asset->GetClass()->GetName()),
            nullptr, TEXT("UNKNOWN_PROPERTY"));
        return false;
    }

    FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
    if (!ArrayProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Property '%s' is not a TArray (got %s)"), *PropertyName, *Property->GetClass()->GetName()),
            nullptr, TEXT("NOT_AN_ARRAY"));
        return false;
    }

    OutAsset = Asset;
    OutArrayProp = ArrayProp;
    OutArrayContainer = Property->ContainerPtrToValuePtr<void>(Asset);
    return true;
}

// Write a JSON value into an array element's memory.
// Handles structs (via FJsonObjectConverter), object / class / soft references
// (loaded from path string), and primitives (via ImportText).
static bool WriteJsonValueIntoArrayElement(
    FProperty* InnerProp,
    void* ElemPtr,
    const TSharedPtr<FJsonValue>& Value)
{
    if (!InnerProp || !ElemPtr || !Value.IsValid())
    {
        return false;
    }

    FStructProperty* StructProp = CastField<FStructProperty>(InnerProp);
    if (StructProp && Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), StructProp->Struct, ElemPtr, 0, 0);
    }

    // TSubclassOf / UClass* inner: load the class from a path string, accepting
    // both with-_C and without-_C forms, and Blueprint asset paths (resolved to
    // GeneratedClass). ImportText_Direct on FClassProperty silently produces
    // None when the path can't be parsed in object-export syntax, which is what
    // the consumer was hitting.
    if (FClassProperty* ClassProp = CastField<FClassProperty>(InnerProp))
    {
        if (Value->Type != EJson::String) return false;
        const FString Path = Value->AsString();
        if (Path.IsEmpty())
        {
            ClassProp->SetObjectPropertyValue(ElemPtr, nullptr);
            return true;
        }
        UClass* Loaded = LoadObject<UClass>(nullptr, *Path);
        if (!Loaded)
        {
            const FString WithC = Path.EndsWith(TEXT("_C")) ? Path : (Path + TEXT("_C"));
            Loaded = LoadObject<UClass>(nullptr, *WithC);
        }
        if (!Loaded)
        {
            FString WithoutC = Path;
            if (WithoutC.EndsWith(TEXT("_C"))) WithoutC.LeftChopInline(2);
            if (UObject* Obj = LoadObject<UObject>(nullptr, *WithoutC))
            {
                if (UBlueprint* BP = Cast<UBlueprint>(Obj))
                {
                    Loaded = BP->GeneratedClass;
                }
            }
        }
        if (!Loaded) return false;
        if (ClassProp->MetaClass && !Loaded->IsChildOf(ClassProp->MetaClass)) return false;
        ClassProp->SetObjectPropertyValue(ElemPtr, Loaded);
        return true;
    }

    // TObjectPtr<UAsset> / UObject* inner: load the asset and assign.
    // Accepts bare paths ("/Game/Foo.Foo") or class-prefixed object-export form;
    // tries both shapes before giving up.
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(InnerProp))
    {
        if (Value->Type != EJson::String) return false;
        const FString Raw = Value->AsString();
        if (Raw.IsEmpty())
        {
            ObjProp->SetObjectPropertyValue(ElemPtr, nullptr);
            return true;
        }
        // Strip a class-prefixed wrapper if present: ClassPath'/Game/...'
        FString CleanPath = Raw;
        int32 QuoteStart = INDEX_NONE;
        if (CleanPath.FindChar(TEXT('\''), QuoteStart) && CleanPath.EndsWith(TEXT("'")))
        {
            CleanPath = CleanPath.Mid(QuoteStart + 1, CleanPath.Len() - QuoteStart - 2);
        }
        UObject* Loaded = LoadObject<UObject>(nullptr, *CleanPath);
        if (!Loaded)
        {
            Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *CleanPath);
        }
        if (!Loaded && CleanPath != Raw)
        {
            // Fallback: try the original (unstripped) string.
            Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *Raw);
        }
        if (!Loaded) return false;
        if (ObjProp->PropertyClass && !Loaded->IsA(ObjProp->PropertyClass)) return false;
        ObjProp->SetObjectPropertyValue(ElemPtr, Loaded);
        return true;
    }

    // FSoftClassProperty: store as soft path, don't force load.
    if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(InnerProp))
    {
        if (Value->Type != EJson::String) return false;
        const FSoftObjectPath SoftPath(Value->AsString());
        *static_cast<FSoftObjectPtr*>(ElemPtr) = FSoftObjectPtr(SoftPath);
        return true;
    }

    // FSoftObjectProperty: store as soft path.
    if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(InnerProp))
    {
        if (Value->Type != EJson::String) return false;
        const FSoftObjectPath SoftPath(Value->AsString());
        *static_cast<FSoftObjectPtr*>(ElemPtr) = FSoftObjectPtr(SoftPath);
        return true;
    }

    FString ValueStr;
    if (Value->Type == EJson::String)
    {
        ValueStr = Value->AsString();
    }
    else if (Value->Type == EJson::Number)
    {
        ValueStr = FString::SanitizeFloat(Value->AsNumber());
    }
    else if (Value->Type == EJson::Boolean)
    {
        ValueStr = Value->AsBool() ? TEXT("True") : TEXT("False");
    }
    else
    {
        // Object / array fallback: serialize raw JSON for ImportText to consume.
        TSharedRef<FJsonValue> ValRef = Value.ToSharedRef();
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
        FJsonSerializer::Serialize(ValRef, TEXT(""), Writer);
        Writer->Close();
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const TCHAR* Result = InnerProp->ImportText_Direct(*ValueStr, ElemPtr, nullptr, PPF_None);
#else
    const TCHAR* Result = InnerProp->ImportText(*ValueStr, ElemPtr, PPF_None, nullptr);
#endif
    return Result != nullptr;
}

// Read a dotted property path on a struct element to its export-text representation.
// Example path: "Key.TagName" on FCanopySchemaEntry -> walks Key (FStructProperty) then TagName (FNameProperty).
static bool ReadDottedPropertyAsString(
    UStruct* OwningStruct,
    void* StructData,
    const FString& DottedPath,
    FString& OutValue)
{
    if (!OwningStruct || !StructData || DottedPath.IsEmpty())
    {
        return false;
    }

    TArray<FString> Parts;
    DottedPath.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty=*/true);
    if (Parts.Num() == 0) return false;

    UStruct* CurrentStruct = OwningStruct;
    void* CurrentPtr = StructData;

    for (int32 i = 0; i < Parts.Num(); ++i)
    {
        FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Parts[i]));
        if (!Prop) return false;
        void* PropPtr = Prop->ContainerPtrToValuePtr<void>(CurrentPtr);

        if (i == Parts.Num() - 1)
        {
            OutValue.Reset();
            Prop->ExportTextItem_Direct(OutValue, PropPtr, nullptr, nullptr, PPF_None);
            return true;
        }

        FStructProperty* StructProp = CastField<FStructProperty>(Prop);
        if (!StructProp) return false;
        CurrentStruct = StructProp->Struct;
        CurrentPtr = PropPtr;
    }
    return false;
}

// Locate the index of the first array element whose dotted property path matches MatchValue.
// Returns INDEX_NONE if not found. For simple-typed arrays without struct inner, only "" path against
// the element's exported text is supported (caller passes empty MatchKey to compare against the whole element).
static int32 FindMatchingArrayIndex(
    FArrayProperty* ArrayProp,
    void* ArrayContainer,
    const FString& MatchKey,
    const FString& MatchValue)
{
    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    FProperty* Inner = ArrayProp->Inner;
    if (!Inner) return INDEX_NONE;

    FStructProperty* InnerStruct = CastField<FStructProperty>(Inner);

    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        void* ElemPtr = Helper.GetRawPtr(i);
        FString Exported;

        if (MatchKey.IsEmpty())
        {
            Inner->ExportTextItem_Direct(Exported, ElemPtr, nullptr, nullptr, PPF_None);
        }
        else
        {
            if (!InnerStruct) continue;
            if (!ReadDottedPropertyAsString(InnerStruct->Struct, ElemPtr, MatchKey, Exported))
            {
                continue;
            }
        }

        if (Exported.Equals(MatchValue, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }

    return INDEX_NONE;
}

#endif // WITH_EDITOR

// ----------------------------------------------------------------------------
// append_array_item
// ----------------------------------------------------------------------------
static bool HandleAppendArrayItem(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UObject* Asset = nullptr;
    FArrayProperty* ArrayProp = nullptr;
    void* ArrayContainer = nullptr;
    if (!ResolveArrayProperty(Subsystem, RequestId, Socket, Payload, Asset, ArrayProp, ArrayContainer))
    {
        return true;
    }

    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("value is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    const int32 NewIndex = Helper.AddValue();
    void* ElemPtr = Helper.GetRawPtr(NewIndex);

    if (!WriteJsonValueIntoArrayElement(ArrayProp->Inner, ElemPtr, Value))
    {
        Helper.RemoveValues(NewIndex, 1);
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to deserialize value into array element"), nullptr, TEXT("INVALID_VALUE"));
        return true;
    }

    Asset->MarkPackageDirty();
    McpSafeAssetSave(Asset);

    TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("propertyName"), ArrayProp->GetName());
    ResultJson->SetNumberField(TEXT("index"), NewIndex);
    ResultJson->SetNumberField(TEXT("arrayLength"), Helper.Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Appended item to %s.%s at index %d"),
            *Asset->GetName(), *ArrayProp->GetName(), NewIndex), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// insert_array_item
// ----------------------------------------------------------------------------
static bool HandleInsertArrayItem(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UObject* Asset = nullptr;
    FArrayProperty* ArrayProp = nullptr;
    void* ArrayContainer = nullptr;
    if (!ResolveArrayProperty(Subsystem, RequestId, Socket, Payload, Asset, ArrayProp, ArrayContainer))
    {
        return true;
    }

    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("value is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    int32 Index = -1;
    if (!Payload->TryGetNumberField(TEXT("index"), Index))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("index is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    if (Index < 0 || Index > Helper.Num())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Index %d out of range [0, %d]"), Index, Helper.Num()),
            nullptr, TEXT("INDEX_OUT_OF_RANGE"));
        return true;
    }

    Helper.InsertValues(Index, 1);
    void* ElemPtr = Helper.GetRawPtr(Index);

    if (!WriteJsonValueIntoArrayElement(ArrayProp->Inner, ElemPtr, Value))
    {
        Helper.RemoveValues(Index, 1);
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to deserialize value into array element"), nullptr, TEXT("INVALID_VALUE"));
        return true;
    }

    Asset->MarkPackageDirty();
    McpSafeAssetSave(Asset);

    TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("propertyName"), ArrayProp->GetName());
    ResultJson->SetNumberField(TEXT("index"), Index);
    ResultJson->SetNumberField(TEXT("arrayLength"), Helper.Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Inserted item into %s.%s at index %d"),
            *Asset->GetName(), *ArrayProp->GetName(), Index), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_array_item_at
// ----------------------------------------------------------------------------
static bool HandleRemoveArrayItemAt(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UObject* Asset = nullptr;
    FArrayProperty* ArrayProp = nullptr;
    void* ArrayContainer = nullptr;
    if (!ResolveArrayProperty(Subsystem, RequestId, Socket, Payload, Asset, ArrayProp, ArrayContainer))
    {
        return true;
    }

    int32 Index = -1;
    if (!Payload->TryGetNumberField(TEXT("index"), Index))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("index is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    if (Index < 0 || Index >= Helper.Num())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Index %d out of range [0, %d)"), Index, Helper.Num()),
            nullptr, TEXT("INDEX_OUT_OF_RANGE"));
        return true;
    }

    Helper.RemoveValues(Index, 1);

    Asset->MarkPackageDirty();
    McpSafeAssetSave(Asset);

    TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("propertyName"), ArrayProp->GetName());
    ResultJson->SetNumberField(TEXT("removedIndex"), Index);
    ResultJson->SetNumberField(TEXT("arrayLength"), Helper.Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed %s.%s[%d]"),
            *Asset->GetName(), *ArrayProp->GetName(), Index), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_array_item_where
// ----------------------------------------------------------------------------
static bool HandleRemoveArrayItemWhere(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UObject* Asset = nullptr;
    FArrayProperty* ArrayProp = nullptr;
    void* ArrayContainer = nullptr;
    if (!ResolveArrayProperty(Subsystem, RequestId, Socket, Payload, Asset, ArrayProp, ArrayContainer))
    {
        return true;
    }

    const FString MatchKey = GetJsonStringField(Payload, TEXT("matchKey"), TEXT(""));
    const FString MatchValue = GetJsonStringField(Payload, TEXT("matchValue"), TEXT(""));
    if (MatchValue.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("matchValue is required (matchKey optional for non-struct arrays)"),
            nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    const int32 FoundIndex = FindMatchingArrayIndex(ArrayProp, ArrayContainer, MatchKey, MatchValue);
    if (FoundIndex == INDEX_NONE)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No element in %s.%s matching %s=%s"),
                *Asset->GetName(), *ArrayProp->GetName(), *MatchKey, *MatchValue),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    Helper.RemoveValues(FoundIndex, 1);

    Asset->MarkPackageDirty();
    McpSafeAssetSave(Asset);

    TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("propertyName"), ArrayProp->GetName());
    ResultJson->SetNumberField(TEXT("removedIndex"), FoundIndex);
    ResultJson->SetNumberField(TEXT("arrayLength"), Helper.Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed %s.%s[%d] (matched %s=%s)"),
            *Asset->GetName(), *ArrayProp->GetName(), FoundIndex, *MatchKey, *MatchValue), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// update_array_item
// ----------------------------------------------------------------------------
static bool HandleUpdateArrayItem(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UObject* Asset = nullptr;
    FArrayProperty* ArrayProp = nullptr;
    void* ArrayContainer = nullptr;
    if (!ResolveArrayProperty(Subsystem, RequestId, Socket, Payload, Asset, ArrayProp, ArrayContainer))
    {
        return true;
    }

    const FString MatchKey = GetJsonStringField(Payload, TEXT("matchKey"), TEXT(""));
    const FString MatchValue = GetJsonStringField(Payload, TEXT("matchValue"), TEXT(""));
    if (MatchValue.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("matchValue is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    TSharedPtr<FJsonValue> NewValue = Payload->TryGetField(TEXT("newValue"));
    if (!NewValue.IsValid())
    {
        // Allow `value` as an alias for newValue.
        NewValue = Payload->TryGetField(TEXT("value"));
    }
    if (!NewValue.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("newValue is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayContainer);
    const int32 FoundIndex = FindMatchingArrayIndex(ArrayProp, ArrayContainer, MatchKey, MatchValue);
    if (FoundIndex == INDEX_NONE)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No element in %s.%s matching %s=%s"),
                *Asset->GetName(), *ArrayProp->GetName(), *MatchKey, *MatchValue),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    void* ElemPtr = Helper.GetRawPtr(FoundIndex);

    // Zero out struct memory before write so JSON omissions revert to defaults rather than carrying old data.
    if (FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
    {
        InnerStruct->Struct->ClearScriptStruct(ElemPtr);
    }
    else
    {
        ArrayProp->Inner->ClearValue(ElemPtr);
    }

    if (!WriteJsonValueIntoArrayElement(ArrayProp->Inner, ElemPtr, NewValue))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to deserialize newValue into array element"), nullptr, TEXT("INVALID_VALUE"));
        return true;
    }

    Asset->MarkPackageDirty();
    McpSafeAssetSave(Asset);

    TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
    ResultJson->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    ResultJson->SetStringField(TEXT("propertyName"), ArrayProp->GetName());
    ResultJson->SetNumberField(TEXT("index"), FoundIndex);
    ResultJson->SetNumberField(TEXT("arrayLength"), Helper.Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Updated %s.%s[%d] (matched %s=%s)"),
            *Asset->GetName(), *ArrayProp->GetName(), FoundIndex, *MatchKey, *MatchValue), ResultJson);
    return true;
}

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageDataAssetAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpDataAssetHandlers, Verbose, TEXT("HandleManageDataAssetAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_data_asset"))
    {
        return HandleCreateDataAsset(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_data_asset_blueprint"))
    {
        return HandleCreateDataAssetBlueprint(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_data_asset_properties"))
    {
        return HandleGetDataAssetProperties(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_data_asset_properties"))
    {
        return HandleSetDataAssetProperties(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("list_data_assets"))
    {
        return HandleListDataAssets(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("duplicate_data_asset"))
    {
        return HandleDuplicateDataAsset(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_curve_keys"))
    {
        return HandleGetCurveKeys(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_curve_keys"))
    {
        return HandleSetCurveKeys(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("append_array_item"))
    {
        return HandleAppendArrayItem(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("insert_array_item"))
    {
        return HandleInsertArrayItem(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_array_item_at"))
    {
        return HandleRemoveArrayItemAt(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_array_item_where"))
    {
        return HandleRemoveArrayItemWhere(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("update_array_item"))
    {
        return HandleUpdateArrayItem(this, RequestId, Payload, Socket);
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown data_asset subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data asset operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
