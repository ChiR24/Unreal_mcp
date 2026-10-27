#pragma once

#include "CoreMinimal.h"
#include "Curves/RealCurve.h"
#include "Dom/JsonObject.h"
#include "Engine/TimelineTemplate.h"

class UBlueprint;
class UCurveBase;
class UEdGraphNode;

namespace McpBlueprintHandlers {
// Gives a placed Timeline node the timeline it plays, as the editor's menu entry does, and rebuilds its pins (one per
// track): TimelineName (a new Timeline_N when empty) binds a timeline no other node plays, or makes it. Called after
// the overlap check, so a refused placement leaves no timeline behind. Any other node passes untouched. False, with
// OutError and the node taken back out of its graph, outside an Actor Blueprint's event graph, for a name a member
// already uses, or when another node plays that timeline.
bool McpBindTimelineNode(UEdGraphNode *Node, UBlueprint *Blueprint, const FString &TimelineName, FString &OutError);
// Adds timelineName to a node reply when the node is a Timeline.
void McpDescribeTimelineNode(const UEdGraphNode *Node, const TSharedPtr<FJsonObject> &Result);

namespace McpTimeline {
// Indexed by FTTTrackBase::ETrackType, the id the display list (and so the node's pins) uses.
extern const TCHAR *const TrackTypeNames[4];

struct FKeyEdit {
  float Time = 0.f;
  float Value[4] = {0.f, 0.f, 0.f, 1.f};
};

struct FTrackEdit {
  FName Name;
  FTTTrackBase::ETrackType Type = FTTTrackBase::TT_FloatInterp;
  ERichCurveInterpMode Interp = RCIM_Cubic;
  TArray<FKeyEdit> Keys;
};

// Reads tracks[] whole before anything changes; the first bad entry is named by its index (tracks[1].keys[0]).
bool ParseTracks(const TSharedPtr<FJsonObject> &Payload, TArray<FTrackEdit> &Out, FString &OutError);
// The index of the track called Name in its type's list (OutType), INDEX_NONE when the timeline has none.
int32 FindTrack(const UTimelineTemplate *Template, FName Name, FTTTrackBase::ETrackType &OutType);
// The track's own curve; null when it plays a curve asset, whose keys belong to that asset.
UCurveBase *OwnCurve(UTimelineTemplate *Template, FTTTrackBase::ETrackType Type, int32 Index);
// A new track as the editor's "+ Track" button makes it; returns its curve.
UCurveBase *AddTrack(UBlueprint *Blueprint, UTimelineTemplate *Template, const FTrackEdit &Edit);
// The edit's keys replace the curve's.
void WriteKeys(UCurveBase *Curve, const FTrackEdit &Edit);
// Each track as the node shows it: its pin name, type and key count.
TArray<TSharedPtr<FJsonValue>> DescribeTracks(const UTimelineTemplate *Template);
} // namespace McpTimeline
} // namespace McpBlueprintHandlers
