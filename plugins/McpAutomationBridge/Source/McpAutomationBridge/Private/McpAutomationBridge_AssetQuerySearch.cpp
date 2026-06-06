#include "McpAutomationBridge_AssetQueryHandlersPrivate.h"

namespace McpAssetQueryHandlers
{
namespace
{
struct FClassMapping
{
    const TCHAR* ShortName;
    const TCHAR* PackagePath;
    const TCHAR* ClassNameStr;
};

static const FClassMapping GClassMappings[] =
{
    { TEXT("Blueprint"),                TEXT("/Script/Engine"),          TEXT("Blueprint") },
    { TEXT("StaticMesh"),               TEXT("/Script/Engine"),          TEXT("StaticMesh") },
    { TEXT("SkeletalMesh"),             TEXT("/Script/Engine"),          TEXT("SkeletalMesh") },
    { TEXT("Material"),                 TEXT("/Script/Engine"),          TEXT("Material") },
    { TEXT("MaterialInstance"),         TEXT("/Script/Engine"),          TEXT("MaterialInstanceConstant") },
    { TEXT("MaterialInstanceConstant"), TEXT("/Script/Engine"),          TEXT("MaterialInstanceConstant") },
    { TEXT("Texture2D"),                TEXT("/Script/Engine"),          TEXT("Texture2D") },
    { TEXT("Level"),                    TEXT("/Script/Engine"),          TEXT("World") },
    { TEXT("World"),                    TEXT("/Script/Engine"),          TEXT("World") },
    { TEXT("SoundCue"),                 TEXT("/Script/Engine"),          TEXT("SoundCue") },
    { TEXT("SoundWave"),                TEXT("/Script/Engine"),          TEXT("SoundWave") },
    { TEXT("AnimSequence"),             TEXT("/Script/Engine"),          TEXT("AnimSequence") },
    { TEXT("AnimMontage"),              TEXT("/Script/Engine"),          TEXT("AnimMontage") },
    { TEXT("AnimBlueprint"),            TEXT("/Script/Engine"),          TEXT("AnimBlueprint") },
    { TEXT("BlendSpace"),               TEXT("/Script/Engine"),          TEXT("BlendSpace") },
    { TEXT("BlendSpace1D"),             TEXT("/Script/Engine"),          TEXT("BlendSpace1D") },
    { TEXT("Skeleton"),                 TEXT("/Script/Engine"),          TEXT("Skeleton") },
    { TEXT("PhysicsAsset"),             TEXT("/Script/Engine"),          TEXT("PhysicsAsset") },
    { TEXT("NiagaraSystem"),            TEXT("/Script/Niagara"),         TEXT("NiagaraSystem") },
    { TEXT("NiagaraEmitter"),           TEXT("/Script/Niagara"),         TEXT("NiagaraEmitter") },
    { TEXT("ParticleSystem"),           TEXT("/Script/Engine"),          TEXT("ParticleSystem") },
    { TEXT("SoundBase"),                TEXT("/Script/Engine"),          TEXT("SoundBase") },
    { TEXT("MetaSoundSource"),          TEXT("/Script/MetasoundEngine"), TEXT("MetaSoundSource") },
    { TEXT("WidgetBlueprint"),          TEXT("/Script/UMGEditor"),       TEXT("WidgetBlueprint") },
    { TEXT("DataTable"),                TEXT("/Script/Engine"),          TEXT("DataTable") },
    { TEXT("DataAsset"),                TEXT("/Script/Engine"),          TEXT("DataAsset") },
    { TEXT("CurveFloat"),               TEXT("/Script/Engine"),          TEXT("CurveFloat") },
    { TEXT("Texture"),                  TEXT("/Script/Engine"),          TEXT("Texture") },
    { TEXT("TextureRenderTarget2D"),    TEXT("/Script/Engine"),          TEXT("TextureRenderTarget2D") },
    { TEXT("MaterialFunction"),         TEXT("/Script/Engine"),          TEXT("MaterialFunction") },
    { TEXT("LevelSequence"),            TEXT("/Script/LevelSequence"),   TEXT("LevelSequence") },
};

FString GetSupportedClassNames()
{
    static FString SupportedNames;
    if (!SupportedNames.IsEmpty())
    {
        return SupportedNames;
    }

    TSet<FString> UniqueNames;
    for (const FClassMapping& Mapping : GClassMappings)
    {
        UniqueNames.Add(Mapping.ShortName);
    }
    TArray<FString> SortedNames = UniqueNames.Array();
    SortedNames.Sort();
    SupportedNames = FString::Join(SortedNames, TEXT(", "));
    return SupportedNames;
}

bool AddClassFilter(const FString& ClassName, FARFilter& Filter)
{
    if (ClassName.Contains(TEXT("/")))
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
#else
        int32 DotIndex;
        if (ClassName.FindLastChar(TEXT('.'), DotIndex))
        {
            Filter.ClassNames.Add(FName(*ClassName.Mid(DotIndex + 1)));
        }
        else
        {
            Filter.ClassNames.Add(FName(*ClassName));
        }
#endif
        return true;
    }

