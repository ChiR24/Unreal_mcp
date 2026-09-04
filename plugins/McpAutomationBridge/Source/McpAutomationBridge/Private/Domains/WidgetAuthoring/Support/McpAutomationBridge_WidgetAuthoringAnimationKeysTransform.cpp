// McpAutomationBridge_WidgetAuthoringAnimationKeysTransform.cpp — RenderTransform keys for widget animations.
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeysInternal.h"

#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"

namespace WidgetAuthoringHelpers
{
using namespace WidgetAnimationKeys;

bool McpAuthorTransformKeys(UMovieScene* MovieScene, const FGuid& Guid, FFrameNumber Frame, const FString& Kind,
                            const FString& TrackType, const FString& Interp, const TSharedPtr<FJsonValue>& ValueField,
                            FMcpWidgetKeyResult& Out, UMovieSceneSection*& OutSection, FString& OutError,
                            FString& OutErrorCode)
{
    // UMovieScene2DTransformSection channels: Translation.X/Y (0,1), Angle (2), Scale.X/Y (3,4), Shear.X/Y (5,6).
    UMovieScene2DTransformTrack* Track =
        FindOrAddPropertyTrack<UMovieScene2DTransformTrack>(MovieScene, Guid, TEXT("RenderTransform"), Out.bCreatedTrack);
    UMovieSceneSection* Section = FindOrAddSection(Track);
    OutSection = Section;
    double A = 0.0;
    double B = 0.0;
    int32 Written = 0;
    auto KeyPair = [&](int32 FirstChannel, const TSharedPtr<FJsonValue>& Pair) {
        if (ReadPair(Pair, TEXT("x"), TEXT("y"), A, B))
        {
            AddFloatKey(Section, FirstChannel, Frame, A, Interp);
            Out.KeyCount = AddFloatKey(Section, FirstChannel + 1, Frame, B, Interp);
            Written += 2;
        }
    };
    if (Kind == TEXT("translation") || Kind == TEXT("position"))
    {
        KeyPair(0, ValueField);
    }
    else if (Kind == TEXT("scale"))
    {
        KeyPair(3, ValueField);
    }
    else if (Kind == TEXT("shear"))
    {
        KeyPair(5, ValueField);
    }
    else if (Kind == TEXT("angle") || Kind == TEXT("rotation"))
    {
        if (ValueField.IsValid() && ValueField->TryGetNumber(A))
        {
            Out.KeyCount = AddFloatKey(Section, 2, Frame, A, Interp);
            Written = 1;
        }
    }
    else
    {
        const TSharedPtr<FJsonObject>* Transform = nullptr;
        if (ValueField.IsValid() && ValueField->TryGetObject(Transform) && Transform)
        {
            KeyPair(0, (*Transform)->TryGetField(TEXT("translation")));
            KeyPair(3, (*Transform)->TryGetField(TEXT("scale")));
            KeyPair(5, (*Transform)->TryGetField(TEXT("shear")));
            if ((*Transform)->TryGetNumberField(TEXT("angle"), A))
            {
                Out.KeyCount = AddFloatKey(Section, 2, Frame, A, Interp);
                ++Written;
            }
        }
    }
    if (Written == 0)
    {
        OutError = FString::Printf(
            TEXT("%s keys need an {x,y} pair in propertyValue (angle: a number; transform: {translation,scale,angle,shear})"),
            *TrackType);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    Out.PropertyName = TEXT("RenderTransform");
    Out.ChannelCount = Written;
    return true;
}
} // namespace WidgetAuthoringHelpers
