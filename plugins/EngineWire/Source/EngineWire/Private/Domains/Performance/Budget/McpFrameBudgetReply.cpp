// Copyright (c) 2024 EngineWire Contributors
//
// fit_frame_budget's pick and reply (McpFrameBudget.h).

#include "Domains/Performance/Budget/McpFrameBudget.h"

#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "EditorViewportClient.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Optimize/McpChoiceKnapsack.h"

namespace McpFrameBudget
{
namespace
{
// A gain smaller than this, or than twice the timing noise, is not counted: it cannot be told from the noise.
constexpr double MinGainMs = 0.05;
constexpr int32 NamesPerKnob = 8;

double BudgetRound(double Value)
{
    return FMath::RoundToDouble(Value * 100.0) / 100.0;
}

void AddBudgetWarning(const TSharedPtr<FJsonObject>& Result, const FString& Text)
{
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Held = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Held))
    {
        Warnings = *Held;
    }
    Warnings.Add(MakeShared<FJsonValueString>(Text));
    Result->SetArrayField(TEXT("warnings"), Warnings);
}

TSharedPtr<FJsonObject> KnobRow(const FKnob& Knob, bool bChosen)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("kind"), Knob.Kind);
    Row->SetStringField(TEXT("what"), Knob.What);
    TArray<TSharedPtr<FJsonValue>> Names;
    const auto AddName = [&Names](const UActorComponent* Component) {
        const AActor* Owner = Component ? Component->GetOwner() : nullptr;
        if (Owner && Names.Num() < NamesPerKnob)
        {
            Names.Add(MakeShared<FJsonValueString>(Owner->GetActorLabel()));
        }
    };
    for (const TWeakObjectPtr<ULightComponent>& Light : Knob.Lights)
    {
        AddName(Light.Get());
    }
    for (const TWeakObjectPtr<UPrimitiveComponent>& Mesh : Knob.Meshes)
    {
        AddName(Mesh.Get());
    }
    Row->SetArrayField(TEXT("actors"), Names);
    if (Knob.Ms < 0.0)
    {
        Row->SetBoolField(TEXT("measured"), false); // out of time before its turn
        return Row;
    }
    Row->SetNumberField(TEXT("gainMs"), BudgetRound(Knob.GainMs));
    Row->SetNumberField(TEXT("changed"), Knob.Changed);
    Row->SetBoolField(TEXT("chosen"), bChosen);
    return Row;
}
} // namespace

void Pick(FRun& Run)
{
    // The timing noise of a difference between two measures, as a gain is one: 1.4826 times the median change between
    // consecutive measures with every knob off (a standard deviation, were it normal). The spread of those measures as a
    // whole counted slow drift too (a baseline that rose 1.5 ms over a run read 0.79 ms of noise and hid 2 ms gains),
    // and drift cancels in a gain measured on and off a moment apart.
    const double Base = Run.BaselineMs();
    TArray<double> Steps;
    for (int32 Index = 1; Index < Run.Bases.Num(); ++Index)
    {
        Steps.Add(FMath::Abs(Run.Bases[Index] - Run.Bases[Index - 1]));
    }
    Steps.Sort();
    Run.NoiseMs = Steps.Num() > 0 ? 1.4826 * Steps[Steps.Num() / 2] : 0.0;
    // Under three times that noise a gain cannot be told from it.
    const double Floor = FMath::Max(MinGainMs, 3.0 * Run.NoiseMs);
    TArray<McpChoiceKnapsack::FOption> Options;
    TArray<int32> KnobOf;
    for (int32 Index = 0; Index < Run.Knobs.Num(); ++Index)
    {
        FKnob& Knob = Run.Knobs[Index];
        if (Knob.Ms < 0.0 || Knob.OffMs < 0.0)
        {
            continue;
        }
        // Off less on, measured a moment apart: drift over the run (the GPU warming, clocks changing) cancels.
        Knob.GainMs = Knob.OffMs - Knob.Ms;
        if (Knob.GainMs >= Floor)
        {
            // 0.01 a knob: of two picks that change the picture alike, the one with fewer knobs.
            Options.Add({Knob.Group, Knob.GainMs, Knob.Changed + 0.01});
            KnobOf.Add(Index);
        }
    }
    const TArray<int32> Picked = McpChoiceKnapsack::Pick(Options, Base - Run.BudgetMs, 0.01, Run.bReachable);
    Run.Chosen.Reset();
    for (const int32 Option : Picked)
    {
        Run.Chosen.Add(KnobOf[Option]);
    }
    if (!Run.bReachable)
    {
        // Out of reach: the largest gain of each group, measured together, says how close the knobs come.
        TMap<int32, int32> Largest;
        for (int32 Option = 0; Option < Options.Num(); ++Option)
        {
            const int32* Held = Largest.Find(Options[Option].Group);
            if (!Held || Options[*Held].Gain < Options[Option].Gain)
            {
                Largest.Add(Options[Option].Group, Option);
            }
        }
        for (const TPair<int32, int32>& Entry : Largest)
        {
            Run.Chosen.Add(KnobOf[Entry.Value]);
        }
        Run.Chosen.Sort();
    }
}

