// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFrameShot.h"

#include "Foundation/Optimize/McpNelderMead.h"

namespace McpFrameShot
{
namespace
{
constexpr double ShotPi = 3.14159265358979323846;
constexpr double ShotFramingWeight = 4.0;   // per subject: squared screen distance from its point
constexpr double ShotOcclusionWeight = 3.0; // per subject: share of its sight lines blocked
constexpr double ShotInsideCost = 10.0;
constexpr double ShotElevationWeight = 0.2;
constexpr double ShotPreferredElevation = 0.17; // radians: about 10 degrees above the subject
constexpr double ShotFacingWeight = 0.05;
constexpr double ShotThreeQuarter = 0.866; // cos 30 degrees: a three-quarter view of a front rather than straight on
constexpr double ShotLeadWeight = 0.25;
constexpr double ShotBalanceWeight = 1.0; // two subjects: squared log of their distance ratio; at 0.5, a facing that
                                          // fought the order asked still left one looming and the other far off
constexpr int32 ShotRefined = 4;
constexpr int32 ShotEvaluationsEach = 150;

// The camera-space direction (x ahead, y right, z up) of a screen point.
FVector ScreenRay(const FVector2D& Target, const FLens& Lens)
{
    return FVector(1.0, Target.X * Lens.TanHalfH, Target.Y * Lens.TanHalfV).GetSafeNormal();
}

// The roll-free rotation turning the camera-space ray View onto the world direction Toward: the pitch that makes View
// climb as steeply as Toward, then the yaw that turns it round onto it.
FRotator AimRay(const FVector& Toward, const FVector& View)
{
    const double Reach = FMath::Max(FMath::Sqrt(View.X * View.X + View.Z * View.Z), 1e-9);
    const double Pitch = FMath::Asin(FMath::Clamp(Toward.Z / Reach, -1.0, 1.0)) - FMath::Atan2(View.Z, View.X);
    const double Ahead = View.X * FMath::Cos(Pitch) - View.Z * FMath::Sin(Pitch);
    const double Yaw = FMath::Atan2(Toward.Y, Toward.X) - FMath::Atan2(View.Y, Ahead);
    return FRotator(FMath::RadiansToDegrees(Pitch), FMath::RadiansToDegrees(Yaw), 0.0).GetNormalized();
}

// The share of the frame a box spans: the larger of its height over the frame's and its width over the frame's.
double FillOf(const FSubject& Subject, const FVector& Camera, const FRotator& Rotation, const FLens& Lens)
{
    FVector2D Low(1e9, 1e9);
    FVector2D High(-1e9, -1e9);
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Sign((Corner & 1) ? 1.0 : -1.0, (Corner & 2) ? 1.0 : -1.0, (Corner & 4) ? 1.0 : -1.0);
        FVector2D Screen;
        if (!Project(Subject.Center + Sign * Subject.Extent, Camera, Rotation, Lens, Screen))
        {
            return 10.0; // a corner behind the camera: far too close
        }
        Low = FVector2D(FMath::Min(Low.X, Screen.X), FMath::Min(Low.Y, Screen.Y));
        High = FVector2D(FMath::Max(High.X, Screen.X), FMath::Max(High.Y, Screen.Y));
    }
    return FMath::Max(High.X - Low.X, High.Y - Low.Y) / 2.0;
}

FVector DirectionOf(double Yaw, double Elevation)
{
    const double Up = FMath::Clamp(Elevation, -1.2, 1.45);
    return FVector(FMath::Cos(Up) * FMath::Cos(Yaw), FMath::Cos(Up) * FMath::Sin(Yaw), FMath::Sin(Up));
}

// One subject: the camera along Direction from it, as far as makes its box span Fill of the frame. A box's span on
// screen falls about as one over the distance, so a first guess from its bounding sphere is corrected by that.
FVector AroundOne(const FProblem& Problem, const FVector& Direction)
{
    const FSubject& Subject = Problem.Subjects[0];
    const double Radius = FMath::Max(Subject.Extent.Size(), 1.0);
    double Distance = Radius / (Problem.Fill * Problem.Lens.TanHalfV);
    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        const FVector Camera = Subject.Center + Direction * Distance;
        Distance *= FillOf(Subject, Camera, Aim(Camera, Subject.Center, Problem.Targets[0], Problem.Lens), Problem.Lens) / Problem.Fill;
    }
    return Subject.Center + Direction * FMath::Max(Distance, Radius + 15.0); // outside its bounding sphere, past the near clip
}

