// Copyright (c) 2024 EngineWire Contributors
//
// auto_edit's costs (McpAutoEditScore.h): a camera's shot of the subjects at one time, and a cut between two cameras.

#include "Domains/Sequence/Planner/McpAutoEditScore.h"

#include "Domains/Sequence/Planner/McpFrameShotCamera.h"
#include "Engine/World.h"

namespace McpAutoEdit
{
namespace
{
constexpr double EditMainWeight = 2.0;   // the main subject's hidden share counts double
constexpr double EditOtherWeight = 0.5;
constexpr double EditSmallest = 0.08;    // the main subject spans less of the frame's height than this: too far
constexpr double EditLargest = 1.5;      // more than this: too close to read
constexpr double EditCutCost = 0.3;
constexpr double EditJumpCost = 1.0;     // two cameras within 30 degrees of each other round the main subject
constexpr double EditCrossCost = 2.0;    // two cameras on either side of the line through the first two subjects
} // namespace

FShotScore ScoreShot(const UWorld& World, const FTransform& Camera, const McpFrameShot::FLens& Lens, const TArray<FSubjectAt>& Subjects)
{
    FShotScore Score;
    for (int32 Index = 0; Index < Subjects.Num(); ++Index)
    {
        const FSubjectAt& Subject = Subjects[Index];
        FVector2D Screen;
        const bool bInFrame = McpFrameShot::Project(Subject.Box.GetCenter(), Camera.GetLocation(), Camera.Rotator(), Lens, Screen) &&
                              FMath::Abs(Screen.X) <= 1.0 && FMath::Abs(Screen.Y) <= 1.0;
        const double Hidden = bInFrame ? McpFrameShot::HiddenShare(World, *Subject.Params, Camera.GetLocation(), Subject.Box) : 1.0;
        Score.Cost += (Index == 0 ? EditMainWeight : EditOtherWeight) * Hidden;
        if (Index == 0)
        {
            Score.bMainSeen = Hidden < 1.0;
            // How much of the frame's height the main subject spans, from its bounding sphere: too small reads as a
            // dot, too large as a wall.
            const double Distance = FMath::Max(FVector::Dist(Camera.GetLocation(), Subject.Box.GetCenter()), 1.0);
            const double Share = Subject.Box.GetExtent().Size() / (Distance * Lens.TanHalfV);
            Score.Cost += Share < EditSmallest ? (EditSmallest - Share) * 10.0 : Share > EditLargest ? (Share - EditLargest) * 2.0 : 0.0;
        }
    }
    return Score;
}

double ScoreCut(const FTransform& From, const FTransform& To, const FVector& Main, const TOptional<FVector>& Second)
{
    double Cost = EditCutCost;
    const FVector Before = (From.GetLocation() - Main).GetSafeNormal();
    const FVector After = (To.GetLocation() - Main).GetSafeNormal();
    Cost += FVector::DotProduct(Before, After) > FMath::Cos(FMath::DegreesToRadians(30.0)) ? EditJumpCost : 0.0;
    if (Second.IsSet())
    {
        const FVector Line = Second.GetValue() - Main;
        const double SideBefore = FVector::CrossProduct(Line, From.GetLocation() - Main).Z;
        const double SideAfter = FVector::CrossProduct(Line, To.GetLocation() - Main).Z;
        Cost += SideBefore * SideAfter < 0.0 ? EditCrossCost : 0.0;
    }
    return Cost;
}
} // namespace McpAutoEdit
