// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Audio/McpAcoustics.h"

#include "Math/RandomStream.h"

namespace McpAcoustics
{
namespace
{
constexpr int32 AcousticRays = 256;
constexpr double AcousticPi = 3.14159265358979323846;
constexpr double AcousticSpeed = 34300.0;  // cm a second
constexpr double AcousticScatter = 0.3;    // share of reflections scattered (Lambert) rather than mirrored
constexpr double AcousticMinCos = 0.2;     // a grazing hit's surface share is capped here
constexpr double AcousticBin = 0.01;       // seconds per energy bin
constexpr int32 AcousticBins = 2000;       // 20 s, the longest decay a Reverb Effect holds
constexpr double AcousticFloor = 1e-6;     // a ray is followed until its energy is 60 dB down
constexpr double AcousticOffset = 0.5;     // cm off a surface before the next ray leaves it

FVector AcousticDirection(int32 Index)
{
    const double Z = 1.0 - (2.0 * Index + 1.0) / AcousticRays;
    const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
    const double Angle = AcousticPi * (3.0 - FMath::Sqrt(5.0)) * Index;
    return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Z);
}

FVector AcousticLambert(const FVector& Normal, FRandomStream& Random)
{
    const double U = Random.FRand(), V = Random.FRand();
    FVector Tangent, Bitangent;
    Normal.FindBestAxisVectors(Tangent, Bitangent);
    return (Tangent * FMath::Sqrt(U) * FMath::Cos(2.0 * AcousticPi * V) + Bitangent * FMath::Sqrt(U) * FMath::Sin(2.0 * AcousticPi * V) +
            Normal * FMath::Sqrt(1.0 - U)).GetSafeNormal();
}

// The walls' extent along the room's own floor axes and up: of the yaws in 1-degree steps over 90, the one whose
// rectangle around the hits' floor plan is smallest, so a room turned on the floor reads its own length and width.
void AcousticExtents(const TArray<FVector>& Points, FRoom& Room)
{
    double BestArea = TNumericLimits<double>::Max();
    for (int32 Degree = 0; Degree < 90; ++Degree)
    {
        const double C = FMath::Cos(Degree * AcousticPi / 180.0), S = FMath::Sin(Degree * AcousticPi / 180.0);
        double MinU = TNumericLimits<double>::Max(), MaxU = -MinU, MinV = MinU, MaxV = -MinU;
        for (const FVector& Point : Points)
        {
            const double U = Point.X * C + Point.Y * S, V = Point.Y * C - Point.X * S;
            MinU = FMath::Min(MinU, U);
            MaxU = FMath::Max(MaxU, U);
            MinV = FMath::Min(MinV, V);
            MaxV = FMath::Max(MaxV, V);
        }
        if ((MaxU - MinU) * (MaxV - MinV) < BestArea)
        {
            BestArea = (MaxU - MinU) * (MaxV - MinV);
            Room.LengthM = FMath::Max(MaxU - MinU, MaxV - MinV) / 100.0;
            Room.WidthM = FMath::Min(MaxU - MinU, MaxV - MinV) / 100.0;
        }
    }
    Room.HeightM = (Room.Bounds.Max.Z - Room.Bounds.Min.Z) / 100.0;
    const double Smallest = FMath::Max(FMath::Min3(Room.LengthM, Room.WidthM, Room.HeightM), 0.01);
    Room.Proportion = FMath::Max3(Room.LengthM, Room.WidthM, Room.HeightM) / Smallest;
}

// T30 (or T20) off the Schroeder curve of the energy arriving in each bin: unset when it falls less than 25 dB.
TOptional<double> AcousticDecay(const TArray<double>& Energy)
{
    TArray<double> Remaining;
    Remaining.Init(0.0, Energy.Num());
    double Sum = 0.0;
    for (int32 Bin = Energy.Num() - 1; Bin >= 0; --Bin)
    {
        Sum += Energy[Bin];
        Remaining[Bin] = Sum;
    }
    if (Sum <= 0.0)
    {
        return TOptional<double>();
    }
    const auto Reached = [&](double Db) -> TOptional<double>
    {
        for (int32 Bin = 0; Bin < Remaining.Num(); ++Bin)
        {
            if (Remaining[Bin] <= 0.0 || 10.0 * FMath::LogX(10.0, Remaining[Bin] / Sum) <= Db)
            {
                return Bin * AcousticBin;
            }
        }
        return TOptional<double>();
    };
    const TOptional<double> Start = Reached(-5.0), T30 = Reached(-35.0), T20 = Reached(-25.0);
    if (Start.IsSet() && T30.IsSet())
    {
        return 2.0 * (T30.GetValue() - Start.GetValue());
    }
    return Start.IsSet() && T20.IsSet() ? TOptional<double>(3.0 * (T20.GetValue() - Start.GetValue())) : TOptional<double>();
}
} // namespace