// Two subjects A and B: the places that see them Alpha apart, the angle between the rays of their screen points. With
// Beta the angle at A between AB and the camera, the camera is |AB| sin(Alpha + Beta) / sin(Alpha) from A (law of
// sines); Spin turns that plane about AB. T runs Beta across (0, pi - Alpha), short of both ends (on B, on A).
FVector OnTorus(const FProblem& Problem, double Alpha, double T, double Spin)
{
    const FVector A = Problem.Subjects[0].Center;
    const FVector Along = Problem.Subjects[1].Center - A;
    const FVector U = Along.GetSafeNormal();
    const FVector Flat = FVector::CrossProduct(U, FVector::UpVector);
    const FVector Side = Flat.SizeSquared() < 1e-6 ? FVector::CrossProduct(U, FVector::ForwardVector).GetSafeNormal() : Flat.GetSafeNormal();
    const FVector Off = Side * FMath::Cos(Spin) + FVector::CrossProduct(Side, U) * FMath::Sin(Spin);
    const double Beta = (0.05 + 0.9 * FMath::Clamp(T, 0.0, 1.0)) * (ShotPi - Alpha);
    const double Reach = Along.Size() * FMath::Sin(Alpha + Beta) / FMath::Sin(Alpha);
    return A + Reach * (U * FMath::Cos(Beta) + Off * FMath::Sin(Beta));
}

void Place(const FProblem& Problem, double Alpha, const TArray<double>& X, FVector& OutCamera, FRotator& OutRotation)
{
    if (Problem.Subjects.Num() == 1)
    {
        OutCamera = AroundOne(Problem, DirectionOf(X[0], X[1]));
        OutRotation = Aim(OutCamera, Problem.Subjects[0].Center, Problem.Targets[0], Problem.Lens);
        return;
    }
    // Two: the middle of their directions onto the middle of their screen rays; what roll would fix stays as error.
    OutCamera = OnTorus(Problem, Alpha, X[0], X[1]);
    const FVector Toward = (Problem.Subjects[0].Center - OutCamera).GetSafeNormal() + (Problem.Subjects[1].Center - OutCamera).GetSafeNormal();
    const FVector View = ScreenRay(Problem.Targets[0], Problem.Lens) + ScreenRay(Problem.Targets[1], Problem.Lens);
    OutRotation = AimRay(Toward.GetSafeNormal(), View.GetSafeNormal());
}

double Score(const FProblem& Problem, const FVector& Camera, const FRotator& Rotation, FShot* Out)
{
    double Cost = 0.0;
    FVector Middle = FVector::ZeroVector;
    FVector Front = FVector::ZeroVector;
    for (int32 Index = 0; Index < Problem.Subjects.Num(); ++Index)
    {
        const FSubject& Subject = Problem.Subjects[Index];
        FVector2D Screen(10.0, 10.0); // behind the camera: far from every point on screen
        Project(Subject.Center, Camera, Rotation, Problem.Lens, Screen);
        const double Hidden = Problem.Occluded ? Problem.Occluded(Camera, Index) : 0.0;
        Cost += ShotFramingWeight * FVector2D::DistSquared(Screen, Problem.Targets[Index]) + ShotOcclusionWeight * Hidden;
        Middle += Subject.Center / Problem.Subjects.Num();
        Front += Subject.Forward;
        if (Out)
        {
            Out->Screen.Add(Screen);
            Out->Occluded.Add(Hidden);
        }
    }
    const bool bInside = Problem.Inside && Problem.Inside(Camera);
    const FVector Back = (Camera - Middle).GetSafeNormal();
    const double Elevation = FMath::Asin(FMath::Clamp(Back.Z, -1.0, 1.0));
    Cost += (bInside ? ShotInsideCost : 0.0) + ShotElevationWeight * FMath::Square(Elevation - ShotPreferredElevation);
    if (Problem.Subjects.Num() == 2) // a two-shot keeps both about as far away, so neither dwarfs the other
    {
        const double ToFirst = FMath::Max(FVector::Dist(Camera, Problem.Subjects[0].Center), 1.0);
        const double ToSecond = FMath::Max(FVector::Dist(Camera, Problem.Subjects[1].Center), 1.0);
        Cost += ShotBalanceWeight * FMath::Square(FMath::Loge(ToFirst / ToSecond));
    }
    // Seen from the front, three-quarter on; one subject off centre turned toward the open side of the frame (lead room).
    if (Front.Size2D() > 0.3)
    {
        const FVector Ahead = Front.GetSafeNormal2D();
        Cost += ShotFacingWeight * FMath::Square(FVector::DotProduct(Ahead, Back.GetSafeNormal2D()) - ShotThreeQuarter);
        const double Side = Problem.Subjects.Num() == 1 ? Problem.Targets[0].X : 0.0;
        if (FMath::Abs(Side) > 0.1)
        {
            const double Turn = FVector::DotProduct(Ahead, Rotation.RotateVector(FVector::RightVector));
            Cost += ShotLeadWeight * FMath::Max(0.0, Turn * FMath::Sign(Side));
        }
    }
    if (Out)
    {
        Out->bInside = bInside;
        Out->Fill = FillOf(Problem.Subjects[0], Camera, Rotation, Problem.Lens);
    }
    return Cost;
}

