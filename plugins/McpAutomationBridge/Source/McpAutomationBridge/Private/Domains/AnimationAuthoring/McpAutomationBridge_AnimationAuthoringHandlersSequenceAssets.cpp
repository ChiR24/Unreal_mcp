#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_animation_sequence"))
    {
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    int32 NumFrames = static_cast<int32>(GetJsonNumberField(Params, TEXT("numFrames"), 30));
    int32 FrameRate = static_cast<int32>(GetJsonNumberField(Params, TEXT("frameRate"), 30));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (FrameRate <= 0)
    {
        ANIM_ERROR_RESPONSE(TEXT("frameRate must be greater than 0"), TEXT("INVALID_FRAME_RATE"));
    }

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton = Skeleton;
    bool bExisting = false;
    FString Code;
    FString Error;
    UAnimSequence* NewSequence = Cast<UAnimSequence>(McpAnimationHandlers::CreateOrReuseAnimAsset(
        UAnimSequence::StaticClass(), Factory, Path, Name, bExisting, Code, Error));
    if (!NewSequence)
    {
        ANIM_ERROR_RESPONSE(Error, Code);
    }
    if (bExisting)
    {
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        Response->SetBoolField(TEXT("existingAsset"), true);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' already exists - reusing existing asset"), *Name));
        McpHandlerUtils::AddVerification(Response, NewSequence);
        return Response;
    }
    McpAnimationHandlers::SetAnimSequenceFrames(NewSequence, NumFrames, FrameRate);
    SaveAnimAsset(NewSequence, bSave);
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    Response->SetBoolField(TEXT("existingAsset"), false);
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewSequence);
    return Response;
    }

    if (SubAction == TEXT("set_sequence_length"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        // The rate stays what the sequence has unless frameRate is sent; it
        // used to reset to 30 whenever only the length changed. `length` is in
        // seconds and was declared but never read.
        const int32 CurrentRate = FMath::Max(1, FMath::RoundToInt(Sequence->GetSamplingFrameRate().AsDecimal()));
        const int32 FrameRate = static_cast<int32>(GetJsonNumberField(Params, TEXT("frameRate"), CurrentRate));
        if (FrameRate <= 0)
        {
            ANIM_ERROR_RESPONSE(TEXT("frameRate must be greater than 0"), TEXT("INVALID_FRAME_RATE"));
        }
        int32 NumFrames = 0;
        if (Params->HasField(TEXT("numFrames")))
        {
            NumFrames = static_cast<int32>(GetJsonNumberField(Params, TEXT("numFrames"), 0));
        }
        else if (Params->HasField(TEXT("length")))
        {
            NumFrames = FMath::RoundToInt(GetJsonNumberField(Params, TEXT("length"), 0.0) * FrameRate);
        }
        else
        {
            ANIM_ERROR_RESPONSE(TEXT("Pass numFrames, or length in seconds"), TEXT("MISSING_LENGTH"));
        }
        if (NumFrames <= 0)
        {
            ANIM_ERROR_RESPONSE(TEXT("The sequence must be at least one frame long"), TEXT("INVALID_LENGTH"));
        }

        McpAnimationHandlers::SetAnimSequenceFrames(Sequence, NumFrames, FrameRate);
        Response->SetNumberField(TEXT("numFrames"), NumFrames);
        Response->SetNumberField(TEXT("frameRate"), FrameRate);

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Sequence length updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
