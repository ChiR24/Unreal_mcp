#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

// Wave 7+ #12: explicit BS rebuild for cases where SampleData / BlendParameters
// were mutated via Python set_editor_property (raw memory write skips
// PostEditChange) or any other path that bypasses MCP's add_blend_sample.
// Optionally cascade-compile referencing AnimBlueprints so the user doesn't
// have to hunt them down manually.
TSharedPtr<FJsonObject> HandleBlendSpaceRebuild(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    bool bRebuildBlendParams = GetJsonBoolField(Params, TEXT("rebuildBlendParameters"), false);
    bool bCompileReferencers = GetJsonBoolField(Params, TEXT("compileReferencers"), true);
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
    if (!BlendSpace)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
    }

    // Step 1: drop invalid / out-of-range samples first
    BlendSpace->ValidateSampleData();

    // Step 2: trigger SampleData PostEditChange → drives grid + RuntimeBuilder rebuild
    if (FProperty* SampleDataProp = BlendSpace->GetClass()->FindPropertyByName(TEXT("SampleData")))
    {
        FPropertyChangedEvent SampleEvent(SampleDataProp);
        BlendSpace->PostEditChangeProperty(SampleEvent);
    }

    // Step 3 (optional): trigger BlendParameters PostEditChange (axis min/max changed)
    if (bRebuildBlendParams)
    {
        if (FProperty* BPProp = BlendSpace->GetClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            FPropertyChangedEvent BPEvent(BPProp);
            BlendSpace->PostEditChangeProperty(BPEvent);
        }
    }

    BlendSpace->MarkPackageDirty();
    // SaveAnimAsset returns true when bSave=false (no-op) or when the save
    // succeeds. When bSave=true and the save actually fails, surface that
    // to the caller rather than silently reporting success — the BS in
    // memory is rebuilt but the on-disk asset is stale.
    const bool bSaved = SaveAnimAsset(BlendSpace, bSave);
    if (bSave && !bSaved)
    {
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Blend space rebuilt in memory but failed to save asset: %s"), *AssetPath),
            TEXT("BLENDSPACE_SAVE_FAILED"));
    }

    // Step 4 (optional): cascade-compile every AnimBlueprint referencing this BS.
    // Track both successful and failed compiles so callers can distinguish
    // "no referencers found" from "some referencers failed to compile" —
    // the latter usually means the new BS shape broke the ABP's graph and
    // needs author attention.
    int32 CompiledCount = 0;
    int32 FailedCount = 0;
    TArray<TSharedPtr<FJsonValue>> CompiledArr;
    TArray<TSharedPtr<FJsonValue>> FailedArr;
    if (bCompileReferencers)
    {
        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const FName BSPackageName = BlendSpace->GetOutermost()->GetFName();
        TArray<FName> Referencers;
        AR.GetReferencers(BSPackageName, Referencers,
                          UE::AssetRegistry::EDependencyCategory::Package);

        for (const FName& RefPkg : Referencers)
        {
            TArray<FAssetData> Assets;
            AR.GetAssetsByPackageName(RefPkg, Assets);
            for (const FAssetData& Data : Assets)
            {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
                if (Data.AssetClassPath == UAnimBlueprint::StaticClass()->GetClassPathName())
#else
                if (Data.AssetClass == UAnimBlueprint::StaticClass()->GetFName())
#endif
                {
                    // Capture the soft path up front so a null GetAsset()
                    // still gets reported as a load failure rather than
                    // silently skipped.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
                    const FString RefPath = Data.GetObjectPathString();
#else
                    const FString RefPath = Data.ToSoftObjectPath().ToString();
#endif
                    if (UAnimBlueprint* ABP = Cast<UAnimBlueprint>(Data.GetAsset()))
                    {
                        if (McpSafeCompileBlueprint(ABP))
                        {
                            ++CompiledCount;
                            CompiledArr.Add(MakeShared<FJsonValueString>(ABP->GetPathName()));
                        }
                        else
                        {
                            ++FailedCount;
                            FailedArr.Add(MakeShared<FJsonValueString>(ABP->GetPathName()));
                        }
                    }
                    else
                    {
                        ++FailedCount;
                        FailedArr.Add(MakeShared<FJsonValueString>(RefPath));
                    }
                }
            }
        }
    }

    Response->SetStringField(TEXT("assetPath"), AssetPath);
    Response->SetBoolField(TEXT("rebuiltBlendParameters"), bRebuildBlendParams);
    Response->SetNumberField(TEXT("referencersCompiled"), CompiledCount);
    Response->SetArrayField(TEXT("compiledAnimBlueprints"), CompiledArr);
    Response->SetNumberField(TEXT("referencersFailed"), FailedCount);
    Response->SetArrayField(TEXT("failedAnimBlueprints"), FailedArr);

    ANIM_SUCCESS_RESPONSE(TEXT("Blend space rebuilt"));
    return Response;
}

} // namespace McpAnimationAuthoring
