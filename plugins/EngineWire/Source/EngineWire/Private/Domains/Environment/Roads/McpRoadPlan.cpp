// Copyright (c) 2024 EngineWire Contributors
//
// Searching a road on a height grid (McpRoadPlan.h).

#include "Domains/Environment/Roads/McpRoadPlan.h"

#include "HAL/PlatformTime.h"

namespace McpRoadPlan
{
namespace
{
// The 16 headings in angle order: 0, 26.57, 45, 63.43, 90 degrees and on round; slot 16 is the start, entered by none.
// ponytail: straight runs and 18.43-26.57 degree notches, so up a cone-shaped hill whose sides pass about three times the
// grade limit no turn uphill from level stays under it (38% sides at 12% found a way only on some lattices); arc motion
// primitives (Hybrid A* proper) if roads must climb such hills.
constexpr int32 RoadStepX[16] = {1, 2, 1, 1, 0, -1, -1, -2, -1, -2, -1, -1, 0, 1, 1, 2};
constexpr int32 RoadStepY[16] = {0, 1, 1, 2, 1, 2, 1, 1, 0, -1, -1, -2, -1, -2, -1, -1};
constexpr int32 RoadSlots = 17;
constexpr int32 RoadStart = 16;
constexpr double RoadPi = 3.14159265358979323846;
constexpr double RoadNotch = 0.46364760900080611;  // atan(1 / 2): the larger step between two headings
constexpr int32 RoadMaxFrontier = 8000000;           // 128 MB of queued states: past it the search stops

double RoadStepLength(int32 K)
{
    return FMath::Sqrt(static_cast<double>(RoadStepX[K] * RoadStepX[K] + RoadStepY[K] * RoadStepY[K]));
}
} // namespace

double FRoadField::HeightAt(double X, double Y) const
{
    const int32 CX = FMath::Clamp(FMath::FloorToInt(X), 0, Width - 2), CY = FMath::Clamp(FMath::FloorToInt(Y), 0, Height - 2);
    const double U = X - CX, V = Y - CY;
    const int32 I = CY * Width + CX;
    return Heights[I] * (1.0 - U) * (1.0 - V) + Heights[I + 1] * U * (1.0 - V) + Heights[I + Width] * (1.0 - U) * V + Heights[I + Width + 1] * U * V;
}

double CornerRadius(double LengthA, double LengthB, double Turn)
{
    return Turn < 1e-9 ? TNumericLimits<double>::Max() : FMath::Min(LengthA, LengthB) / (2.0 * FMath::Tan(Turn / 2.0));
}

FRun Segment(const FRoadField& Field, const FVector2D& A, const FVector2D& B, double MaxGrade)
{
    FRun Run;
    const double Length = FVector2D::Distance(A, B);
    const int32 Samples = FMath::Max(1, FMath::CeilToInt(Length * 4.0));
    const double Start = Field.HeightAt(A.X, A.Y), End = Field.HeightAt(B.X, B.Y);
    double Previous = Start;
    for (int32 Sample = 1; Sample <= Samples; ++Sample)
    {
        const FVector2D At = A + (B - A) * (static_cast<double>(Sample) / Samples);
        if (!Field.IsOpen(FMath::RoundToInt(At.X), FMath::RoundToInt(At.Y)))
        {
            return Run;
        }
        const double Here = Field.HeightAt(At.X, At.Y);
        Run.Grade = FMath::Max(Run.Grade, FMath::Abs(Here - Previous) / (Length / Samples * Field.Cell));
        Run.Gap = FMath::Max(Run.Gap, FMath::Abs(Here - FMath::Lerp(Start, End, static_cast<double>(Sample) / Samples)));
        if (Run.Grade > MaxGrade)
        {
            Run.bClear = true;
            return Run;
        }
        Previous = Here;
    }
    Run.bClear = true;
    return Run;
}

void Grow(FRoadField& Field, int32 Cells, const TArray<FIntPoint>& Keep)
{
    if (Cells <= 0)
    {
        return;
    }
    TArray<uint8> Grown = Field.Open;
    for (int32 Y = 0; Y < Field.Height; ++Y)
    {
        for (int32 X = 0; X < Field.Width; ++X)
        {
            if (Field.Open[Y * Field.Width + X])
            {
                continue;
            }
            for (int32 DY = -Cells; DY <= Cells; ++DY)
            {
                for (int32 DX = -Cells; DX <= Cells; ++DX)
                {
                    const int32 NX = X + DX, NY = Y + DY;
                    if (DX * DX + DY * DY > Cells * Cells || NX < 0 || NY < 0 || NX >= Field.Width || NY >= Field.Height)
                    {
                        continue;
                    }
                    const bool bKept = Keep.ContainsByPredicate([&](const FIntPoint& Point) { return FMath::Square(NX - Point.X) + FMath::Square(NY - Point.Y) <= Cells * Cells; });
                    if (!bKept || !Field.Open[NY * Field.Width + NX])
                    {
                        Grown[NY * Field.Width + NX] = 0;
                    }
                }
            }
        }
    }
    Field.Open = MoveTemp(Grown);
}

TArray<FIntPoint> Search(const FRoadField& Field, FIntPoint From, FIntPoint To, double MaxGrade, double RadiusCm, double Deadline,
                         int32& OutExpanded, bool& bOutStopped)
{
    OutExpanded = 0;
    bOutStopped = false;
    const int32 W = Field.Width, Cells = Field.Width * Field.Height;
    TArray<double> Cost, Grade;
    Cost.Init(TNumericLimits<double>::Max(), Cells * RoadSlots);
    Grade.Init(-1.0, Cells * 16);  // each edge measured once, when first reached: -1 not yet
    TArray<int32> Back;
    Back.Init(INDEX_NONE, Cells * RoadSlots);
    TArray<uint8> Done;
    Done.SetNumZeroed(Cells * RoadSlots);
    double Angle[16];
    for (int32 K = 0; K < 16; ++K)
    {
        Angle[K] = FMath::Atan2(static_cast<double>(RoadStepY[K]), static_cast<double>(RoadStepX[K]));
    }
    const auto Guess = [&](int32 X, int32 Y) { return FMath::Sqrt(static_cast<double>(FMath::Square(X - To.X) + FMath::Square(Y - To.Y))) * Field.Cell; };
    using FEntry = TPair<double, int32>;
    const auto Sooner = [](const FEntry& A, const FEntry& B) { return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value); };
    TArray<FEntry> Frontier;
    const int32 Start = (From.Y * W + From.X) * RoadSlots + RoadStart, Goal = To.Y * W + To.X;
    Cost[Start] = 0.0;
    Frontier.HeapPush(FEntry(Guess(From.X, From.Y), Start), Sooner);
    int32 Found = INDEX_NONE;
    while (Frontier.Num() > 0)
    {
        if (Frontier.Num() > RoadMaxFrontier || ((OutExpanded & 4095) == 0 && FPlatformTime::Seconds() > Deadline))
        {
            bOutStopped = true;
            break;
        }
        FEntry Top;
        Frontier.HeapPop(Top, Sooner);
        const int32 State = Top.Value, Cell = State / RoadSlots, In = State % RoadSlots;
        if (Done[State])
        {
            continue;
        }
        Done[State] = 1;
        ++OutExpanded;
        if (Cell == Goal)
        {
            Found = State;
            break;
        }
        const int32 X = Cell % W, Y = Cell / W;
        for (int32 Out = 0; Out < 16; ++Out)
        {
            const int32 NX = X + RoadStepX[Out], NY = Y + RoadStepY[Out];
            if (!Field.IsOpen(NX, NY))
            {
                continue;
            }
            double Turn = 0.0;
            if (In != RoadStart)
            {
                Turn = FMath::Abs(Angle[In] - Angle[Out]);
                Turn = Turn > RoadPi ? 2.0 * RoadPi - Turn : Turn;
                if (CornerRadius(RoadStepLength(In) * Field.Cell, RoadStepLength(Out) * Field.Cell, Turn) < RadiusCm)
                {
                    continue;
                }
            }
            double& Steep = Grade[Cell * 16 + Out];
            if (Steep < 0.0)
            {
                const FRun Run = Segment(Field, FVector2D(X, Y), FVector2D(NX, NY), MaxGrade);
                Steep = Run.bClear ? Run.Grade : TNumericLimits<double>::Max();
            }
            if (Steep > MaxGrade)
            {
                continue;
            }
            // Length, more of it the nearer the grade limit, and a little for each notch of turn.
            const int32 Next = (NY * W + NX) * RoadSlots + Out;
            const double Reach = Cost[State] + RoadStepLength(Out) * Field.Cell * (1.0 + FMath::Square(Steep / MaxGrade)) + 0.25 * Field.Cell * Turn / RoadNotch;
            if (Reach < Cost[Next])
            {
                Cost[Next] = Reach;
                Back[Next] = State;
                Frontier.HeapPush(FEntry(Reach + Guess(NX, NY), Next), Sooner);
            }
        }
    }
    TArray<FIntPoint> Path;
    for (int32 State = Found; State != INDEX_NONE; State = Back[State])
    {
        Path.Insert(FIntPoint((State / RoadSlots) % W, (State / RoadSlots) / W), 0);
    }
    return Path;
}
} // namespace McpRoadPlan
