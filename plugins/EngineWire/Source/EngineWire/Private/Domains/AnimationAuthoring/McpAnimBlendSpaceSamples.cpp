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
        // (INDEX_NONE), after widening the axes; refused here first, so a refusal changes nothing.
        if (!BlendSpace->ValidateAnimationSequence(Animation) || BlendSpace->IsTooCloseToExistingSamplePoint(SampleValue, INDEX_NONE))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("The blend space refused a sample at (%g, %g): another sample is already there, or the animation's skeleton or additive type does not match it"), SampleValue.X, SampleValue.Y), TEXT("SAMPLE_REJECTED"));
        }
        int32 SampleIndex = INDEX_NONE;
        {
            const FMcpAssetEdit Edit(TEXT("Add Blend Sample"), {BlendSpace}, Response);
            SampleIndex = BlendSpace->AddSample(Animation, SampleValue);
            // Grid rebuild and referencer notifications: without it the cached grid stays stale and referencing
            // Animation Blueprints may warn "sample out of bounds".
            BlendSpace->PostEditChange();
        }

        Response->SetNumberField(TEXT("sampleIndex"), SampleIndex);
        Response->SetBoolField(TEXT("saved"), bSave && SaveAnimAsset(BlendSpace, bSave));
        McpHandlerUtils::AddVerification(Response, BlendSpace);
        ANIM_SUCCESS_RESPONSE(TEXT("Blend sample added"));
        return Response;
    }

    if (SubAction == TEXT("force_rebuild_blend_space"))
    {
        return HandleBlendSpaceRebuild(Params, Response);
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
        {
            const FMcpAssetEdit Edit(TEXT("Set Axis Settings"), {BlendSpace}, Response);
            BlendParams[bVertical ? 1 : 0] = Updated;
            // PostEditChange names no property, so the blend space rebuilt nothing: samples off the new range stayed
            // valid and the runtime data kept the old grid. Validate and resample as its own editor does after an edit.
            BlendSpace->ValidateSampleData();
            BlendSpace->PostEditChange();
            BlendSpace->ResampleData();
        }

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
        {
            const FMcpAssetEdit Edit(TEXT("Set Interpolation Settings"), {BlendSpace}, Response);
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
        }

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