FRoom Estimate(FTrace Trace, const FVector& Listener, double Absorption, double MaxDistance)
{
    FRoom Room;
    TArray<FVector> Points;
    double Volume = 0.0, Surface = 0.0, Nearest = TNumericLimits<double>::Max();
    TArray<double> Energy;
    Energy.Init(0.0, AcousticBins);
    const int32 Bounces = FMath::Min(700, int32(14.0 / Absorption) + 1); // enough to fall 60 dB
    FRandomStream Random(1); // the same rays and the same scattering every call
    for (int32 Ray = 0; Ray < AcousticRays; ++Ray)
    {
        FVector From = Listener, Direction = AcousticDirection(Ray);
        double Carried = 1.0, Seconds = 0.0;
        for (int32 Bounce = 0; Bounce < Bounces && Carried > AcousticFloor; ++Bounce)
        {
            FHit Hit;
            if (!Trace(From, Direction, MaxDistance, Hit))
            {
                Room.OpenShare += Bounce == 0 ? 1.0 / AcousticRays : 0.0;
                break; // the energy leaves the room
            }
            if (Bounce == 0)
            {
                const double Metres = Hit.Distance / 100.0;
                Volume += FMath::Pow(Metres, 3.0);
                Surface += Metres * Metres / FMath::Max(FMath::Abs(FVector::DotProduct(Direction, Hit.Normal)), AcousticMinCos);
                Nearest = FMath::Min(Nearest, Hit.Distance);
                Points.Add(From + Direction * Hit.Distance);
                Room.Bounds += Points.Last();
            }
            Seconds += Hit.Distance / AcousticSpeed;
            From += Direction * Hit.Distance + Hit.Normal * AcousticOffset;
            Direction = Random.FRand() < AcousticScatter ? AcousticLambert(Hit.Normal, Random)
                                                        : Direction - 2.0 * FVector::DotProduct(Direction, Hit.Normal) * Hit.Normal;
            Carried *= 1.0 - Absorption;
            const int32 Bin = int32(Seconds / AcousticBin);
            if (Bin >= AcousticBins)
            {
                break;
            }
            Energy[Bin] += Carried;
        }
    }
    Room.VolumeM3 = Volume * 4.0 * AcousticPi / (3.0 * AcousticRays);
    Room.SurfaceM2 = Surface * 4.0 * AcousticPi / AcousticRays;
    Room.MeanFreePathM = Room.SurfaceM2 > 0.0 ? 4.0 * Room.VolumeM3 / Room.SurfaceM2 : 0.0;
    Room.FirstReflectionMs = Points.Num() > 0 ? 1000.0 * 2.0 * Nearest / AcousticSpeed : 0.0; // out to the nearest wall and back
    if (Points.Num() > 0)
    {
        AcousticExtents(Points, Room);
    }
    if (Room.OpenShare < 0.05 && Room.Proportion < LongRoom && Absorption > 0.0 && Absorption < 1.0 && Room.SurfaceM2 > 0.0)
    {
        Room.EyringSeconds = 0.161 * Room.VolumeM3 / (-Room.SurfaceM2 * FMath::Loge(1.0 - Absorption));
    }
    Room.DecaySeconds = AcousticDecay(Energy);
    return Room;
}
} // namespace McpAcoustics
