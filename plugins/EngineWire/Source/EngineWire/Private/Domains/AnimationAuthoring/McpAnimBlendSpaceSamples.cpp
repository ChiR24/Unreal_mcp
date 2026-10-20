#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlendSpaceSampleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_blend_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
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
                    SampleValue.X = GetJsonNumberField(SampleObj, TEXT("x"), 0.0);
                    SampleValue.Y = GetJsonNumberField(SampleObj, TEXT("y"), 0.0);
                }
                else if (SampleVal->Type == EJson::Array && SampleVal->AsArray().Num() >= 2)
                {
                    // [x, y]: an array was dropped and the sample landed at 0 on both axes.
                    SampleValue.X = SampleVal->AsArray()[0]->AsNumber();
                    SampleValue.Y = SampleVal->AsArray()[1]->AsNumber();
                }
            }
        }

        // AddSample refuses a position another sample holds and an animation of another skeleton or additive type
        // (INDEX_NONE); that refusal used to be answered "Blend sample added".
        const int32 SampleIndex = BlendSpace->AddSample(Animation, SampleValue);
        if (SampleIndex == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("The blend space refused a sample at (%g, %g): another sample is already there, or the animation's skeleton or additive type does not match it"), SampleValue.X, SampleValue.Y), TEXT("SAMPLE_REJECTED"));
        }

        // Wave 7+ #12: Trigger PostEditChange so grid rebuild + referencer
        // notifications fire. Without this, BS internal cached grid stays
        // stale and referencing ABPs may compile-warn "sample out of bounds".
        BlendSpace->PostEditChange();

        Response->SetNumberField(TEXT("sampleIndex"), SampleIndex);
        Response->SetBoolField(TEXT("saved"), bSave && SaveAnimAsset(BlendSpace, bSave));
        McpHandlerUtils::AddVerification(Response, BlendSpace);
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

    if (SubAction == TEXT("set_axis_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString Axis = GetJsonStringField(Params, TEXT("axis"), TEXT("Horizontal"));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        // Any axis name but these used to mean Horizontal, a 1D blend space took Vertical into a slot it never reads,
        // and a call that set nothing (or set a backwards range) was saved as "updated".
        const bool bVertical = Axis == TEXT("Vertical") || Axis == TEXT("Y");
        const bool bAnyField = Params->HasField(TEXT("axisName")) || Params->HasField(TEXT("minValue")) || Params->HasField(TEXT("maxValue")) || Params->HasField(TEXT("gridDivisions"));
        if ((!bVertical && Axis != TEXT("Horizontal") && Axis != TEXT("X")) || (bVertical && BlendSpace->IsA<UBlendSpace1D>()) || !bAnyField)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("axis must be Horizontal or Vertical (a 1D blend space has Horizontal only), with axisName, minValue, maxValue or gridDivisions to set; got axis '%s'"), *Axis), TEXT("INVALID_ARGUMENT"));
        }
        // BlendParameters is protected; write it through reflection exactly like create_blend_space_1d/2d do. Only the
        // fields the caller sent change.
        FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters"));
        FBlendParameter* BlendParams = BlendParamsProp ? BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace) : nullptr;
        if (!BlendParams)
        {
            ANIM_ERROR_RESPONSE(TEXT("This blend space has no axis settings to write"), TEXT("UNSUPPORTED"));
        }
        FBlendParameter Updated = BlendParams[bVertical ? 1 : 0];
        Updated.DisplayName = GetJsonStringField(Params, TEXT("axisName"), Updated.DisplayName);
        Updated.Min = static_cast<float>(GetJsonNumberField(Params, TEXT("minValue"), Updated.Min));
        Updated.Max = static_cast<float>(GetJsonNumberField(Params, TEXT("maxValue"), Updated.Max));
        Updated.GridNum = static_cast<int32>(GetJsonNumberField(Params, TEXT("gridDivisions"), Updated.GridNum));
        if (Updated.Min >= Updated.Max || Updated.GridNum < 1)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("minValue must be below maxValue and gridDivisions at least 1 (got %g to %g, %d divisions); nothing was changed"), Updated.Min, Updated.Max, Updated.GridNum), TEXT("INVALID_ARGUMENT"));
        }
        BlendSpace->Modify();
        BlendParams[bVertical ? 1 : 0] = Updated;
        BlendSpace->PostEditChange();
        BlendSpace->MarkPackageDirty();

        Response->SetStringField(TEXT("axis"), bVertical ? TEXT("Vertical") : TEXT("Horizontal"));
        Response->SetStringField(TEXT("axisName"), Updated.DisplayName);
        Response->SetNumberField(TEXT("minValue"), Updated.Min);
        Response->SetNumberField(TEXT("maxValue"), Updated.Max);
        Response->SetNumberField(TEXT("gridDivisions"), Updated.GridNum);
        Response->SetBoolField(TEXT("saved"), bSave && SaveAnimAsset(BlendSpace, bSave));
        McpHandlerUtils::AddVerification(Response, BlendSpace);
        ANIM_SUCCESS_RESPONSE(TEXT("Axis settings updated"));
        return Response;
    }

    if (SubAction == TEXT("set_interpolation_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        // interpolationType was read and never applied, and the weight speed was
        // overwritten with 5 on every call. Each field now changes only when sent;
        // the smoothing type and time apply to every input axis.
        if (!Params->HasField(TEXT("interpolationType")) && !Params->HasField(TEXT("interpolationTime")) && !Params->HasField(TEXT("targetWeightInterpolationSpeed")))
        {
            ANIM_ERROR_RESPONSE(TEXT("Send interpolationType, interpolationTime or targetWeightInterpolationSpeed; nothing was changed"), TEXT("INVALID_ARGUMENT"));
        }
        TOptional<EFilterInterpolationType> SmoothingType;
        if (Params->HasField(TEXT("interpolationType")))
        {
            const FString Requested = GetJsonStringField(Params, TEXT("interpolationType"), TEXT(""));
            const UEnum* SmoothingEnum = StaticEnum<EFilterInterpolationType>();
            int64 Value = SmoothingEnum->GetValueByNameString(TEXT("BSIT_") + Requested);
            if (Value == INDEX_NONE)
            {
                Value = SmoothingEnum->GetValueByNameString(Requested);
            }
            if (Value == INDEX_NONE || Value == BSIT_MAX)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Unknown interpolationType '%s'; use Average, Linear, Cubic, EaseInOut, ExponentialDecay or SpringDamper"), *Requested), TEXT("INVALID_INTERPOLATION_TYPE"));
            }
            SmoothingType = static_cast<EFilterInterpolationType>(Value);
        }
        BlendSpace->Modify();
        for (FInterpolationParameter& Axis : BlendSpace->InterpolationParam)
        {
            if (SmoothingType.IsSet())
            {
                Axis.InterpolationType = SmoothingType.GetValue();
            }
            if (Params->HasField(TEXT("interpolationTime")))
            {
                Axis.InterpolationTime = static_cast<float>(GetJsonNumberField(Params, TEXT("interpolationTime"), 0.0));
            }
        }
        if (Params->HasField(TEXT("targetWeightInterpolationSpeed")))
        {
            BlendSpace->TargetWeightInterpolationSpeedPerSec = static_cast<float>(GetJsonNumberField(Params, TEXT("targetWeightInterpolationSpeed"), 0.0));
        }
        BlendSpace->PostEditChange();
        BlendSpace->MarkPackageDirty();

        Response->SetBoolField(TEXT("saved"), bSave && SaveAnimAsset(BlendSpace, bSave));
        Response->SetNumberField(TEXT("interpolationTime"), BlendSpace->InterpolationParam[0].InterpolationTime);
        Response->SetStringField(TEXT("interpolationType"), StaticEnum<EFilterInterpolationType>()->GetNameStringByValue(BlendSpace->InterpolationParam[0].InterpolationType));
        Response->SetNumberField(TEXT("targetWeightInterpolationSpeed"), BlendSpace->TargetWeightInterpolationSpeedPerSec);
        McpHandlerUtils::AddVerification(Response, BlendSpace);

        ANIM_SUCCESS_RESPONSE(TEXT("Interpolation settings updated"));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
