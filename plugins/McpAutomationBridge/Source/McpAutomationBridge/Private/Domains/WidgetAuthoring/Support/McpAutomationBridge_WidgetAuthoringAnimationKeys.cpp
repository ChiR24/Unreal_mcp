// McpAutomationBridge_WidgetAuthoringAnimationKeys.cpp — MovieScene key authoring for widget animations.
//
// Dogfood #38: add_animation_keyframe used to refuse with NOT_SUPPORTED. It now finds or creates the
// widget binding + property track, adds a section, and writes real channel keys for RenderOpacity,
// ColorAndOpacity and (in AnimationKeysTransform.cpp) the RenderTransform.
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeysInternal.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Components/Widget.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
using namespace WidgetAnimationKeys;

namespace
{
FGuid FindOrCreateBinding(UMovieScene* MovieScene, UWidgetAnimation* Animation, UWidget* Target, bool& bOutCreated)
{
    for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
    {
        if (Binding.WidgetName == Target->GetFName())
        {
            return Binding.AnimationGuid;
        }
    }
    const FGuid Guid = MovieScene->AddPossessable(Target->GetName(), Target->GetClass());
    FWidgetAnimationBinding NewBinding;
    NewBinding.AnimationGuid = Guid;
    NewBinding.WidgetName = Target->GetFName();
    NewBinding.SlotWidgetName = NAME_None;
    NewBinding.bIsRootWidget = false;
    Animation->AnimationBindings.Add(NewBinding);
    bOutCreated = true;
    return Guid;
}

TSharedPtr<FJsonValue> ReadValueField(const TSharedPtr<FJsonObject>& Payload)
{
    static const TCHAR* const Fields[] = { TEXT("propertyValue"), TEXT("value"), TEXT("keyValue") };
    for (const TCHAR* Field : Fields)
    {
        if (Payload->HasField(Field))
        {
            return Payload->TryGetField(Field);
        }
    }
    return nullptr;
}

bool IsTransformKind(const FString& Kind)
{
    return Kind == TEXT("translation") || Kind == TEXT("position") || Kind == TEXT("scale") || Kind == TEXT("shear") ||
           Kind == TEXT("angle") || Kind == TEXT("rotation") || Kind == TEXT("transform") || Kind == TEXT("rendertransform");
}
} // namespace

bool McpAuthorWidgetAnimationKey(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation, UWidget* Target,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpWidgetKeyResult& Out,
                                 FString& OutError, FString& OutErrorCode)
{
    UMovieScene* MovieScene = Animation ? Animation->GetMovieScene() : nullptr;
    if (!WidgetBP || !MovieScene || !Target || !Payload.IsValid())
    {
        OutError = TEXT("The animation has no MovieScene or the target widget is missing");
        OutErrorCode = TEXT("ANIMATION_ERROR");
        return false;
    }
    FString TrackType;
    Payload->TryGetStringField(TEXT("trackType"), TrackType);
    if (TrackType.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("propertyName"), TrackType);
    }
    if (TrackType.IsEmpty())
    {
        TrackType = TEXT("opacity");
    }
    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);
    FString Interp = TEXT("auto");
    Payload->TryGetStringField(TEXT("interpolation"), Interp);
    const FFrameNumber Frame = (Time * MovieScene->GetTickResolution()).RoundToFrame();
    const TSharedPtr<FJsonValue> ValueField = ReadValueField(Payload);
    const FString Kind = TrackType.ToLower();
    MovieScene->Modify();
    Animation->Modify();
    const FGuid Guid = FindOrCreateBinding(MovieScene, Animation, Target, Out.bCreatedBinding);
    Out.BindingGuid = Guid.ToString();
    Out.TrackType = Kind;
    Out.FrameNumber = Frame.Value;
    UMovieSceneSection* Section = nullptr;
    if (Kind == TEXT("opacity") || Kind == TEXT("renderopacity") || Kind == TEXT("float"))
    {
        double Value = 1.0;
        if (!ValueField.IsValid() || !ValueField->TryGetNumber(Value))
        {
            OutError = TEXT("opacity keys need a numeric value (0..1) in propertyValue or value");
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
        UMovieSceneFloatTrack* Track =
            FindOrAddPropertyTrack<UMovieSceneFloatTrack>(MovieScene, Guid, TEXT("RenderOpacity"), Out.bCreatedTrack);
        Section = FindOrAddSection(Track);
        Out.KeyCount = AddFloatKey(Section, 0, Frame, Value, Interp);
        Out.PropertyName = TEXT("RenderOpacity");
        Out.ChannelCount = 1;
    }
    else if (Kind == TEXT("color") || Kind == TEXT("colorandopacity") || Kind == TEXT("tint"))
    {
        const TSharedPtr<FJsonObject>* ColorObject = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
        double Channels[4] = { 1.0, 1.0, 1.0, 1.0 };
        if (ValueField.IsValid() && ValueField->TryGetObject(ColorObject) && ColorObject)
        {
            static const TCHAR* const Names[4] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
            for (int32 Index = 0; Index < 4; ++Index)
            {
                (*ColorObject)->TryGetNumberField(Names[Index], Channels[Index]);
            }
        }
        else if (ValueField.IsValid() && ValueField->TryGetArray(ColorArray) && ColorArray && ColorArray->Num() >= 3)
        {
            for (int32 Index = 0; Index < FMath::Min(4, ColorArray->Num()); ++Index)
            {
                Channels[Index] = (*ColorArray)[Index]->AsNumber();
            }
        }
        else
        {
            OutError = TEXT("color keys need a {r,g,b,a} object or [r,g,b,a] array in propertyValue");
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
        UMovieSceneColorTrack* Track =
            FindOrAddPropertyTrack<UMovieSceneColorTrack>(MovieScene, Guid, TEXT("ColorAndOpacity"), Out.bCreatedTrack);
        Section = FindOrAddSection(Track);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Out.KeyCount = AddFloatKey(Section, Index, Frame, Channels[Index], Interp);
        }
        Out.PropertyName = TEXT("ColorAndOpacity");
        Out.ChannelCount = 4;
    }
    else if (IsTransformKind(Kind))
    {
        if (!McpAuthorTransformKeys(MovieScene, Guid, Frame, Kind, TrackType, Interp, ValueField, Out, Section, OutError,
                                    OutErrorCode))
        {
            return false;
        }
    }
    else
    {
        OutError = FString::Printf(
            TEXT("Unsupported trackType '%s'; supported: opacity, color, translation, scale, angle, shear, transform"), *TrackType);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    if (!Section)
    {
        OutError = TEXT("Could not create a MovieScene section for the track");
        OutErrorCode = TEXT("ANIMATION_ERROR");
        return false;
    }
    Out.TrackClass = Section->GetOuter() ? Section->GetOuter()->GetClass()->GetName() : FString();
    const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
    if (!Range.Contains(Frame))
    {
        const FFrameNumber LowerBound = FMath::Min(Range.GetLowerBoundValue(), Frame);
        const FFrameNumber UpperBound = FMath::Max(Range.GetUpperBoundValue(), Frame + FFrameNumber(1));
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>(LowerBound, UpperBound));
    }
    return true;
}
} // namespace WidgetAuthoringHelpers
