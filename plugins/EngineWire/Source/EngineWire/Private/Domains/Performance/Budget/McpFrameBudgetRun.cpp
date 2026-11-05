// Copyright (c) 2024 EngineWire Contributors
//
// fit_frame_budget's run (McpFrameBudget.h), one step a frame on the core ticker: each setting is held for a few frames,
// its GPU frame times read, the picture captured, then the next setting. Each knob is measured on and then off straight
// after, so its gain is the difference of two measures a moment apart.

#include "Domains/Performance/Budget/McpFrameBudget.h"

#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "RHI.h"

namespace McpFrameBudget
{
namespace
{
// The GPU's time for a frame arrives a few frames after it is drawn, and a light whose shadows come back renders its
// shadow maps again: the frames after a change are let drain first.
constexpr int32 WarmFrames = 6;
constexpr int32 SampleFrames = 16;
// A knob turned off again brings its work back: shadow maps rendered anew over several frames. Read early, the off measure
// held that work, every gain came out large and each chosen set measured slower than predicted.
constexpr int32 OffWarmFrames = 16;
// Before the first measure: a GPU woken from idle clocks up over the first second, and the exposure adapts once the view
// draws every frame; measured at once, the two opening baselines differed by 0.8 ms.
constexpr double SettleSeconds = 1.5;
// No knob is begun past this many seconds, leaving time for the chosen set's measure inside the client's 30 s limit.
constexpr double KnobsUntilSeconds = 14.0;

// The lower quartile of a measure's frames: a frame's GPU time grows when the editor's own work leaves the GPU waiting,
// never shrinks, so the low end follows what the frame costs. Medians of 8 frames swung 0.4 ms between two runs.
double LowQuartile(TArray<double> Values)
{
    Values.Sort();
    return Values.Num() > 0 ? Values[Values.Num() / 4] : 0.0;
}

void Answer(FRun& Run, const TFunctionRef<void(UEngineWireSubsystem&)> Send)
{
    if (UEngineWireSubsystem* Self = Run.Self.Get())
    {
        Self->RunLatentReply(Run.RequestId, TEXT("system_control"), [&]() { Send(*Self); });
    }
}

void Fail(FRun& Run, const TCHAR* Code, const FString& Message)
{
    RestoreView(Run);
    Answer(Run, [&](UEngineWireSubsystem& Self) { Self.SendAutomationResponse(Run.Socket, Run.RequestId, false, Message, nullptr, Code); });
}

void Finish(FRun& Run)
{
    RestoreView(Run);
    Answer(Run, [&](UEngineWireSubsystem& Self) {
        int32 Kept = 0;
        FString Code, Detail;
        if (!Run.bDryRun && Run.bReachable && Run.Chosen.Num() > 0)
        {
            Kept = KeepKnobs(Run, Code, Detail);
        }
        if (Kept == INDEX_NONE)
        {
            Self.SendAutomationResponse(Run.Socket, Run.RequestId, false, Detail, nullptr, Code);
            return;
        }
        FString Message;
        const TSharedPtr<FJsonObject> Result = Reply(Run, Kept, Message);
        Self.SendAutomationResponse(Run.Socket, Run.RequestId, true, Message, Result);
    });
}

void Begin(FRun& Run, EStep Step)
{
    Run.Step = Step;
    Run.Frames = 0;
    Run.Samples.Reset();
}

// The share of the picture a setting changed beyond the renderer's noise, and where: against the picture with every knob
// off taken just before (against the opening picture, exposure and a settling scene made every knob read as changing
// most of the frame).
TSharedPtr<FJsonObject> Changed(const FRun& Run, const McpPerception::Diff::FViewMeasure& Frame)
{
    return McpPerception::Diff::CompareFrames(TEXT("baseline"), Run.Pair, Frame);
}

// The picture as the viewport draws it now.
bool Capture(FRun& Run, McpPerception::Diff::FViewMeasure& OutFrame, FString& OutCode, FString& OutError)
{
    McpEnvironmentHandlers::FMcpViewCapture View;
    if (!McpEnvironmentHandlers::McpCaptureView(MakeShared<FJsonObject>(), TEXT("fit_frame_budget"), false, View, OutError, OutCode))
    {
        return false;
    }
    OutFrame = McpPerception::Diff::MeasureFrame(View);
    if (Run.Step != EStep::OpenBase && OutFrame.Size != Run.Baseline.Frame.Size)
    {
        OutCode = TEXT("VIEWPORT_CHANGED");
        OutError = TEXT("The level viewport changed size during the measure; nothing was changed.");
        return false;
    }
    return true;
}

// The next knob on, or, once they are done or time is short, the pick and the chosen knobs on together. False once
// the run has answered.
bool NextKnob(FRun& Run)
{
    if (Run.Knob < Run.Knobs.Num() && FPlatformTime::Seconds() - Run.Started < KnobsUntilSeconds)
    {
        SetKnob(Run.Knobs[Run.Knob], true);
        Begin(Run, EStep::Knob);
        return true;
    }
    Pick(Run);
    if (Run.Chosen.Num() == 0)
    {
        Finish(Run);
        return false;
    }
    for (const int32 Index : Run.Chosen)
    {
        SetKnob(Run.Knobs[Index], true);
    }
    Begin(Run, EStep::Chosen);
    return true;
}
} // namespace

bool Tick(const TSharedRef<FRun>& RunRef)
{
    FRun& Run = *RunRef;
    if (!Run.Self.IsValid())
    {
        RestoreView(Run);
        return false;
    }
    if (!ClientAlive(Run) || GEditor->PlayWorld)
    {
        Fail(Run, TEXT("VIEWPORT_CHANGED"), TEXT("The level viewport closed or Play started during the measure; nothing was changed."));
        return false;
    }
    ++Run.FramesDrawn;
    const bool bSettling = Run.Step == EStep::OpenBase && FPlatformTime::Seconds() - Run.Started < SettleSeconds;
    if (++Run.Frames <= (Run.Step == EStep::KnobOff ? OffWarmFrames : WarmFrames) || bSettling)
    {
        return true;
    }
    Run.Samples.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
    if (Run.Samples.Num() < SampleFrames)
    {
        return true;
    }
    const double Ms = LowQuartile(Run.Samples);
    McpPerception::Diff::FViewMeasure Frame;
    FString Code, Error;
    if (!Capture(Run, Frame, Code, Error))
    {
        Fail(Run, *Code, Error);
        return false;
    }
    switch (Run.Step)
    {
    case EStep::OpenBase:
        if (Ms <= 0.0)
        {
            Fail(Run, TEXT("GPU_TIMING_UNAVAILABLE"), TEXT("The GPU's frame time reads 0 here, so nothing can be measured: turn GPU timing "
                                                            "on (system_control enable_gpu_timing) and call again."));
            return false;
        }
        Run.Bases.Add(Ms);
        Run.Baseline.Frame = MoveTemp(Frame);
        Begin(Run, EStep::NoiseBase);
        return true;
    case EStep::NoiseBase:
        Run.Bases.Add(Ms);
        McpPerception::Diff::BlockChange(Run.Baseline.Frame, Frame, Run.Baseline.NoiseLoss, Run.Baseline.NoiseColour);
        if (Run.BaselineMs() <= Run.BudgetMs)
        {
            Finish(Run); // within the budget already: nothing to try
            return false;
        }
        Run.Pair.NoiseLoss = Run.Baseline.NoiseLoss;
        Run.Pair.NoiseColour = Run.Baseline.NoiseColour;
        Run.Pair.Frame = MoveTemp(Frame); // every knob off, just taken: the first knob's pair
        Run.Knob = 0;
        return NextKnob(Run);
    case EStep::Knob:
    {
        FKnob& Knob = Run.Knobs[Run.Knob];
        Knob.Ms = Ms;
        Knob.Changed = Changed(Run, Frame)->GetNumberField(TEXT("changed"));
        SetKnob(Knob, false);
        Begin(Run, EStep::KnobOff);
        return true;
    }
    case EStep::KnobOff:
        Run.Knobs[Run.Knob].OffMs = Ms;
        Run.Bases.Add(Ms);
        Run.Pair.Frame = MoveTemp(Frame); // every knob off again: the next knob's pair
        ++Run.Knob;
        return NextKnob(Run);
    case EStep::Chosen:
        Run.ChosenMs = Ms;
        Run.Change = Changed(Run, Frame);
        Finish(Run); // RestoreView turns the picked knobs back off; KeepKnobs then keeps them as an undo step
        return false;
    }
    return false;
}
} // namespace McpFrameBudget
