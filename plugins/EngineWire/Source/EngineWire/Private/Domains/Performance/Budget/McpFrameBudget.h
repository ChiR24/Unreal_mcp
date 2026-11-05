// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/Perception/McpPerceptionDiff.h"

class FEditorViewportClient;
class FMcpBridgeWebSocket;
class ULightComponent;
class UEngineWireSubsystem;
class UPrimitiveComponent;

// system_control fit_frame_budget (Domains/Performance/Budget): which per-actor knobs bring the level viewport's GPU frame
// time under a budget at the least change to the picture. Each knob is set in the editor world, measured (the median GPU
// time of a few frames) and judged against the baseline picture (the share changed beyond the renderer's own noise,
// describe_view's comparison), then put back; a multiple-choice knapsack picks the cheapest set that saves enough, which
// is measured as a whole and, unless dryRun, kept as one undo step. Measured in the level viewport, not in Play: a knob
// set in the editor world shows at once, where Play copies the world when it starts.
namespace McpFrameBudget
{
/** One way to save time: a light's shadows off, or small meshes culled beyond a distance (the bands share one group). */
struct FKnob
{
    FString Kind; // shadows, draw_distance
    FString What;
    int32 Group = 0;
    TArray<TWeakObjectPtr<ULightComponent>> Lights;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Meshes;
    TArray<float> OldDistance; // per mesh: the draw distance it was drawn with
    float Distance = 0.f;
    bool bOn = false;
    double Ms = -1.0;    // the GPU frame time with it on; -1 until measured
    double OffMs = -1.0; // the same, off again straight after: drift between knobs cancels in the difference
    double GainMs = 0.0;
    double Changed = 0.0; // % of the picture changed beyond noise
};

/** The steps of a run, in order; each ends with a measure. */
enum class EStep : uint8
{
    OpenBase,  // the baseline
    NoiseBase, // the baseline again: the renderer's noise per block
    Knob,      // a knob on
    KnobOff,   // the same knob off again: its gain, a baseline sample, and the picture the next knob is judged against
    Chosen     // the picked knobs together
};

struct FRun
{
    TWeakObjectPtr<UEngineWireSubsystem> Self;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FEditorViewportClient* Client = nullptr;
    FMcpEditorRunHold Hold;
    bool bWasGameView = false;
    bool bDryRun = false;
    bool bNoDynamicShadows = false; // shadow quality 0: the renderer draws no dynamic shadows, so lights are not tried
    double BudgetMs = 0.0;
    double Started = 0.0;
    TArray<FKnob> Knobs;
    EStep Step = EStep::OpenBase;
    int32 Knob = 0;   // the knob being measured
    int32 Frames = 0; // frames drawn since the setting last changed
    TArray<double> Samples;
    TArray<double> Bases; // every measure with every knob off
    double NoiseMs = 0.0;
    int64 FramesDrawn = 0; // over the measures, for the frame rate
    McpPerception::Diff::FSavedView Baseline;
    // The picture with every knob off, taken just before the next knob goes on: each knob is judged against it, since
    // over a run the exposure adapts and a new scene settles, and a knob judged against the opening picture read as
    // changing all of it. Its noise is the baseline's.
    McpPerception::Diff::FSavedView Pair;
    TArray<int32> Chosen;
    bool bReachable = true;
    double ChosenMs = -1.0;
    TSharedPtr<FJsonObject> Change; // what the picked knobs change in the picture

    /** The median of the measures with every knob off. */
    double BaselineMs() const;
};

/** The handler: refusals, the camera and the knobs found, then the run on the core ticker. */
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
/** The viewport and window as the run found them. */
void RestoreView(FRun& Run);
bool ClientAlive(const FRun& Run);

// McpFrameBudgetKnobs.cpp
/** The knobs that could save time in this view: movable shadow-casting lights reaching it (at most 12, largest first),
 *  and the small meshes in it beyond 20, 40 and 80 m. */
void FindKnobs(FRun& Run, bool bShadows, bool bDistance);
/** A knob on or off for a measure only: render state changes, nothing is recorded or marked unsaved. */
void SetKnob(FKnob& Knob, bool bOn);
/** The picked knobs kept as one undo step: their count, or INDEX_NONE with why the step could not be recorded. */
int32 KeepKnobs(const FRun& Run, FString& OutCode, FString& OutDetail);

// McpFrameBudgetRun.cpp
/** One engine frame of the run; false once it has answered. */
bool Tick(const TSharedRef<FRun>& Run);

// McpFrameBudgetReply.cpp
/** Each knob's gain (off less on), the timing noise from the spread of the knob-off measures, and the knapsack's pick
 *  (Run.Chosen, Run.bReachable). */
void Pick(FRun& Run);
TSharedPtr<FJsonObject> Reply(const FRun& Run, int32 Kept, FString& OutMessage);
} // namespace McpFrameBudget