    for (const FClassMapping& Mapping : GClassMappings)
    {
        if (ClassName.Equals(Mapping.ShortName, ESearchCase::IgnoreCase))
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            Filter.ClassPaths.Add(FTopLevelAssetPath(Mapping.PackagePath, Mapping.ClassNameStr));
#else
            Filter.ClassNames.Add(FName(Mapping.ClassNameStr));
#endif
            return true;
        }
    }
    return false;
}
}

bool HandleSearchAssets(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FARFilter Filter;

    const TArray<TSharedPtr<FJsonValue>>* ClassNamesPtr;
    if (Payload->TryGetArrayField(TEXT("classNames"), ClassNamesPtr) && ClassNamesPtr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *ClassNamesPtr)
        {
            const FString ClassName = Val->AsString();
            if (!ClassName.IsEmpty() && !AddClassFilter(ClassName, Filter))
            {
                Bridge->SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Unknown short class name '%s'. Use full path (e.g. /Script/Engine.AnimSequence) or one of: %s."),
                        *ClassName, *GetSupportedClassNames()),
                    TEXT("UNKNOWN_CLASS_NAME"));
                return true;
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* PackagePathsPtr;
    bool bHasValidPaths = false;
    if (Payload->TryGetArrayField(TEXT("packagePaths"), PackagePathsPtr) &&
        PackagePathsPtr && PackagePathsPtr->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Val : *PackagePathsPtr)
        {
            const FString RawPath = Val->AsString();
            const FString SanitizedPath = SanitizeProjectRelativePath(RawPath);
            if (SanitizedPath.IsEmpty())
            {
                Bridge->SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Invalid package path '%s': contains traversal sequences"), *RawPath),
                    TEXT("INVALID_PATH"));
                return true;
            }
            Filter.PackagePaths.Add(FName(*SanitizedPath));
            bHasValidPaths = true;
        }
    }

    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("path"), SinglePath) && !SinglePath.IsEmpty())
    {
        const FString SanitizedPath = SanitizeProjectRelativePath(SinglePath);
        if (SanitizedPath.IsEmpty())
        {
            Bridge->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *SinglePath),
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        Filter.PackagePaths.Add(FName(*SanitizedPath));
        bHasValidPaths = true;
    }

    if (!bHasValidPaths)
    {
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
    }

    FString SearchText;
    Payload->TryGetStringField(TEXT("searchText"), SearchText);

    bool bRecursivePaths = true;
    if (Payload->HasField(TEXT("recursivePaths")))
    {
        Payload->TryGetBoolField(TEXT("recursivePaths"), bRecursivePaths);
    }
    Filter.bRecursivePaths = bRecursivePaths;

    bool bRecursiveClasses = false;
    if (Payload->HasField(TEXT("recursiveClasses")))
    {
        Payload->TryGetBoolField(TEXT("recursiveClasses"), bRecursiveClasses);
    }
    Filter.bRecursiveClasses = bRecursiveClasses;

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);

    if (!SearchText.IsEmpty())
    {
        AssetDataList.RemoveAll([&SearchText](const FAssetData& Data)
        {
            return !Data.AssetName.ToString().Contains(SearchText, ESearchCase::IgnoreCase);
        });
    }

    AssetDataList.Sort([](const FAssetData& A, const FAssetData& B)
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
#else
        return A.ToSoftObjectPath().ToString() < B.ToSoftObjectPath().ToString();
#endif
    });

    const int32 TotalCount = AssetDataList.Num();

    int32 Offset = 0;
    if (Payload->HasField(TEXT("offset")))
    {
        Payload->TryGetNumberField(TEXT("offset"), Offset);
        Offset = FMath::Max(0, Offset);
    }

    int32 Limit = 100;
    if (Payload->HasField(TEXT("limit")))
    {
        Payload->TryGetNumberField(TEXT("limit"), Limit);
        Limit = FMath::Max(0, Limit);
    }

    if (Offset > 0 && Offset < AssetDataList.Num())
    {
        AssetDataList.RemoveAt(0, Offset);
    }
    else if (Offset >= AssetDataList.Num())
    {
        AssetDataList.Empty();
    }

    if (Limit == 0)
    {
        AssetDataList.Empty();
    }
    else if (AssetDataList.Num() > Limit)
    {
        AssetDataList.SetNum(Limit);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData& Data : AssetDataList)
    {
        TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
        AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        AssetObj->SetStringField(TEXT("assetPath"), Data.GetSoftObjectPath().ToString());
        AssetObj->SetStringField(TEXT("classPath"), Data.AssetClassPath.ToString());
#else
        AssetObj->SetStringField(TEXT("assetPath"), Data.ToSoftObjectPath().ToString());
        AssetObj->SetStringField(TEXT("classPath"), Data.AssetClass.ToString());
#endif
        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), AssetsArray.Num());
    Result->SetNumberField(TEXT("totalCount"), TotalCount);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetNumberField(TEXT("limit"), Limit);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Assets found."), Result);
    return true;
}
}
