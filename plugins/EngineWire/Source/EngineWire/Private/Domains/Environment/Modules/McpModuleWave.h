// Copyright (c) 2024 EngineWire Contributors
#pragma once

// The wave of assemble_modules' solver (McpModuleSolve.cpp): a domain of variants per cell as bit sets, propagation
// until nothing changes, observation by weight, and a trail that undoes both.

#include "Domains/Environment/Modules/McpModuleKit.h"
#include "Math/RandomStream.h"

namespace McpModuleKit
{
/** The step to the neighbour beyond Side (+X, -X, +Y, -Y, +Z, -Z). */
inline FIntVector ModuleStep(int32 Side)
{
    return FIntVector(Side == 0 ? 1 : Side == 1 ? -1 : 0, Side == 2 ? 1 : Side == 3 ? -1 : 0, Side == 4 ? 1 : Side == 5 ? -1 : 0);
}

inline int32 LowestBit(uint64 Bits)
{
    return static_cast<int32>(FMath::CountTrailingZeros64(Bits));
}

// The wave and what it takes to keep it consistent: per-cell weight sums are kept as bits come and go, so finding the
// cell of least entropy costs one pass over the cells.
struct FModuleWave
{
    const FModuleProblem& In;
    int32 V = 0, Words = 0, Cells = 0;
    TArray<uint64> Allowed;  // [Side][Variant][Word]: the variants that may sit beyond that side
    TArray<uint64> Domain;   // [Cell][Word]
    TArray<double> SumW, SumWLogW;
    TArray<int32> Count;
    TArray<TPair<int32, uint64>> Trail;  // (word index, its value before)
    TArray<int32> Stack;
    TArray<uint8> Pending;
    int32 StackSize = 0;

    explicit FModuleWave(const FModuleProblem& InInput) : In(InInput)
    {
        V = In.Variants.Num();
        Words = (V + 63) / 64;
        Cells = In.Size.X * In.Size.Y * In.Size.Z;
        Allowed.SetNumZeroed(6 * V * Words);
        for (int32 Side = 0; Side < 6; ++Side)
        {
            for (int32 A = 0; A < V; ++A)
            {
                for (int32 B = 0; B < V; ++B)
                {
                    if (Matches(In.Variants[A].Faces[Side], In.Variants[B].Faces[Side ^ 1]))
                    {
                        Allowed[(Side * V + A) * Words + (B >> 6)] |= uint64(1) << (B & 63);
                    }
                }
            }
        }
        Stack.SetNumZeroed(Cells);
        Pending.SetNumZeroed(Cells);
    }

    FIntVector At(int32 Cell) const { return FIntVector(Cell % In.Size.X, (Cell / In.Size.X) % In.Size.Y, Cell / (In.Size.X * In.Size.Y)); }
    bool Inside(const FIntVector& P) const { return P.X >= 0 && P.Y >= 0 && P.Z >= 0 && P.X < In.Size.X && P.Y < In.Size.Y && P.Z < In.Size.Z; }
    int32 IndexOf(const FIntVector& P) const { return (P.Z * In.Size.Y + P.Y) * In.Size.X + P.X; }

    // Writes one word, keeping the cell's sums: bits gone are taken off them, bits back (an undo) put on.
    void Assign(int32 Index, uint64 Value)
    {
        const int32 Cell = Index / Words, Base = (Index % Words) * 64;
        const uint64 Old = Domain[Index];
        for (uint64 Bits = Old ^ Value; Bits; Bits &= Bits - 1)
        {
            const int32 Bit = LowestBit(Bits);
            const double W = In.Variants[Base + Bit].Weight, Sign = (Value >> Bit) & 1 ? 1.0 : -1.0;
            SumW[Cell] += Sign * W;
            SumWLogW[Cell] += Sign * W * FMath::Loge(W);
            Count[Cell] += Sign > 0.0 ? 1 : -1;
        }
        Domain[Index] = Value;
    }

    void Set(int32 Index, uint64 Value)
    {
        if (Domain[Index] != Value)
        {
            Trail.Emplace(Index, Domain[Index]);
            Assign(Index, Value);
        }
    }

    void UndoTo(int32 Mark)
    {
        while (Trail.Num() > Mark)
        {
            const TPair<int32, uint64> Step = Trail.Last();
            Trail.SetNum(Trail.Num() - 1);
            Assign(Step.Key, Step.Value);
        }
    }

    void Push(int32 Cell)
    {
        if (!Pending[Cell])
        {
            Pending[Cell] = 1;
            Stack[StackSize++] = Cell;
        }
    }

    // Every variant the outside allows in each cell (open sides take anything, the rest only an air face).
    void Reset()
    {
        // Init, not SetNumZeroed: a later attempt must not keep the last one's values.
        Domain.Init(0, Cells * Words);
        SumW.Init(0.0, Cells);
        SumWLogW.Init(0.0, Cells);
        Count.Init(0, Cells);
        Trail.Reset();
        for (int32 Cell = 0; Cell < Cells; ++Cell)
        {
            const FIntVector P = At(Cell);
            for (int32 A = 0; A < V; ++A)
            {
                bool bFits = true;
                for (int32 Side = 0; Side < 6 && bFits; ++Side)
                {
                    bFits = Inside(P + ModuleStep(Side)) || In.OpenSide[Side] || In.Variants[A].Faces[Side].IsAir();
                }
                if (bFits)
                {
                    Assign(Cell * Words + (A >> 6), Domain[Cell * Words + (A >> 6)] | (uint64(1) << (A & 63)));
                }
            }
            Push(Cell);
        }
    }

    // Each changed cell narrows its neighbours to what its remaining variants allow; false when a cell empties.
    bool Propagate()
    {
        TArray<uint64> Mask;
        Mask.SetNumUninitialized(Words);
        while (StackSize > 0)
        {
            const int32 Cell = Stack[--StackSize];
            Pending[Cell] = 0;
            for (int32 Side = 0; Side < 6; ++Side)
            {
                const FIntVector Next = At(Cell) + ModuleStep(Side);
                if (!Inside(Next))
                {
                    continue;
                }
                FMemory::Memzero(Mask.GetData(), Words * sizeof(uint64));
                for (int32 Word = 0; Word < Words; ++Word)
                {
                    for (uint64 Bits = Domain[Cell * Words + Word]; Bits; Bits &= Bits - 1)
                    {
                        const uint64* Row = &Allowed[(Side * V + Word * 64 + LowestBit(Bits)) * Words];
                        for (int32 Other = 0; Other < Words; ++Other)
                        {
                            Mask[Other] |= Row[Other];
                        }
                    }
                }
                const int32 N = IndexOf(Next);
                bool bChanged = false;
                for (int32 Word = 0; Word < Words; ++Word)
                {
                    const uint64 Value = Domain[N * Words + Word] & Mask[Word];
                    bChanged = bChanged || Value != Domain[N * Words + Word];
                    Set(N * Words + Word, Value);
                }
                if (Count[N] == 0)
                {
                    while (StackSize > 0)
                    {
                        Pending[Stack[--StackSize]] = 0;
                    }
                    return false;
                }
                if (bChanged)
                {
                    Push(N);
                }
            }
        }
        return true;
    }

    int32 LeastEntropy(FRandomStream& Random) const
    {
        int32 Best = INDEX_NONE;
        double BestH = TNumericLimits<double>::Max();
        for (int32 Cell = 0; Cell < Cells; ++Cell)
        {
            if (Count[Cell] > 1)
            {
                const double H = FMath::Loge(SumW[Cell]) - SumWLogW[Cell] / SumW[Cell] + Random.FRand() * 1e-6;
                Best = H < BestH ? Cell : Best;
                BestH = FMath::Min(H, BestH);
            }
        }
        return Best;
    }

    int32 Choose(int32 Cell, FRandomStream& Random) const
    {
        double Left = Random.FRand() * SumW[Cell];
        int32 Last = INDEX_NONE;
        for (int32 Word = 0; Word < Words; ++Word)
        {
            for (uint64 Bits = Domain[Cell * Words + Word]; Bits; Bits &= Bits - 1)
            {
                Last = Word * 64 + LowestBit(Bits);
                Left -= In.Variants[Last].Weight;
                if (Left <= 0.0)
                {
                    return Last;
                }
            }
        }
        return Last;
    }

    void Only(int32 Cell, int32 Pick)
    {
        for (int32 Word = 0; Word < Words; ++Word)
        {
            Set(Cell * Words + Word, Word == (Pick >> 6) ? uint64(1) << (Pick & 63) : 0);
        }
        Push(Cell);
    }
};
} // namespace McpModuleKit
