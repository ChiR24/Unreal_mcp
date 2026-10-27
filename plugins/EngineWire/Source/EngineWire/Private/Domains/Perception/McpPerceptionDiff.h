// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpEnvironmentHandlers
{
struct FMcpViewCapture;
}

// describe_view's saveAs and compareWith: the frames measured and compared (McpPerceptionDiffFrames) and the frames kept
// under a name (McpPerceptionDiff).
namespace McpPerception::Diff
{
/** A frame reduced to what the comparison reads. */
struct FViewMeasure
{
    FIntPoint Size = FIntPoint::ZeroValue;
    int32 BlocksX = 0; // whole 8 x 8 pixel blocks; the few pixels past the last one are left out
    int32 BlocksY = 0;
    TArray<uint8> Luma;         // per pixel
    TArray<FLinearColor> BlockColour; // per block: its mean colour as shown (sRGB, 0-1)
    TArray<uint16> Actor;       // per sample (every second pixel of every second row): 1 + its label's index in Names, 0 for none
    TArray<FString> Names;
};

/** A frame kept under a name: the renderer's own noise per block and the camera it was taken from. */
struct FSavedView
{
    FViewMeasure Frame;
    TArray<float> NoiseLoss;
    TArray<float> NoiseColour;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float Fov = 0.f;
    uint64 Order = 0;
};

FViewMeasure MeasureFrame(const McpEnvironmentHandlers::FMcpViewCapture& View);

/** Per block of two frames: the SSIM loss, (1 - SSIM) / 2, of its luma and how far its mean colour moved (the RMS of
 *  the channel differences as shown); all zero when the frames differ in size. */
void BlockChange(const FViewMeasure& A, const FViewMeasure& B, TArray<float>& OutLoss, TArray<float>& OutColour);

/** describe_view's change: the blocks changed beyond the saved frame's noise, joined into regions named by the actors
 *  drawn there before and after. */
TSharedPtr<FJsonObject> CompareFrames(const FString& Name, const FSavedView& Saved, const FViewMeasure& Now);
} // namespace McpPerception::Diff
