// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// Erosion of a heightfield, the part of erode_landscape that never touches the engine: heights in cm on Width x Height
// cells of CellX x CellY cm, row by row; the border is base level, held and never changed. Priority-Flood fills the
// depressions (Barnes, Lehman, Mulla 2014), each cell drains to its steepest lower neighbour (a flat through an
// equal-height search to its outlet), implicit stream power cuts the valleys (Braun & Willett 2013), droplets add the
// gullies (Beyer 2015) and a talus pass slumps what is too steep. Deterministic: the same heights and seed give the
// same terrain.
namespace McpTerrainErosion
{
struct FGrid
{
    int32 Width = 0;
    int32 Height = 0;
    double CellX = 100.0;
    double CellY = 100.0;
    TArray<double> Heights;

    int32 Num() const { return Width * Height; }
    bool IsBorder(int32 Index) const
    {
        const int32 X = Index % Width, Y = Index / Width;
        return X == 0 || Y == 0 || X == Width - 1 || Y == Height - 1;
    }
    // The cm between a cell and its neighbour K (0-3 straight, 4-7 diagonal).
    double Step(int32 K) const { return K < 2 ? CellX : K < 4 ? CellY : FMath::Sqrt(CellX * CellX + CellY * CellY); }
};

// The 8 neighbours inside the grid, as Visit(Index, K).
template <typename FVisit>
void ForEachNeighbour(const FGrid& Grid, int32 Index, FVisit&& Visit)
{
    static constexpr int32 OffsetX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static constexpr int32 OffsetY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int32 X = Index % Grid.Width, Y = Index / Grid.Width;
    for (int32 K = 0; K < 8; ++K)
    {
        const int32 NX = X + OffsetX[K], NY = Y + OffsetY[K];
        if (NX >= 0 && NY >= 0 && NX < Grid.Width && NY < Grid.Height)
        {
            Visit(NY * Grid.Width + NX, K);
        }
    }
}

struct FFill
{
    int32 Cells = 0;        // cells raised
    int32 Depressions = 0;  // separate basins they filled
    double MaxRise = 0.0;   // cm
};

// Where every cell drains: its receiver (INDEX_NONE for the border, and for a pit, none on a filled surface) and the
// cm to it, all cells in an order that puts each receiver before its donors, each cell's donors (Donors[DonorStart[i]]
// up to DonorStart[i + 1]) and the area draining through it in cm2.
struct FFlow
{
    TArray<int32> Receiver;
    TArray<double> Distance;
    TArray<int32> Order;
    TArray<int32> DonorStart;
    TArray<int32> Donors;
    TArray<double> Area;
    int32 Pits = 0;
};

struct FRiver
{
    double CatchmentM2 = 0.0;
    double LengthCm = 0.0;
    TArray<FVector> Points;  // x and y in cells, z the height in cm: from its source to its mouth, simplified
};

/** Raise every cell to where it spills, so every cell has a path to the border that never climbs; lakes stay flat. */
FFill Fill(FGrid& Grid);
FFlow Flow(const FGrid& Grid);
/** One implicit step: an erosion front moves WaveCm up the largest river, sqrt(A / A_max) as far up the others. */
void StreamPower(FGrid& Grid, const FFlow& Flow, double WaveCm);
/** Droplets from Seed until Count have run or FPlatformTime::Seconds() passes Deadline; returns how many ran. */
int32 Droplets(FGrid& Grid, int32 Count, int32 Seed, double Deadline);
void Thermal(FGrid& Grid, double TalusDegrees, int32 Passes);
/** Cells with no path to the border that never climbs: 0 when every cell drains. */
int32 Undrained(const FGrid& Grid);
/** Rivers whose catchment reaches AreaCm2: each from its mouth up the largest donor, a tributary ending at its junction. */
TArray<FRiver> Rivers(const FGrid& Grid, const FFlow& Flow, double AreaCm2, int32 Max);
} // namespace McpTerrainErosion

namespace McpLandscapeErosion
{
// build_environment erode_landscape (McpLandscapeErode.cpp).
bool HandleErodeLandscape(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpLandscapeErosion
