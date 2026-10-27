// Copyright (c) 2024 EngineWire Contributors
//
// Wave function collapse over a grid of cells (McpModuleKit.h): a domain of variants per cell as bit sets, propagation
// until nothing changes, the undecided cell of least Shannon entropy observed by weight, a trail to undo a choice that
// empties a cell, bounded backtracking, then fresh attempts on the next seeds. Gumin's WaveFunctionCollapse (MIT) is the
// reference for the collapse, DeBroglie (MIT) for the backtracking.

#include "Domains/Environment/Modules/McpModuleWave.h"

#include "HAL/PlatformTime.h"

namespace McpModuleKit
{
namespace
{
struct FModuleDecision
{
    int32 Mark = 0;
    int32 Cell = 0;
    int32 Pick = 0;
};
} // namespace

FModuleLayout Solve(const FModuleProblem& In)
{
    FModuleLayout Result;
    FModuleWave Wave(In);
    for (int32 Attempt = 0; Attempt < In.Attempts; ++Attempt)
    {
        FRandomStream Random(In.Seed + Attempt);
        Wave.Reset();
        // A lone cell no piece fits has no neighbour to tell propagation so.
        if (!Wave.Propagate() || Wave.Count.Contains(0))
        {
            Result.Why = TEXT("no piece fits some cells against the outside and its neighbours");
            return Result;
        }
        Wave.Trail.Reset();  // nothing undoes past the first choice
        TArray<FModuleDecision> Decisions;
        int32 Backtracks = 0;
        bool bStuck = false;
        for (int32 Cell = Wave.LeastEntropy(Random); Cell != INDEX_NONE && !bStuck; Cell = Wave.LeastEntropy(Random))
        {
            if (FPlatformTime::Seconds() > In.Deadline)
            {
                Result.Backtracks += Backtracks;
                Result.Why = TEXT("it ran out of time");
                return Result;
            }
            const int32 Pick = Wave.Choose(Cell, Random);
            Decisions.Add({Wave.Trail.Num(), Cell, Pick});
            Wave.Only(Cell, Pick);
            bool bOk = Wave.Propagate();
            // Undo the latest choice and rule it out, then propagate; past the budget, the attempt is given up.
            while (!bOk && !bStuck)
            {
                bStuck = ++Backtracks > In.MaxBacktracks || Decisions.Num() == 0;
                if (!bStuck)
                {
                    const FModuleDecision Last = Decisions.Last();
                    Decisions.SetNum(Decisions.Num() - 1);
                    Wave.UndoTo(Last.Mark);
                    const int32 Index = Last.Cell * Wave.Words + (Last.Pick >> 6);
                    Wave.Set(Index, Wave.Domain[Index] & ~(uint64(1) << (Last.Pick & 63)));
                    Wave.Push(Last.Cell);
                    bOk = Wave.Count[Last.Cell] > 0 && Wave.Propagate();
                }
            }
        }
        Result.Backtracks += Backtracks;
        if (!bStuck)
        {
            Result.bSolved = true;
            Result.Attempt = Attempt;
            Result.Placed.Init(INDEX_NONE, Wave.Cells);
            for (int32 Cell = 0; Cell < Wave.Cells; ++Cell)
            {
                for (int32 Word = 0; Word < Wave.Words; ++Word)
                {
                    const uint64 Bits = Wave.Domain[Cell * Wave.Words + Word];
                    Result.Placed[Cell] = Bits ? Word * 64 + LowestBit(Bits) : Result.Placed[Cell];
                }
            }
            return Result;
        }
    }
    Result.Why = FString::Printf(TEXT("no layout within %d backtracks on each of %d seeds"), In.MaxBacktracks, In.Attempts);
    return Result;
}

int32 OpenFaces(const FModuleProblem& In, const TArray<int32>& Placed)
{
    int32 Open = 0;
    for (int32 Z = 0; Z < In.Size.Z; ++Z)
    {
        for (int32 Y = 0; Y < In.Size.Y; ++Y)
        {
            for (int32 X = 0; X < In.Size.X; ++X)
            {
                const FModuleVariant& Here = In.Variants[Placed[(Z * In.Size.Y + Y) * In.Size.X + X]];
                for (int32 Side = 0; Side < 6; ++Side)
                {
                    const FIntVector N = FIntVector(X, Y, Z) + ModuleStep(Side);
                    const bool bOut = N.X < 0 || N.Y < 0 || N.Z < 0 || N.X >= In.Size.X || N.Y >= In.Size.Y || N.Z >= In.Size.Z;
                    const bool bFits = bOut ? In.OpenSide[Side] || Here.Faces[Side].IsAir()
                                            : Matches(Here.Faces[Side], In.Variants[Placed[(N.Z * In.Size.Y + N.Y) * In.Size.X + N.X]].Faces[Side ^ 1]);
                    Open += bFits ? 0 : 1;
                }
            }
        }
    }
    return Open;
}

TArray<FIntPoint> UnmatchedFaces(const FModuleProblem& In)
{
    TArray<FIntPoint> Out;
    for (int32 A = 0; A < In.Variants.Num(); ++A)
    {
        for (int32 Side = 0; Side < 6; ++Side)
        {
            const FModuleFace& Face = In.Variants[A].Faces[Side];
            const bool bOutsideTakes = In.OpenSide[Side] || Face.IsAir();
            // A grid one cell thick on this axis puts every face on it against the outside: a partner cannot help.
            const int32 Along = Side < 2 ? In.Size.X : Side < 4 ? In.Size.Y : In.Size.Z;
            const bool bPartner = Along > 1 && In.Variants.ContainsByPredicate([&](const FModuleVariant& B) { return Matches(Face, B.Faces[Side ^ 1]); });
            if (!bOutsideTakes && !bPartner)
            {
                Out.Emplace(A, Side);
            }
        }
    }
    return Out;
}
} // namespace McpModuleKit
