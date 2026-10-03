// McpAutomationBridge_WidgetAuthoringAnimationKeys.cpp — MovieScene key authoring for widget animations.
//
// Dogfood #38: add_animation_keyframe used to refuse with NOT_SUPPORTED. It now finds or creates the
// widget binding + property track, adds a section, and writes real channel keys for RenderOpacity,
// ColorAndOpacity and (in AnimationKeysTransform.cpp) the RenderTransform.
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeysInternal.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Components/Widget.h"
#include "Animation/MovieScene2DTransformTrack.h"
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
} // namespace

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

// Checked before anything is touched: a refused key used to leave a new binding and an empty track
// behind (a scale key without an {x,y} pair created both on CoinBox, then answered INVALID_ARGUMENT).
FString KeyValueError(const FString& Kind, const FString& TrackType, const TSharedPtr<FJsonValue>& Value)
{
    double A = 0.0;
    double B = 0.0;
    const TSharedPtr<FJsonObject>* Object = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Kind == TEXT("opacity") || Kind == TEXT("renderopacity") || Kind == TEXT("float"))
    {
        return Value.IsValid() && Value->TryGetNumber(A) ? FString()
                                                         : FString(TEXT("opacity keys need a numeric value (0..1) in propertyValue or value"));
    }
    if (Kind == TEXT("color") || Kind == TEXT("colorandopacity") || Kind == TEXT("tint"))
    {
        return Value.IsValid() && ((Value->TryGetObject(Object) && Object) || (Value->TryGetArray(Array) && Array && Array->Num() >= 3))
                   ? FString()
                   : FString(TEXT("color keys need a {r,g,b,a} object or [r,g,b,a] array in propertyValue"));
    }
    if (!IsTransformKind(Kind))
    {
        return FString::Printf(
            TEXT("Unsupported trackType '%s'; supported: opacity, color, translation, scale, angle, shear, transform"), *TrackType);
    }
    bool bValid = false;
    if (Kind == TEXT("angle") || Kind == TEXT("rotation"))
    {
        bValid = Value.IsValid() && Value->TryGetNumber(A);
    }
    else if (Kind != TEXT("transform") && Kind != TEXT("rendertransform"))
    {
        bValid = ReadPair(Value, TEXT("x"), TEXT("y"), A, B);
    }
    else if (Value.IsValid() && Value->TryGetObject(Object) && Object)
    {
        bValid = ReadPair((*Object)->TryGetField(TEXT("translation")), TEXT("x"), TEXT("y"), A, B) ||
                 ReadPair((*Object)->TryGetField(TEXT("scale")), TEXT("x"), TEXT("y"), A, B) ||
                 ReadPair((*Object)->TryGetField(TEXT("shear")), TEXT("x"), TEXT("y"), A, B) ||
                 (*Object)->TryGetNumberField(TEXT("angle"), A);
    }
    return bValid ? FString()
                  : FString::Printf(TEXT("%s keys need an {x,y} pair in propertyValue (angle: a number; transform: "
                                         "{translation,scale,angle,shear})"), *TrackType);
}

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
    OutError = KeyValueError(Kind, TrackType, ValueField);
    if (!OutError.IsEmpty())
    {
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
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
bool McpAddWidgetAnimationTrack(UWidgetAnimation* Animation, UWidget* Target, const FString& TrackType,
                                FMcpWidgetKeyResult& Out, FString& OutError)
{
    UMovieScene* MovieScene = Animation ? Animation->GetMovieScene() : nullptr;
    if (!MovieScene || !Target)
    {
        OutError = TEXT("The animation has no MovieScene or the target widget is missing");
        return false;
    }
    const FString Kind = TrackType.ToLower();
    // The kind is checked before the binding is made, so a refused track leaves nothing behind.
    if (Kind != TEXT("opacity") && Kind != TEXT("renderopacity") && Kind != TEXT("color") && !IsTransformKind(Kind))
    {
        OutError = FString::Printf(TEXT("trackType '%s' is not one of opacity, color, translation, scale, angle, shear, transform"), *TrackType);
        return false;
    }
    MovieScene->Modify();
    Animation->Modify();
    UMovieSceneTrack* Track = nullptr;
    bool bBinding = false;
    const FGuid Guid = FindOrCreateBinding(MovieScene, Animation, Target, bBinding);
    if (Kind == TEXT("opacity") || Kind == TEXT("renderopacity"))
    {
        Track = FindOrAddPropertyTrack<UMovieSceneFloatTrack>(MovieScene, Guid, TEXT("RenderOpacity"), Out.bCreatedTrack);
        Out.PropertyName = TEXT("RenderOpacity");
    }
    else if (Kind == TEXT("color"))
    {
        Track = FindOrAddPropertyTrack<UMovieSceneColorTrack>(MovieScene, Guid, TEXT("ColorAndOpacity"), Out.bCreatedTrack);
        Out.PropertyName = TEXT("ColorAndOpacity");
    }
    else
    {
        Track = FindOrAddPropertyTrack<UMovieScene2DTransformTrack>(MovieScene, Guid, TEXT("RenderTransform"), Out.bCreatedTrack);
        Out.PropertyName = TEXT("RenderTransform");
    }
    Out.bCreatedBinding = bBinding;
    Out.BindingGuid = Guid.ToString();
    Out.TrackType = Kind;
    Out.TrackClass = Track ? Track->GetClass()->GetName() : FString();
    return FindOrAddSection(Track) != nullptr;
}
} // namespace WidgetAuthoringHelpers
