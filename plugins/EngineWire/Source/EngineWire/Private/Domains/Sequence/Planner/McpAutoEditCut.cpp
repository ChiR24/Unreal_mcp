// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpAutoEdit.h"

namespace McpAutoEdit
{
// Dynamic programming over (segment, camera, segments held so far), as Galvane et al. (AAAI 2015) cast continuity
// editing: holding a camera adds its shot cost; cutting adds the cut cost and may only end a shot of at least MinRun
// segments; no shot runs past MaxRun. Ties keep the lower camera index, so the same costs give the same edit.
TArray<int32> Cut(const TArray<TArray<double>>& Shot, TFunctionRef<double(int32 Segment, int32 From, int32 To)> CutCost, int32 MinRun,
                  int32 MaxRun)
{
    const int32 Segments = Shot.Num();
    const int32 Cameras = Segments > 0 ? Shot[0].Num() : 0;
    if (Segments == 0 || Cameras == 0)
    {
        return {};
    }
    MinRun = FMath::Clamp(MinRun, 1, Segments);
    MaxRun = FMath::Max(MaxRun, MinRun);
    const double Never = TNumericLimits<double>::Max() / 4.0;
    const int32 Width = Cameras * (MaxRun + 1);
    const auto At = [Width, MaxRun](int32 Segment, int32 Camera, int32 Run) { return Segment * Width + Camera * (MaxRun + 1) + Run; };
    TArray<double> Best;
    Best.Init(Never, Segments * Width);
    TArray<int32> From; // the camera the shot before came from, for a run of 1
    From.Init(INDEX_NONE, Segments * Width);
    for (int32 Camera = 0; Camera < Cameras; ++Camera)
    {
        Best[At(0, Camera, 1)] = Shot[0][Camera];
    }
    for (int32 Segment = 1; Segment < Segments; ++Segment)
    {
        for (int32 Camera = 0; Camera < Cameras; ++Camera)
        {
            for (int32 Run = 2; Run <= MaxRun; ++Run) // held on
            {
                const double Before = Best[At(Segment - 1, Camera, Run - 1)];
                Best[At(Segment, Camera, Run)] = Before < Never ? Before + Shot[Segment][Camera] : Never;
            }
            for (int32 Previous = 0; Previous < Cameras; ++Previous) // cut to it from a shot long enough
            {
                for (int32 Run = MinRun; Run <= MaxRun && Previous != Camera; ++Run)
                {
                    const double Before = Best[At(Segment - 1, Previous, Run)];
                    const double Through = Before < Never ? Before + CutCost(Segment, Previous, Camera) + Shot[Segment][Camera] : Never;
                    if (Through < Best[At(Segment, Camera, 1)])
                    {
                        Best[At(Segment, Camera, 1)] = Through;
                        From[At(Segment, Camera, 1)] = Previous * (MaxRun + 1) + Run;
                    }
                }
            }
        }
    }
    // The last shot must be long enough too, unless the whole range is shorter than one shot.
    int32 Camera = INDEX_NONE;
    int32 Run = 0;
    double Least = Never;
    for (int32 Option = 0; Option < Cameras; ++Option)
    {
        for (int32 Held = FMath::Min(MinRun, Segments); Held <= MaxRun; ++Held)
        {
            if (Best[At(Segments - 1, Option, Held)] < Least)
            {
                Least = Best[At(Segments - 1, Option, Held)];
                Camera = Option;
                Run = Held;
            }
        }
    }
    if (Camera == INDEX_NONE)
    {
        return {};
    }
    TArray<int32> Out;
    Out.Init(INDEX_NONE, Segments);
    for (int32 Segment = Segments - 1; Segment >= 0; --Segment)
    {
        Out[Segment] = Camera;
        if (Run > 1)
        {
            --Run;
            continue;
        }
        const int32 Came = From[At(Segment, Camera, 1)];
        if (Came == INDEX_NONE)
        {
            break; // segment 0
        }
        Camera = Came / (MaxRun + 1);
        Run = Came % (MaxRun + 1);
    }
    return Out;
}
} // namespace McpAutoEdit