TArray<TArray<double>> Starts(const FProblem& Problem)
{
    TArray<TArray<double>> Out;
    if (Problem.Subjects.Num() == 2)
    {
        for (const double T : {0.3, 0.5, 0.7})
        {
            for (int32 Step = 0; Step < 16; ++Step)
            {
                Out.Add({T, Step * ShotPi / 8.0});
            }
        }
        return Out;
    }
    // Fibonacci directions over the sphere, kept from a little below the subject to well above it, and the two
    // three-quarter views of its front.
    constexpr int32 Count = 64;
    const double Golden = ShotPi * (3.0 - FMath::Sqrt(5.0));
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const double Elevation = FMath::Asin(1.0 - (2.0 * Index + 1.0) / Count);
        if (Elevation > -0.18 && Elevation < 1.05)
        {
            Out.Add({Index * Golden, Elevation});
        }
    }
    const double Front = FMath::Atan2(Problem.Subjects[0].Forward.Y, Problem.Subjects[0].Forward.X);
    Out.Add({Front + 0.52, ShotPreferredElevation});
    Out.Add({Front - 0.52, ShotPreferredElevation});
    return Out;
}
} // namespace

FRotator Aim(const FVector& Camera, const FVector& Toward, const FVector2D& Target, const FLens& Lens)
{
    return AimRay((Toward - Camera).GetSafeNormal(), ScreenRay(Target, Lens));
}

bool Project(const FVector& Point, const FVector& Camera, const FRotator& Rotation, const FLens& Lens, FVector2D& OutScreen)
{
    const FVector Local = Rotation.UnrotateVector(Point - Camera);
    if (Local.X <= 1e-3)
    {
        return false;
    }
    OutScreen = FVector2D(Local.Y / (Local.X * Lens.TanHalfH), Local.Z / (Local.X * Lens.TanHalfV));
    return true;
}

FShot Solve(const FProblem& Problem)
{
    const bool bTwo = Problem.Subjects.Num() == 2;
    const double Alpha = bTwo ? FMath::Acos(FMath::Clamp(FVector::DotProduct(ScreenRay(Problem.Targets[0], Problem.Lens),
                                                                              ScreenRay(Problem.Targets[1], Problem.Lens)), -1.0, 1.0))
                              : 0.0;
    int32 Evaluations = 0;
    const auto Cost = [&Problem, Alpha, &Evaluations](const TArray<double>& X)
    {
        FVector Camera;
        FRotator Rotation;
        Place(Problem, Alpha, X, Camera, Rotation);
        ++Evaluations;
        return Score(Problem, Camera, Rotation, nullptr);
    };
    const TArray<TArray<double>> Candidates = Starts(Problem);
    TArray<TPair<double, int32>> Ranked;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        Ranked.Add(TPair<double, int32>(Cost(Candidates[Index]), Index));
    }
    Ranked.Sort([](const TPair<double, int32>& L, const TPair<double, int32>& R) { return L.Key != R.Key ? L.Key < R.Key : L.Value < R.Value; });
    const TArray<double> Steps = bTwo ? TArray<double>{0.1, 0.4} : TArray<double>{0.3, 0.15};
    TArray<double> Best = Candidates[Ranked[0].Value];
    double BestCost = Ranked[0].Key;
    for (int32 Rank = 0; Rank < FMath::Min(ShotRefined, Ranked.Num()); ++Rank)
    {
        const McpNelderMead::FResult Refined = McpNelderMead::Minimize(Cost, Candidates[Ranked[Rank].Value], Steps, 1e-6, ShotEvaluationsEach);
        if (Refined.Value < BestCost)
        {
            BestCost = Refined.Value;
            Best = Refined.Point;
        }
    }
    FShot Shot;
    Place(Problem, Alpha, Best, Shot.Location, Shot.Rotation);
    Shot.Cost = Score(Problem, Shot.Location, Shot.Rotation, &Shot);
    Shot.Evaluations = Evaluations;
    return Shot;
}
} // namespace McpFrameShot
