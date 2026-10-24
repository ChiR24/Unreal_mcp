// Copyright (c) 2024 EngineWire Contributors
//
// arrange_by_relations' model (McpArrange.h): footprints, each relation's miss, the turn facing and against give, and the
// closing snap. Misses are in square metres, so the annealer's temperatures (0.25 down to 0.001) mean the same in a room
// as in a hall: a 50 cm overlap costs 0.25, a 10 cm miss 0.01.

#include "Domains/ControlActor/Relations/McpArrange.h"

namespace McpArrange
{
namespace
{
constexpr double SideShare = 10.0;      // cm two sides must share to stand side by side
constexpr double HeightGrace = 1.0;     // cm of height two boxes may share without overlapping (one rests on the other)
constexpr double FacingGrace = 0.34907; // 20 degrees in radians: a turn within it holds
constexpr double HairGap = 0.5;         // cm a snapped contact keeps: touching shapes read as intersecting
constexpr double EdgeMargin = 10.0;     // cm from a support's edges a subject keeps where the top has room (a lamp on a table)
constexpr double FlushWeight = 0.1;     // per radian squared two sides that touch stand off square: furniture stands flush
constexpr double RightAngle = 1.57079632679489662;

double M2(double Cm)
{
    return FMath::Square(Cm / 100.0);
}

FVector2D Turned(double Yaw, const FVector2D& Local)
{
    const double Cos = FMath::Cos(Yaw);
    const double Sin = FMath::Sin(Yaw);
    return FVector2D(Cos * Local.X - Sin * Local.Y, Sin * Local.X + Cos * Local.Y);
}

// The part of a support's top a subject stands in: inset by up to EdgeMargin wherever the top has that much to spare.
McpBoxSat::FFootprint Top(const McpBoxSat::FFootprint& Self, const McpBoxSat::FFootprint& Support)
{
    const FVector2D AxisX = Turned(Support.Yaw, FVector2D(1.0, 0.0));
    const FVector2D AxisY(-AxisX.Y, AxisX.X);
    McpBoxSat::FFootprint Out = Support;
    Out.Half.X -= FMath::Clamp(0.5 * (Support.Half.X - McpBoxSat::Reach(Self, AxisX)), 0.0, EdgeMargin);
    Out.Half.Y -= FMath::Clamp(0.5 * (Support.Half.Y - McpBoxSat::Reach(Self, AxisY)), 0.0, EdgeMargin);
    return Out;
}

// How far a turn misses the direction it should point, past the grace, in radians.
double TurnMiss(double Yaw, const FVector2D& Want)
{
    const double Off = FMath::Abs(FMath::FindDeltaAngleRadians(Yaw, FMath::Atan2(Want.Y, Want.X)));
    return FMath::Max(0.0, Off - FacingGrace);
}
} // namespace

McpBoxSat::FFootprint Footprint(const FBody& Body)
{
    McpBoxSat::FFootprint Out;
    Out.Center = FVector2D(Body.Location.X, Body.Location.Y) + Turned(Body.Yaw, FVector2D(Body.LocalCenter.X, Body.LocalCenter.Y));
    Out.Half = FVector2D(Body.Half.X, Body.Half.Y);
    Out.Yaw = Body.Yaw;
    return Out;
}

FVector2D Heights(const FBody& Body)
{
    const double Middle = Body.Location.Z + Body.LocalCenter.Z;
    return FVector2D(Middle - Body.Half.Z, Middle + Body.Half.Z);
}

double Miss(const TArray<FBody>& Bodies, const FGoal& Goal, bool bMargins)
{
    const FBody& A = Bodies[Goal.Subject];
    const McpBoxSat::FFootprint FA = Footprint(A);
    const McpBoxSat::FFootprint FB = Footprint(Bodies[Goal.Object]);
    const FVector2D HA = Heights(A);
    const FVector2D HB = Heights(Bodies[Goal.Object]);
    if (Goal.Kind == TEXT("on"))
    {
        return M2(McpBoxSat::Outside(FA, bMargins ? Top(FA, FB) : FB)) + M2(HA.X - HB.Y);
    }
    if (Goal.Kind == TEXT("inside"))
    {
        return M2(McpBoxSat::Outside(FA, FB)) + M2(FMath::Max(0.0, HB.X - HA.X)) + M2(FMath::Max(0.0, HA.Y - HB.Y));
    }
    if (Goal.Kind == TEXT("near"))
    {
        const double Flat = FMath::Max(0.0, McpBoxSat::Separation(FA, FB));
        const double Up = FMath::Max3(0.0, HB.X - HA.Y, HA.X - HB.Y);
        return M2(FMath::Max(0.0, FMath::Sqrt(Flat * Flat + Up * Up) - Goal.Within));
    }
    if (Goal.Kind == TEXT("facing"))
    {
        double Off = 0.0;
        return McpBoxSat::Faces(FA, FB, FacingGrace, Off) ? 0.0 : FMath::Square(Off - FacingGrace);
    }
    // nextTo and against: the boxes touch across one axis and share a side along the other, at a shared height.
    FVector2D Axis = FVector2D::ZeroVector;
    const double Gap = McpBoxSat::Separation(FA, FB, &Axis);
    const FVector2D Side(-Axis.Y, Axis.X);
    const double ReachA = McpBoxSat::Reach(FA, Side);
    const double ReachB = McpBoxSat::Reach(FB, Side);
    const double Shared = ReachA + ReachB - FMath::Abs(FVector2D::DotProduct(FB.Center - FA.Center, Side));
    const double Tall = FMath::Min(HA.Y, HB.Y) - FMath::Max(HA.X, HB.X);
    double Out = M2(Gap) + M2(FMath::Max(0.0, FMath::Min(SideShare, 2.0 * FMath::Min(ReachA, ReachB)) - Shared)) +
                 M2(FMath::Max(0.0, HeightGrace - Tall));
    if (Goal.Kind == TEXT("against"))
    {
        Out += FMath::Square(TurnMiss(A.Yaw, -Axis)); // its back to the other: its front points away
    }
    if (bMargins) // turned square to the other, the two meet face to face rather than at a corner
    {
        const double Turn = FMath::FindDeltaAngleRadians(FB.Yaw, A.Yaw);
        Out += FlushWeight * FMath::Square(Turn - FMath::RoundToDouble(Turn / RightAngle) * RightAngle);
    }
    return Out;
}

namespace
{
// The turn a facing or against goal gives its subject; false for any other goal.
bool WantedYaw(const TArray<FBody>& Bodies, const FGoal& Goal, double& OutYaw)
{
    if (Goal.Kind != TEXT("facing") && Goal.Kind != TEXT("against"))
    {
        return false;
    }
    const McpBoxSat::FFootprint Self = Footprint(Bodies[Goal.Subject]);
    const McpBoxSat::FFootprint Other = Footprint(Bodies[Goal.Object]);
    FVector2D Want = Other.Center - Self.Center;
    const FVector2D Local = Turned(-Other.Yaw, Self.Center - Other.Center);
    if (Goal.Kind == TEXT("against"))
    {
        // Its front away from the face of the other it stands at: that face's outward normal.
        const bool bAlongX = FMath::Abs(Local.X) * Other.Half.Y >= FMath::Abs(Local.Y) * Other.Half.X;
        Want = Turned(Other.Yaw, bAlongX ? FVector2D(FMath::Sign(Local.X), 0.0) : FVector2D(0.0, FMath::Sign(Local.Y)));
    }
    else if (FMath::Abs(Local.Y) <= Other.Half.Y && FMath::Abs(Local.X) > Other.Half.X)
    {
        Want = Turned(Other.Yaw, FVector2D(-FMath::Sign(Local.X), 0.0)); // square to the side it stands beside
    }
    else if (FMath::Abs(Local.X) <= Other.Half.X && FMath::Abs(Local.Y) > Other.Half.Y)
    {
        Want = Turned(Other.Yaw, FVector2D(0.0, -FMath::Sign(Local.Y)));
    }
    if (Want.IsNearlyZero())
    {
        return false;
    }
    OutYaw = FMath::Atan2(Want.Y, Want.X);
    return true;
}
} // namespace

FVector2D SnapBy(const TArray<FBody>& Bodies, const FGoal& Goal)
{
    const McpBoxSat::FFootprint Self = Footprint(Bodies[Goal.Subject]);
    const McpBoxSat::FFootprint Other = Footprint(Bodies[Goal.Object]);
    if (Goal.Kind == TEXT("on"))
    {
        const McpBoxSat::FFootprint Inner = Top(Self, Other);
        const FVector2D AxisX = Turned(Other.Yaw, FVector2D(1.0, 0.0));
        const FVector2D AxisY(-AxisX.Y, AxisX.X);
        const FVector2D Off = Self.Center - Other.Center;
        const double LimitX = FMath::Max(0.0, Inner.Half.X - McpBoxSat::Reach(Self, AxisX));
        const double LimitY = FMath::Max(0.0, Inner.Half.Y - McpBoxSat::Reach(Self, AxisY));
        const double AlongX = FVector2D::DotProduct(Off, AxisX);
        const double AlongY = FVector2D::DotProduct(Off, AxisY);
        return AxisX * (FMath::Clamp(AlongX, -LimitX, LimitX) - AlongX) + AxisY * (FMath::Clamp(AlongY, -LimitY, LimitY) - AlongY);
    }
    if (Goal.Kind == TEXT("nextTo") || Goal.Kind == TEXT("against"))
    {
        FVector2D Axis = FVector2D::ZeroVector;
        const double Gap = McpBoxSat::Separation(Self, Other, &Axis);
        return Axis * (Gap - HairGap);
    }
    return FVector2D::ZeroVector;
}

void Turn(FScene& Scene, int32 Body)
{
    for (const TCHAR* Kind : {TEXT("facing"), TEXT("against")})
    {
        for (const int32 Index : Scene.GoalsOf[Body])
        {
            double Yaw = 0.0;
            if (Scene.Goals[Index].Subject == Body && Scene.Goals[Index].Kind == Kind && WantedYaw(Scene.Bodies, Scene.Goals[Index], Yaw))
            {
                Scene.Bodies[Body].Yaw = Yaw;
            }
        }
    }
}

void Link(FScene& Scene)
{
    TArray<FBody>& Bodies = Scene.Bodies;
    const int32 Count = Bodies.Num();
    Scene.Carried.SetNum(Count);
    Scene.GoalsOf.SetNum(Count);
    Scene.TurnedBy.SetNum(Count);
    Scene.Swaps.SetNum(Count);
    TArray<int32> Parent;
    Parent.Init(INDEX_NONE, Count);
    FBox Reach(ForceInit);
    for (int32 Index = 0; Index < Scene.Goals.Num(); ++Index)
    {
        const FGoal& Goal = Scene.Goals[Index];
        Scene.GoalsOf[Goal.Subject].AddUnique(Index);
        Scene.GoalsOf[Goal.Object].AddUnique(Index);
        if (Goal.Kind == TEXT("on") || Goal.Kind == TEXT("inside"))
        {
            Scene.Joined.Add(PairKey(Goal.Subject, Goal.Object));
        }
        if (Goal.Kind == TEXT("on") && Parent[Goal.Subject] == INDEX_NONE)
        {
            Parent[Goal.Subject] = Goal.Object;
        }
        if (Goal.Kind == TEXT("facing") || Goal.Kind == TEXT("against"))
        {
            Scene.TurnedBy[Goal.Object].AddUnique(Goal.Subject);
            Scene.TurnedBy[Goal.Subject].AddUnique(Goal.Subject);
            Bodies[Goal.Subject].bFreeYaw = false;
        }
        Reach += Bodies[Goal.Subject].Location;
        Reach += Bodies[Goal.Object].Location;
    }
    Scene.Step = FMath::Clamp(0.5 * Reach.GetSize().Size2D(), 50.0, 500.0);
    for (int32 Body = 0; Body < Count; ++Body)
    {
        // A cycle of on goals stops where it closes.
        for (int32 Under = Parent[Body], Depth = 0; Under != INDEX_NONE && Under != Body && Depth < Count; Under = Parent[Under], ++Depth)
        {
            if (Bodies[Body].bMover)
            {
                Scene.Carried[Under].AddUnique(Body);
            }
        }
    }
    for (int32 Pass = 0; Pass < 4; ++Pass) // a stack of four settles from the bottom up
    {
        for (int32 Body = 0; Body < Count; ++Body)
        {
            if (Parent[Body] != INDEX_NONE && Bodies[Body].bMover)
            {
                Bodies[Body].Location.Z += Heights(Bodies[Parent[Body]]).Y - Heights(Bodies[Body]).X;
            }
        }
    }
    for (int32 Body = 0; Body < Count; ++Body)
    {
        if (!Bodies[Body].bMover)
        {
            continue;
        }
        Scene.Movers.Add(Body);
        Turn(Scene, Body);
        for (int32 Other = 0; Other < Count; ++Other)
        {
            if (Other != Body && Bodies[Other].bMover && Bodies[Other].Shape == Bodies[Body].Shape && Parent[Other] == Parent[Body])
            {
                Scene.Swaps[Body].Add(Other);
            }
        }
    }
}
} // namespace McpArrange
