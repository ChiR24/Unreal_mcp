// McpAutomationBridge_WidgetAuthoringAnimationKeysInternal.h — shared MovieScene helpers for the
// widget animation key authors (AnimationKeys.cpp and AnimationKeysTransform.cpp).
#pragma once

#include "CoreMinimal.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"

namespace WidgetAuthoringHelpers
{
namespace WidgetAnimationKeys
{
template <typename TrackType>
TrackType* FindOrAddPropertyTrack(UMovieScene* MovieScene, const FGuid& Guid, const FName PropertyName, bool& bOutCreated)
{
    TrackType* Track = MovieScene->FindTrack<TrackType>(Guid, PropertyName);
    if (!Track)
    {
        Track = MovieScene->AddTrack<TrackType>(Guid);
        if (Track)
        {
            Track->SetPropertyNameAndPath(PropertyName, PropertyName.ToString());
            bOutCreated = true;
        }
    }
    return Track;
}

inline UMovieSceneSection* FindOrAddSection(UMovieSceneTrack* Track)
{
    if (!Track)
    {
        return nullptr;
    }
    const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
    if (Sections.Num() > 0 && Sections[0])
    {
        return Sections[0];
    }
    UMovieSceneSection* Section = Track->CreateNewSection();
    if (!Section)
    {
        return nullptr;
    }
    Section->SetRange(TRange<FFrameNumber>::All());
    Track->AddSection(*Section);
    return Section;
}

inline int32 AddFloatKey(UMovieSceneSection* Section, int32 ChannelIndex, FFrameNumber Frame, double Value, const FString& Interp)
{
    FMovieSceneFloatChannel* Channel =
        Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(ChannelIndex) : nullptr;
    if (!Channel)
    {
        return 0;
    }
    const float FloatValue = static_cast<float>(Value);
    if (Interp.Equals(TEXT("linear"), ESearchCase::IgnoreCase))
    {
        Channel->AddLinearKey(Frame, FloatValue);
    }
    else if (Interp.Equals(TEXT("constant"), ESearchCase::IgnoreCase))
    {
        Channel->AddConstantKey(Frame, FloatValue);
    }
    else
    {
        Channel->AddCubicKey(Frame, FloatValue);
    }
    return Channel->GetNumKeys();
}

inline bool ReadPair(const TSharedPtr<FJsonValue>& Value, const TCHAR* First, const TCHAR* Second, double& OutA, double& OutB)
{
    if (!Value.IsValid())
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Value->TryGetArray(Array) && Array && Array->Num() >= 2)
    {
        OutA = (*Array)[0]->AsNumber();
        OutB = (*Array)[1]->AsNumber();
        return true;
    }
    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Value->TryGetObject(Object) && Object && (*Object)->HasField(First) && (*Object)->HasField(Second))
    {
        OutA = (*Object)->GetNumberField(First);
        OutB = (*Object)->GetNumberField(Second);
        return true;
    }
    return false;
}
} // namespace WidgetAnimationKeys

// Implemented in McpAutomationBridge_WidgetAuthoringAnimationKeysTransform.cpp: keys on the
// RenderTransform 2D transform track (translation / scale / angle / shear / whole transform).
bool McpAuthorTransformKeys(UMovieScene* MovieScene, const FGuid& Guid, FFrameNumber Frame, const FString& Kind,
                            const FString& TrackType, const FString& Interp, const TSharedPtr<FJsonValue>& ValueField,
                            FMcpWidgetKeyResult& Out, UMovieSceneSection*& OutSection, FString& OutError,
                            FString& OutErrorCode);
}
