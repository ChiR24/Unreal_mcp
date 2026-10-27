// Copyright (c) 2024 EngineWire Contributors
//
// describe_view's change since a saved frame. Each 8 x 8 block of pixels is compared by SSIM on its luma (Wang et al.
// 2004) and by how far its mean colour moved as shown, and counts as changed only beyond the renderer's own noise in
// that block (McpPerceptionDiff measures it when the frame is saved). Not in OKLab: its cube root made a 2 of 255
// shading change on a black wall read as a visible one. Changed blocks near each other join into regions, each named by
// the actors drawn there before and after.

#include "Domains/Perception/McpPerceptionDiff.h"

#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "GameFramework/Actor.h"
#include "Templates/Greater.h"

namespace McpPerception::Diff
{
namespace
{
constexpr int32 DiffBlock = 8;  // pixels per block side, the window SSIM is usually taken over
constexpr int32 DiffStride = 2; // actor samples: every second pixel of every second row
constexpr int32 RegionLimit = 8;
constexpr int32 DrawnLimit = 3;
constexpr float LossFloor = 0.02f; // SSIM loss a block may show with no noise measured
constexpr float ColourFloor = 0.02f; // RMS of the channels as shown: about 5 of 255
constexpr float NoiseFactor = 3.f; // a change stands this far above the block's own noise
constexpr double SsimC1 = 6.5025;  // (0.01 x 255)^2
constexpr double SsimC2 = 58.5225; // (0.03 x 255)^2

int32 SamplesAcross(const FIntPoint& Size)
{
    return (Size.X + DiffStride - 1) / DiffStride;
}

// The actors drawn over a set of blocks, by their share of its samples (up to three).
TArray<TSharedPtr<FJsonValue>> DrawnOver(const FViewMeasure& Frame, const TArray<int32>& Blocks)
{
    TMap<FString, int32> Counts;
    int32 Samples = 0;
    for (const int32 Block : Blocks)
    {
        for (int32 Y = (Block / Frame.BlocksX) * DiffBlock; Y < (Block / Frame.BlocksX + 1) * DiffBlock; Y += DiffStride)
        {
            for (int32 X = (Block % Frame.BlocksX) * DiffBlock; X < (Block % Frame.BlocksX + 1) * DiffBlock; X += DiffStride)
            {
                ++Samples;
                const uint16 Slot = Frame.Actor[(Y / DiffStride) * SamplesAcross(Frame.Size) + X / DiffStride];
                if (Slot)
                {
                    ++Counts.FindOrAdd(Frame.Names[Slot - 1]);
                }
            }
        }
    }
    Counts.ValueSort(TGreater<int32>());
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const TPair<FString, int32>& Entry : Counts)
    {
        if (Rows.Num() == DrawnLimit)
        {
            break;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Entry.Key);
        Row->SetNumberField(TEXT("share"), FMath::RoundToDouble(1000.0 * Entry.Value / FMath::Max(Samples, 1)) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Rows;
}

struct FRegion
{
    TArray<int32> Blocks;
    float Peak = 0.f;
    FIntRect Box = FIntRect(MAX_int32, MAX_int32, 0, 0);
};
} // namespace

FViewMeasure MeasureFrame(const McpEnvironmentHandlers::FMcpViewCapture& View)
{
    using namespace McpEnvironmentHandlers;
    FViewMeasure Out;
    Out.Size = View.Size;
    Out.BlocksX = View.Size.X / DiffBlock;
    Out.BlocksY = View.Size.Y / DiffBlock;
    Out.Luma.SetNumUninitialized(View.Colors.Num());
    for (int32 Index = 0; Index < View.Colors.Num(); ++Index)
    {
        const FColor& Pixel = View.Colors[Index];
        Out.Luma[Index] = uint8(FMath::Clamp(FMath::RoundToInt(0.2126f * Pixel.R + 0.7152f * Pixel.G + 0.0722f * Pixel.B), 0, 255));
    }
    for (int32 BlockY = 0; BlockY < Out.BlocksY; ++BlockY)
    {
        for (int32 BlockX = 0; BlockX < Out.BlocksX; ++BlockX)
        {
            FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
            for (int32 Y = BlockY * DiffBlock; Y < (BlockY + 1) * DiffBlock; ++Y)
            {
                for (int32 X = BlockX * DiffBlock; X < (BlockX + 1) * DiffBlock; ++X)
                {
                    const FColor& Pixel = View.Colors[Y * View.Size.X + X];
                    Sum += FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f, 1.f);
                }
            }
            Out.BlockColour.Add(Sum / float(DiffBlock * DiffBlock));
        }
    }
    TMap<uint32, FMcpViewProxy> Proxies;
    TMap<const AActor*, uint16> Slots;
    for (int32 Y = 0; Y < View.Size.Y; Y += DiffStride)
    {
        for (int32 X = 0; X < View.Size.X; X += DiffStride)
        {
            const FColor& Id = View.Ids[Y * View.Size.X + X];
            const FMcpViewProxy* Proxy = Proxies.Find(McpViewIdKey(Id));
            Proxy = Proxy ? Proxy : &Proxies.Add(McpViewIdKey(Id), McpViewProxyOf(Id));
            const uint16* Slot = Proxy->Kind == EMcpViewKind::Actor ? Slots.Find(Proxy->Actor) : nullptr;
            if (!Slot && Proxy->Kind == EMcpViewKind::Actor && Out.Names.Num() < MAX_uint16 - 1)
            {
                Out.Names.Add(Proxy->Actor->GetActorLabel());
                Slot = &Slots.Add(Proxy->Actor, uint16(Out.Names.Num()));
            }
            Out.Actor.Add(Slot ? *Slot : 0);
        }
    }
    return Out;
}

void BlockChange(const FViewMeasure& A, const FViewMeasure& B, TArray<float>& OutLoss, TArray<float>& OutColour)
{
    OutLoss.Init(0.f, A.BlockColour.Num());
    OutColour.Init(0.f, A.BlockColour.Num());
    if (A.Size != B.Size)
    {
        return;
    }
    constexpr double Count = DiffBlock * DiffBlock;
    for (int32 Block = 0; Block < A.BlockColour.Num(); ++Block)
    {
        double SumA = 0.0, SumB = 0.0, SumAA = 0.0, SumBB = 0.0, SumAB = 0.0;
        for (int32 Y = (Block / A.BlocksX) * DiffBlock; Y < (Block / A.BlocksX + 1) * DiffBlock; ++Y)
        {
            for (int32 X = (Block % A.BlocksX) * DiffBlock; X < (Block % A.BlocksX + 1) * DiffBlock; ++X)
            {
                const double ValueA = A.Luma[Y * A.Size.X + X], ValueB = B.Luma[Y * A.Size.X + X];
                SumA += ValueA;
                SumB += ValueB;
                SumAA += ValueA * ValueA;
                SumBB += ValueB * ValueB;
                SumAB += ValueA * ValueB;
            }
        }
        const double MeanA = SumA / Count, MeanB = SumB / Count;
        const double Ssim = ((2.0 * MeanA * MeanB + SsimC1) * (2.0 * (SumAB / Count - MeanA * MeanB) + SsimC2)) /
                            ((MeanA * MeanA + MeanB * MeanB + SsimC1) *
                             (SumAA / Count - MeanA * MeanA + SumBB / Count - MeanB * MeanB + SsimC2));
        OutLoss[Block] = float((1.0 - Ssim) * 0.5);
        const FLinearColor Shift = A.BlockColour[Block] - B.BlockColour[Block];
        OutColour[Block] = FMath::Sqrt((Shift.R * Shift.R + Shift.G * Shift.G + Shift.B * Shift.B) / 3.f);
    }
}

TSharedPtr<FJsonObject> CompareFrames(const FString& Name, const FSavedView& Saved, const FViewMeasure& Now)
{
    TArray<float> Loss, Colour;
    BlockChange(Saved.Frame, Now, Loss, Colour);
    const int32 Blocks = Loss.Num();
    TArray<float> Ratio; // above 1: changed beyond the block's noise
    for (int32 Block = 0; Block < Blocks; ++Block)
    {
        Ratio.Add(FMath::Max(Loss[Block] / (LossFloor + NoiseFactor * Saved.NoiseLoss[Block]),
                             Colour[Block] / (ColourFloor + NoiseFactor * Saved.NoiseColour[Block])));
    }
    // Changed blocks up to one block apart, corners included: a moved object's outline and the shading it changes
    // around it are one region, not several.
    TArray<bool> Seen;
    Seen.Init(false, Blocks);
    TArray<FRegion> Regions;
    for (int32 Seed = 0; Seed < Blocks; ++Seed)
    {
        if (Seen[Seed] || Ratio[Seed] <= 1.f)
        {
            continue;
        }
        FRegion Region;
        TArray<int32> Open = {Seed};
        Seen[Seed] = true;
        while (Open.Num() > 0)
        {
            const int32 Block = Open.Pop();
            const int32 X = Block % Now.BlocksX, Y = Block / Now.BlocksX;
            Region.Blocks.Add(Block);
            Region.Peak = FMath::Max(Region.Peak, Ratio[Block]);
            Region.Box = FIntRect(FMath::Min(Region.Box.Min.X, X), FMath::Min(Region.Box.Min.Y, Y),
                                  FMath::Max(Region.Box.Max.X, X + 1), FMath::Max(Region.Box.Max.Y, Y + 1));
            for (int32 Near = 0; Near < 25; ++Near)
            {
                const int32 NX = X + Near % 5 - 2, NY = Y + Near / 5 - 2;
                const int32 Next = NY * Now.BlocksX + NX;
                if (NX >= 0 && NY >= 0 && NX < Now.BlocksX && NY < Now.BlocksY && !Seen[Next] && Ratio[Next] > 1.f)
                {
                    Seen[Next] = true;
                    Open.Add(Next);
                }
            }
        }
        if (Region.Blocks.Num() >= 2 || Region.Peak >= 2.f) // one block just over the noise is a sparkle
        {
            Regions.Add(MoveTemp(Region));
        }
    }
    Regions.Sort([](const FRegion& A, const FRegion& B) { return A.Blocks.Num() > B.Blocks.Num(); });
    const auto Across = [&Now](int32 Block) { return FMath::RoundToDouble(1000.0 * Block * DiffBlock / Now.Size.X) / 10.0; };
    const auto Down = [&Now](int32 Block) { return FMath::RoundToDouble(1000.0 * Block * DiffBlock / Now.Size.Y) / 10.0; };
    int32 Changed = 0;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FRegion& Region : Regions)
    {
        Changed += Region.Blocks.Num();
        if (Rows.Num() == RegionLimit)
        {
            continue;
        }
        TArray<TSharedPtr<FJsonValue>> Box;
        for (const double Edge : {Across(Region.Box.Min.X), Down(Region.Box.Min.Y), Across(Region.Box.Max.X), Down(Region.Box.Max.Y)})
        {
            Box.Add(MakeShared<FJsonValueNumber>(Edge));
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetArrayField(TEXT("box"), Box);
        Row->SetNumberField(TEXT("share"), FMath::RoundToDouble(1000.0 * Region.Blocks.Num() / FMath::Max(Blocks, 1)) / 10.0);
        Row->SetArrayField(TEXT("before"), DrawnOver(Saved.Frame, Region.Blocks));
        Row->SetArrayField(TEXT("after"), DrawnOver(Now, Region.Blocks));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    double LumaBefore = 0.0, LumaNow = 0.0;
    for (int32 Index = 0; Index < Now.Luma.Num(); ++Index)
    {
        LumaBefore += Saved.Frame.Luma[Index];
        LumaNow += Now.Luma[Index];
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("comparedWith"), Name);
    Out->SetNumberField(TEXT("changed"), FMath::RoundToDouble(1000.0 * Changed / FMath::Max(Blocks, 1)) / 10.0);
    Out->SetNumberField(TEXT("regionCount"), Regions.Num());
    Out->SetArrayField(TEXT("regions"), Rows);
    Out->SetNumberField(TEXT("brightnessShift"), FMath::RoundToDouble(100.0 * (LumaNow - LumaBefore) / (255.0 * FMath::Max(Now.Luma.Num(), 1))) / 100.0);
    return Out;
}
} // namespace McpPerception::Diff
