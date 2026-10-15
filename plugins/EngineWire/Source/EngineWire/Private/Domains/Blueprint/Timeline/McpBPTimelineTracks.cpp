// Timeline tracks for edit_timeline: reading the requested tracks and keys, and writing them the way the editor's
// timeline panel does (a curve per track owned by the generated class, a display entry per track for the node's pin).
#include "Domains/Blueprint/Timeline/McpBPTimeline.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Engine/Blueprint.h"

namespace McpBlueprintHandlers {
namespace McpTimeline {
const TCHAR *const TrackTypeNames[4] = {TEXT("event"), TEXT("float"), TEXT("vector"), TEXT("color")};

namespace {
const TCHAR *const ValueShapes[4] = {TEXT(""), TEXT(" and a number value"), TEXT(" and an {x, y, z} value"),
                                     TEXT(" and an {r, g, b, a} value")};

bool ParseTrackType(FString Text, FTTTrackBase::ETrackType &Out) {
  Text = Text.IsEmpty() ? FString(TEXT("float")) : Text.ToLower();
  for (int32 Index = 0; Index < UE_ARRAY_COUNT(TrackTypeNames); ++Index) {
    if (Text == TrackTypeNames[Index] || (Index == FTTTrackBase::TT_LinearColorInterp && Text == TEXT("linearcolor"))) {
      Out = static_cast<FTTTrackBase::ETrackType>(Index);
      return true;
    }
  }
  return false;
}

// auto is the editor's smooth curve through the keys.
bool ParseInterp(const FString &Text, ERichCurveInterpMode &Out) {
  Out = Text.Equals(TEXT("linear"), ESearchCase::IgnoreCase)     ? RCIM_Linear
        : Text.Equals(TEXT("constant"), ESearchCase::IgnoreCase) ? RCIM_Constant
                                                                 : RCIM_Cubic;
  return Out != RCIM_Cubic || Text.IsEmpty() || Text.Equals(TEXT("auto"), ESearchCase::IgnoreCase);
}

// A key's value by track type: a number, {x,y,z} or [x,y,z], {r,g,b,a} or [r,g,b(,a)] with alpha 1 when left out; an
// event key has none. False for any other shape.
bool ReadKeyValue(FTTTrackBase::ETrackType Type, const TSharedPtr<FJsonValue> &Value, float (&Out)[4]) {
  if (Type == FTTTrackBase::TT_Event) {
    return true;
  }
  double Number = 0.0;
  if (Type == FTTTrackBase::TT_FloatInterp) {
    const bool bRead = Value.IsValid() && Value->TryGetNumber(Number);
    Out[0] = static_cast<float>(Number);
    return bRead;
  }
  static const TCHAR *const Xyz[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
  static const TCHAR *const Rgb[3] = {TEXT("r"), TEXT("g"), TEXT("b")};
  const bool bVector = Type == FTTTrackBase::TT_VectorInterp;
  double Channels[3] = {0.0, 0.0, 0.0};
  if (!(bVector ? ReadJsonTriple(Value, Xyz, Channels) : ReadJsonTriple(Value, Rgb, Channels))) {
    return false;
  }
  const TSharedPtr<FJsonObject> *Object = nullptr;
  const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
  double Alpha = 1.0;
  if (!bVector && Value->TryGetObject(Object)) {
    (*Object)->TryGetNumberField(TEXT("a"), Alpha);
  } else if (!bVector && Value->TryGetArray(Array) && Array->Num() > 3) {
    Alpha = (*Array)[3]->AsNumber();
  }
  for (int32 Index = 0; Index < 3; ++Index) {
    Out[Index] = static_cast<float>(Channels[Index]);
  }
  Out[3] = static_cast<float>(Alpha);
  return true;
}
} // namespace

bool ParseTracks(const TSharedPtr<FJsonObject> &Payload, TArray<FTrackEdit> &Out, FString &OutError) {
  const TArray<TSharedPtr<FJsonValue>> *Tracks = nullptr;
  if (!Payload->TryGetArrayField(TEXT("tracks"), Tracks)) {
    return true;
  }
  for (int32 TrackIndex = 0; TrackIndex < Tracks->Num(); ++TrackIndex) {
    const TSharedPtr<FJsonObject> *Track = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *Keys = nullptr;
    FString Name, Type, Interp;
    const bool bObject = (*Tracks)[TrackIndex]->TryGetObject(Track) && (*Track)->TryGetStringField(TEXT("name"), Name);
    if (bObject) {
      (*Track)->TryGetStringField(TEXT("type"), Type);
      (*Track)->TryGetStringField(TEXT("interpolation"), Interp);
    }
    FTrackEdit &Edit = Out.AddDefaulted_GetRef();
    Edit.Name = FName(*Name.TrimStartAndEnd());
    if (!bObject || Edit.Name.IsNone() || !ParseTrackType(Type, Edit.Type) || !ParseInterp(Interp, Edit.Interp) ||
        !(*Track)->TryGetArrayField(TEXT("keys"), Keys) || Keys->Num() == 0) {
      OutError = FString::Printf(TEXT("tracks[%d] needs a name, keys [{time, value}], a type (float, vector, color or "
                                      "event) and an interpolation (auto, linear or constant)"), TrackIndex);
      return false;
    }
    if (Out.IndexOfByPredicate([&Edit](const FTrackEdit &Other) { return Other.Name == Edit.Name; }) != TrackIndex) {
      OutError = FString::Printf(TEXT("tracks[%d] repeats the track name %s"), TrackIndex, *Edit.Name.ToString());
      return false;
    }
    for (int32 KeyIndex = 0; KeyIndex < Keys->Num(); ++KeyIndex) {
      const TSharedPtr<FJsonObject> *Key = nullptr;
      double Time = -1.0;
      FKeyEdit &Parsed = Edit.Keys.AddDefaulted_GetRef();
      if (!(*Keys)[KeyIndex]->TryGetObject(Key) || !(*Key)->TryGetNumberField(TEXT("time"), Time) || Time < 0.0 ||
          !ReadKeyValue(Edit.Type, (*Key)->TryGetField(TEXT("value")), Parsed.Value)) {
        OutError = FString::Printf(TEXT("tracks[%d].keys[%d] needs a time in seconds (0 or more)%s"), TrackIndex,
                                   KeyIndex, ValueShapes[Edit.Type]);
        return false;
      }
      Parsed.Time = static_cast<float>(Time);
    }
  }
  return true;
}

int32 FindTrack(const UTimelineTemplate *Template, FName Name, FTTTrackBase::ETrackType &OutType) {
  const auto Find = [Name](const auto &Tracks) {
    return Tracks.IndexOfByPredicate([Name](const FTTTrackBase &Track) { return Track.GetTrackName() == Name; });
  };
  const int32 Found[4] = {Find(Template->EventTracks), Find(Template->FloatTracks), Find(Template->VectorTracks),
                          Find(Template->LinearColorTracks)};
  for (int32 Type = 0; Type < UE_ARRAY_COUNT(Found); ++Type) {
    if (Found[Type] != INDEX_NONE) {
      OutType = static_cast<FTTTrackBase::ETrackType>(Type);
      return Found[Type];
    }
  }
  return INDEX_NONE;
}

UCurveBase *OwnCurve(UTimelineTemplate *Template, FTTTrackBase::ETrackType Type, int32 Index) {
  switch (Type) {
  case FTTTrackBase::TT_Event: {
    const FTTEventTrack &Track = Template->EventTracks[Index];
    return Track.bIsExternalCurve ? nullptr : Track.CurveKeys.Get();
  }
  case FTTTrackBase::TT_FloatInterp: {
    const FTTFloatTrack &Track = Template->FloatTracks[Index];
    return Track.bIsExternalCurve ? nullptr : Track.CurveFloat.Get();
  }
  case FTTTrackBase::TT_VectorInterp: {
    const FTTVectorTrack &Track = Template->VectorTracks[Index];
    return Track.bIsExternalCurve ? nullptr : Track.CurveVector.Get();
  }
  default: {
    const FTTLinearColorTrack &Track = Template->LinearColorTracks[Index];
    return Track.bIsExternalCurve ? nullptr : Track.CurveLinearColor.Get();
  }
  }
}

// The curve is owned by the generated class and public, so placed instances can reference it; the display entry gives
// the node the track's pin.
UCurveBase *AddTrack(UBlueprint *Blueprint, UTimelineTemplate *Template, const FTrackEdit &Edit) {
  UClass *Owner = Blueprint->GeneratedClass;
  UCurveBase *Curve = nullptr;
  int32 Index = INDEX_NONE;
  if (Edit.Type == FTTTrackBase::TT_Event) {
    FTTEventTrack Track;
    Track.SetTrackName(Edit.Name, Template);
    Track.CurveKeys = NewObject<UCurveFloat>(Owner, NAME_None, RF_Public);
    Track.CurveKeys->bIsEventCurve = true;
    Curve = Track.CurveKeys.Get();
    Index = Template->EventTracks.Add(Track);
  } else if (Edit.Type == FTTTrackBase::TT_FloatInterp) {
    FTTFloatTrack Track;
    Track.SetTrackName(Edit.Name, Template);
    Track.CurveFloat = NewObject<UCurveFloat>(Owner, NAME_None, RF_Public);
    Curve = Track.CurveFloat.Get();
    Index = Template->FloatTracks.Add(Track);
  } else if (Edit.Type == FTTTrackBase::TT_VectorInterp) {
    FTTVectorTrack Track;
    Track.SetTrackName(Edit.Name, Template);
    Track.CurveVector = NewObject<UCurveVector>(Owner, NAME_None, RF_Public);
    Curve = Track.CurveVector.Get();
    Index = Template->VectorTracks.Add(Track);
  } else {
    FTTLinearColorTrack Track;
    Track.SetTrackName(Edit.Name, Template);
    Track.CurveLinearColor = NewObject<UCurveLinearColor>(Owner, NAME_None, RF_Public);
    Curve = Track.CurveLinearColor.Get();
    Index = Template->LinearColorTracks.Add(Track);
  }
  Template->AddDisplayTrack(FTTTrackId(Edit.Type, Index));
  return Curve;
}

// One channel for float and event tracks, x/y/z or r/g/b/a otherwise.
void WriteKeys(UCurveBase *Curve, const FTrackEdit &Edit) {
  FRichCurve *Channels[4] = {};
  int32 Count = 0;
  if (UCurveFloat *FloatCurve = Cast<UCurveFloat>(Curve)) {
    Channels[Count++] = &FloatCurve->FloatCurve;
  } else if (UCurveVector *VectorCurve = Cast<UCurveVector>(Curve)) {
    for (FRichCurve &Channel : VectorCurve->FloatCurves) {
      Channels[Count++] = &Channel;
    }
  } else if (UCurveLinearColor *ColorCurve = Cast<UCurveLinearColor>(Curve)) {
    for (FRichCurve &Channel : ColorCurve->FloatCurves) {
      Channels[Count++] = &Channel;
    }
  }
  Curve->Modify();
  for (int32 Channel = 0; Channel < Count; ++Channel) {
    Channels[Channel]->Reset();
    for (const FKeyEdit &Key : Edit.Keys) {
      const FKeyHandle Handle = Channels[Channel]->AddKey(Key.Time, Key.Value[Channel]);
      Channels[Channel]->SetKeyInterpMode(Handle, Edit.Interp, false);
    }
    Channels[Channel]->AutoSetTangents();
  }
}

TArray<TSharedPtr<FJsonValue>> DescribeTracks(const UTimelineTemplate *Template) {
  TArray<TSharedPtr<FJsonValue>> Out;
  const auto Add = [&Out](const FTTTrackBase &Track, FTTTrackBase::ETrackType Type, const FRichCurve *Curve) {
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
    Entry->SetStringField(TEXT("type"), TrackTypeNames[Type]);
    Entry->SetNumberField(TEXT("keys"), Curve ? Curve->GetNumKeys() : 0);
    Out.Add(MakeShared<FJsonValueObject>(Entry));
  };
  for (const FTTFloatTrack &Track : Template->FloatTracks) {
    Add(Track, FTTTrackBase::TT_FloatInterp, Track.CurveFloat ? &Track.CurveFloat->FloatCurve : nullptr);
  }
  for (const FTTVectorTrack &Track : Template->VectorTracks) {
    Add(Track, FTTTrackBase::TT_VectorInterp, Track.CurveVector ? &Track.CurveVector->FloatCurves[0] : nullptr);
  }
  for (const FTTLinearColorTrack &Track : Template->LinearColorTracks) {
    Add(Track, FTTTrackBase::TT_LinearColorInterp,
        Track.CurveLinearColor ? &Track.CurveLinearColor->FloatCurves[0] : nullptr);
  }
  for (const FTTEventTrack &Track : Template->EventTracks) {
    Add(Track, FTTTrackBase::TT_Event, Track.CurveKeys ? &Track.CurveKeys->FloatCurve : nullptr);
  }
  return Out;
}
} // namespace McpTimeline
} // namespace McpBlueprintHandlers
