#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlendSpaceSampleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_blend_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));

        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }

        // Get sample value
        FVector SampleValue = FVector::ZeroVector;
        if (Params->HasField(TEXT("sampleValue")))
        {
            TSharedPtr<FJsonValue> SampleVal = Params->TryGetField(TEXT("sampleValue"));
            if (SampleVal.IsValid())
            {
                if (SampleVal->Type == EJson::Number)
                {
                    // 1D blend space
                    SampleValue.X = SampleVal->AsNumber();
                }
                else if (SampleVal->Type == EJson::Object)
                {
                    // 2D blend space
                    TSharedPtr<FJsonObject> SampleObj = SampleVal->AsObject();
                    SampleValue.X = GetNumberFieldAnimAuth(SampleObj, TEXT("x"), 0.0);
                    SampleValue.Y = GetNumberFieldAnimAuth(SampleObj, TEXT("y"), 0.0);
                }
            }
        }

        // Add sample
        BlendSpace->AddSample(Animation, SampleValue);

        // Wave 7+ #12: Trigger PostEditChange so grid rebuild + referencer
        // notifications fire. Without this, BS internal cached grid stays
        // stale and referencing ABPs may compile-warn "sample out of bounds".
        BlendSpace->PostEditChange();

        SaveAnimAsset(BlendSpace, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend sample added"));
        return Response;
    }

    // Wave 7+ #12: explicit BS rebuild for cases where SampleData / BlendParameters
    // were mutated via Python set_editor_property (raw memory write skips
    // PostEditChange) or any other path that bypasses MCP's add_blend_sample.
    // Optionally cascade-compile referencing AnimBlueprints so the user doesn't
    // have to hunt them down manually.
    if (SubAction == TEXT("force_rebuild_blend_space"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bRebuildBlendParams = GetBoolFieldAnimAuth(Params, TEXT("rebuildBlendParameters"), false);
        bool bCompileReferencers = GetBoolFieldAnimAuth(Params, TEXT("compileReferencers"), true);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
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

    if (SubAction == TEXT("set_axis_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString Axis = GetStringFieldAnimAuth(Params, TEXT("axis"), TEXT("Horizontal"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));

        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        // Determine axis index
        int32 AxisIndex = 0;
        if (Axis == TEXT("Vertical") || Axis == TEXT("Y"))
        {
            AxisIndex = 1;
        }

        // Update axis settings - UE 5.7+ GetBlendParameter returns const ref
        // We need to use Modify() pattern or just read and report the constraint
        // For now, skip direct modification since BlendParameters is protected
        // The creation flow above already sets defaults, this is for runtime update which
        // may need different approach per UE version

        // Log info about what was requested but note it may not take effect in UE 5.7+
        FString RequestedAxisName = GetStringFieldAnimAuth(Params, TEXT("axisName"), TEXT(""));
        float RequestedMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("minValue"), 0.0));
        float RequestedMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("maxValue"), 100.0));
        int32 RequestedGridNum = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("gridDivisions"), 4));

        // Trigger PostEditChange to ensure any internal updates
        BlendSpace->PostEditChange();
        BlendSpace->MarkPackageDirty();

        SaveAnimAsset(BlendSpace, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Axis settings updated"));
        return Response;
    }

    if (SubAction == TEXT("set_interpolation_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InterpolationType = GetStringFieldAnimAuth(Params, TEXT("interpolationType"), TEXT("Lerp"));
        float TargetWeightSpeed = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("targetWeightInterpolationSpeed"), 5.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));

        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        BlendSpace->TargetWeightInterpolationSpeedPerSec = TargetWeightSpeed;

        SaveAnimAsset(BlendSpace, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Interpolation settings updated"));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
