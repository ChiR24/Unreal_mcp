#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

namespace McpMetaHumanHandlers
{
namespace
{
/**
 * Count assets under a content folder.
 *
 * The three export UFUNCTIONs return void and report failure only to the log,
 * so a reflected call that "succeeded" proves nothing about whether anything
 * was written. Comparing the folder before and after is the only evidence
 * available at this layer, and without it this handler would report success
 * for an export that produced no assets at all.
 */
int32 CountAssetsUnder(const FString& ContentPath)
{
    const FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByPath(FName(*ContentPath), Assets, /*bRecursive=*/true);
    return Assets.Num();
}

const TCHAR* const ExportLibraryClassPath =
    TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterExportBlueprintLibrary");
}

// export_metahuman -- write the assembled character out as reusable assets.
//
// Geometry and materials land in project content; DNA can also go to disk. The
// three exports take different parameter structs, so the export type selects
// both the UFUNCTION and the struct built for it.
bool HandleExportMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket)
{
    UObject* Subsystem = nullptr;
    UObject* Character = RequireEditableCharacter(Self, RequestId, Payload, Socket, Subsystem);
    if (!Character)
    {
        return true;
    }

    UClass* LibraryClass = FindObject<UClass>(nullptr, ExportLibraryClassPath);
    if (!LibraryClass)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("MetaHuman export library is not available on this engine version."),
            TEXT("FEATURE_UNAVAILABLE"));
        return true;
    }

    const FString ExportType = GetJsonStringField(Payload, TEXT("exportType"), TEXT("geometry"));
    const FString ProjectPath = GetJsonStringField(Payload, TEXT("projectPath"), TEXT("/Game/MetaHumans"));

    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    const TCHAR* FunctionName = nullptr;

    if (ExportType == TEXT("geometry"))
    {
        FunctionName = TEXT("ExportGeometry");
        Params->SetStringField(TEXT("ProjectPath"), ProjectPath);
        Params->SetBoolField(TEXT("bHeadSkeletalMesh"), GetJsonBoolField(Payload,TEXT("headMesh"), true));
        Params->SetBoolField(TEXT("bBodySkeletalMesh"), GetJsonBoolField(Payload,TEXT("bodyMesh"), true));
        Params->SetBoolField(TEXT("bFullBodySkeletalMesh"), GetJsonBoolField(Payload,TEXT("fullBodyMesh"), false));
        Params->SetBoolField(TEXT("bOverwriteExistingAssets"), GetJsonBoolField(Payload,TEXT("overwrite"), true));
    }
    else if (ExportType == TEXT("materials"))
    {
        FunctionName = TEXT("ExportMaterials");
        Params->SetStringField(TEXT("ProjectPath"), ProjectPath);
        Params->SetBoolField(TEXT("bApplyAsOverrides"), GetJsonBoolField(Payload,TEXT("applyAsOverrides"), true));
    }
    else if (ExportType == TEXT("dna"))
    {
        FunctionName = TEXT("ExportDNA");
        Params->SetStringField(TEXT("ProjectPath"), ProjectPath);
        // A host directory, so it gets the same project-containment check as
        // every other host path the plugin accepts (asset.import's sourcePath,
        // screenshots); passed through raw it was a write to anywhere on disk.
        FString ExternalPath = GetJsonStringField(Payload, TEXT("externalPath"));
        if (!ExternalPath.IsEmpty())
        {
            FString ResolvedExternal, PathError;
            if (!McpResolveProjectFilePath(ExternalPath, ResolvedExternal, PathError))
            {
                Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
                return true;
            }
            ExternalPath = ResolvedExternal;
        }
        Params->SetStringField(TEXT("ExternalPath"), ExternalPath);
        Params->SetBoolField(TEXT("bDNAHead"), GetJsonBoolField(Payload,TEXT("dnaHead"), true));
        Params->SetBoolField(TEXT("bDNABody"), GetJsonBoolField(Payload,TEXT("dnaBody"), true));
        Params->SetBoolField(TEXT("bOverwriteExistingAssets"), GetJsonBoolField(Payload,TEXT("overwrite"), true));
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown exportType '%s'. Use 'geometry', 'materials' or 'dna'."), *ExportType),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const int32 AssetsBefore = CountAssetsUnder(ProjectPath);

    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    Args->SetObjectField(TEXT("InParams"), Params);

    // These are static UFUNCTIONs on a BlueprintFunctionLibrary, so the class
    // default object is the correct call target.
    TSharedPtr<FJsonObject> Results;
    FString Error;
    if (!InvokeMetaHumanFunction(LibraryClass->GetDefaultObject(), FunctionName, Args, Character, Results, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("OPERATION_FAILED"));
        return true;
    }

    const int32 AssetsAfter = CountAssetsUnder(ProjectPath);
    const int32 AssetsCreated = AssetsAfter - AssetsBefore;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("characterPath"), GetJsonStringField(Payload, TEXT("characterPath")));
    Result->SetStringField(TEXT("exportType"), ExportType);
    Result->SetStringField(TEXT("projectPath"), ProjectPath);
    Result->SetNumberField(TEXT("assetsCreated"), AssetsCreated);

    // An export that wrote nothing is a failure, however cleanly the reflected
    // call returned. Overwriting in place legitimately creates no new asset, so
    // only the non-overwrite case can be judged this way.
    const bool bOverwrite = GetJsonBoolField(Payload,TEXT("overwrite"), true);
    if (AssetsCreated <= 0 && !bOverwrite)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("The %s export produced no assets under %s. The character usually ")
                TEXT("has to be built first, and material export additionally needs high-resolution ")
                TEXT("textures, which require MetaHuman Creator Core Data."), *ExportType, *ProjectPath),
            TEXT("OPERATION_FAILED"));
        return true;
    }

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("MetaHuman %s exported (%d new asset(s))"), *ExportType, AssetsCreated), Result);
    return true;
}
}
#endif
