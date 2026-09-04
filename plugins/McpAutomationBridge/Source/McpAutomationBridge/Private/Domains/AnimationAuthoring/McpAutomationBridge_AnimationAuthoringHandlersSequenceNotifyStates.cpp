#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

UClass* ResolveNotifyClassByName(const FString& Requested, const TCHAR* Prefix, UClass* BaseClass, TArray<FString>& OutTried)
{
    if (Requested.IsEmpty())
    {
        return nullptr;
    }
    const bool bIsPath = Requested.Contains(TEXT("/"));
    const FString Prefixed = (bIsPath || Requested.StartsWith(Prefix)) ? Requested : FString(Prefix) + Requested;

    // Short and prefixed names resolve through the engine script package
    // first ("/Script/Engine.AnimNotify_PlaySound"), then by bare class name;
    // a path is looked up (and loaded, for Blueprint classes) as given.
    TArray<FString> Candidates;
    if (!bIsPath)
    {
        Candidates.Add(FString::Printf(TEXT("/Script/Engine.%s"), *Prefixed));
    }
    Candidates.AddUnique(Prefixed);
    Candidates.AddUnique(Requested);

    for (const FString& Candidate : Candidates)
    {
        OutTried.Add(Candidate);
        UClass* Found = nullptr;
        if (Candidate.StartsWith(TEXT("/")))
        {
            Found = FindObject<UClass>(nullptr, *Candidate);
            if (!Found)
            {
                Found = LoadObject<UClass>(nullptr, *Candidate);
            }
        }
        else
        {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
            Found = FindFirstObject<UClass>(*Candidate, EFindFirstObjectOptions::None);
#else
            Found = ResolveClassByName(Candidate);
#endif
        }
        if (Found && Found->IsChildOf(BaseClass))
        {
            return Found;
        }
    }
    return nullptr;
}

FString NotifyNameFromClass(const UClass* NotifyClass, const TCHAR* Prefix)
{
    FString Name = NotifyClass->GetName();
    Name.RemoveFromStart(Prefix);
    Name.RemoveFromEnd(TEXT("_C"));
    return Name;
}

TSharedPtr<FJsonObject> HandleSequenceNotifyStateAction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
    int32 StartFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("startFrame"), 0));
    int32 EndFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("endFrame"), 10));
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (EndFrame < StartFrame)
    {
        ANIM_ERROR_RESPONSE(TEXT("endFrame must be greater than or equal to startFrame"), TEXT("INVALID_FRAME_RANGE"));
    }

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify state class BEFORE modifying the asset
    UClass* ResolvedNotifyStateClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        TArray<FString> Tried;
        ResolvedNotifyStateClass = ResolveNotifyClassByName(NotifyClass, TEXT("AnimNotifyState_"), UAnimNotifyState::StaticClass(), Tried);
        if (!ResolvedNotifyStateClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotifyState class '%s' not found (tried %s). Use a concrete subclass like AnimNotifyState_PlayMontageNotify or a custom AnimNotifyState blueprint class path."), *NotifyClass, *FString::Join(Tried, TEXT(", "))),
                TEXT("CLASS_NOT_FOUND")
            );
        }
        if (ResolvedNotifyStateClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotifyState: '%s' is an abstract class. Use a concrete subclass like AnimNotifyState_PlayMontageNotify or create a custom AnimNotifyState blueprint."), *ResolvedNotifyStateClass->GetName()),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }
        if (NotifyName.IsEmpty())
        {
            NotifyName = NotifyNameFromClass(ResolvedNotifyStateClass, TEXT("AnimNotifyState_"));
        }
    }

    UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
    if (!AnimAsset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
    if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
    {
        FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
    }
#endif
    if (FrameRate <= KINDA_SMALL_NUMBER)
    {
        FrameRate = 30.0f;
    }
    float StartTime = static_cast<float>(StartFrame) / FrameRate;
    float EndTime = static_cast<float>(EndFrame) / FrameRate;
    float Duration = EndTime - StartTime;

    FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NotifyEvent.Link(AnimAsset, StartTime);
    NotifyEvent.SetDuration(Duration);
    NotifyEvent.EndLink.Link(AnimAsset, EndTime);
    NotifyEvent.TrackIndex = TrackIndex;
    NotifyEvent.NotifyName = FName(*NotifyName);

    if (ResolvedNotifyStateClass)
    {
        UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimAsset, ResolvedNotifyStateClass);
        if (!NewNotifyState)
        {
            AnimAsset->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotifyState instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.NotifyStateClass = NewNotifyState;
    }

    AnimAsset->RefreshCacheData();
    SaveAnimAsset(AnimAsset, bSave);

    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Notify state '%s' added at %.3fs"), *NotifyName, StartTime));
    Response->SetStringField(TEXT("notifyName"), NotifyName);
    Response->SetStringField(TEXT("notifyClass"), ResolvedNotifyStateClass ? ResolvedNotifyStateClass->GetPathName() : FString());
    Response->SetNumberField(TEXT("startTime"), StartTime);
    Response->SetNumberField(TEXT("endTime"), EndTime);
    Response->SetNumberField(TEXT("frameRate"), FrameRate);
    Response->SetNumberField(TEXT("trackIndex"), TrackIndex);
    McpHandlerUtils::AddVerification(Response, AnimAsset);
    return Response;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