TSharedPtr<FJsonObject> Reply(const FRun& Run, int32 Kept, FString& OutMessage)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const double Baseline = Run.BaselineMs();
    Result->SetNumberField(TEXT("budgetMs"), BudgetRound(Run.BudgetMs));
    Result->SetNumberField(TEXT("baselineMs"), BudgetRound(Baseline));
    const double Fps = Run.FramesDrawn / FMath::Max(FPlatformTime::Seconds() - Run.Started, 0.001);
    Result->SetNumberField(TEXT("fps"), FMath::RoundToDouble(Fps));
    // A frame far longer than its GPU time waits on the editor's own CPU work: the GPU time then holds idle gaps.
    if (Fps > 0.0 && 1000.0 / Fps > 2.5 * Baseline)
    {
        AddBudgetWarning(Result, FString::Printf(TEXT("The editor's frame takes %.1f ms but its GPU work %.2f ms: the frame waits on the "
                                                      "editor's own CPU work, so GPU times here hold idle gaps and gains under a few tenths of "
                                                      "a millisecond are uncertain. A GPU-bound frame (a heavier view, a game at its own "
                                                      "quality) measures them better."), 1000.0 / Fps, Baseline));
    }
    Result->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(Run.Client->GetViewLocation()));
    Result->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(Run.Client->GetViewRotation()));
    if (Run.bNoDynamicShadows)
    {
        AddBudgetWarning(Result, TEXT("Lights were not tried: at the editor's shadow quality (scalability Low) no dynamic shadows are "
                                      "drawn, so their cost is missing from this measure. A game drawn at a higher quality pays for "
                                      "them: raise the Shadow group (set_scalability) to measure them, then set it back."));
    }
    if (Baseline <= Run.BudgetMs)
    {
        Result->SetBoolField(TEXT("met"), true);
        Result->SetBoolField(TEXT("applied"), false);
        OutMessage = FString::Printf(TEXT("The view draws in %.2f ms on the GPU, within the %.2f ms budget: nothing to change."), Baseline, Run.BudgetMs);
        return Result;
    }
    Result->SetNumberField(TEXT("noiseMs"), BudgetRound(Run.NoiseMs));
    TArray<TSharedPtr<FJsonValue>> Rows;
    double Saved = 0.0;
    for (int32 Index = 0; Index < Run.Knobs.Num(); ++Index)
    {
        const bool bChosen = Run.Chosen.Contains(Index);
        Saved += bChosen ? Run.Knobs[Index].GainMs : 0.0;
        Rows.Add(MakeShared<FJsonValueObject>(KnobRow(Run.Knobs[Index], bChosen)));
    }
    Result->SetArrayField(TEXT("knobs"), Rows);
    const bool bMet = Run.ChosenMs >= 0.0 && Run.ChosenMs <= Run.BudgetMs;
    Result->SetBoolField(TEXT("met"), bMet && Run.bReachable);
    Result->SetBoolField(TEXT("applied"), Kept > 0);
    if (Run.bDryRun)
    {
        Result->SetBoolField(TEXT("dryRun"), true);
    }
    if (Run.Chosen.Num() == 0)
    {
        OutMessage = FString::Printf(TEXT("The view draws in %.2f ms on the GPU; none of the %d knob(s) tried saves a measurable amount, so the "
                                          "%.2f ms budget is out of their reach: nothing changed."), Baseline, Run.Knobs.Num(), Run.BudgetMs);
        return Result;
    }
    Result->SetNumberField(TEXT("predictedMs"), BudgetRound(Baseline - Saved));
    Result->SetNumberField(TEXT("measuredMs"), BudgetRound(Run.ChosenMs));
    if (Run.Change.IsValid())
    {
        Result->SetObjectField(TEXT("change"), Run.Change);
    }
    const double Changed = Run.Change.IsValid() ? Run.Change->GetNumberField(TEXT("changed")) : 0.0;
    if (!Run.bReachable)
    {
        OutMessage = FString::Printf(TEXT("The view draws in %.2f ms on the GPU; every knob that saves time, together, brings it to %.2f ms, "
                                          "short of the %.2f ms budget: nothing changed."), Baseline, Run.ChosenMs, Run.BudgetMs);
        return Result;
    }
    const FString Outcome = Kept > 0   ? FString(TEXT("kept as one undo step"))
                            : Run.bDryRun ? FString(TEXT("dryRun, nothing changed"))
                                          : FString(TEXT("nothing changed"));
    OutMessage = FString::Printf(TEXT("The view draws in %.2f ms on the GPU against a %.2f ms budget: %d knob(s) bring it to %.2f ms "
                                      "(predicted %.2f), changing %.1f%% of the picture; %s."),
                                 Baseline, Run.BudgetMs, Run.Chosen.Num(), Run.ChosenMs, Baseline - Saved, Changed, *Outcome);
    if (!bMet)
    {
        AddBudgetWarning(Result, FString::Printf(TEXT("Together the knobs measured %.2f ms, over the budget their separate gains "
                                                      "promised (%.2f ms): their savings overlap. Call again with a lower budgetMs to "
                                                      "pick more."), Run.ChosenMs, Baseline - Saved));
    }
    return Result;
}
} // namespace McpFrameBudget
