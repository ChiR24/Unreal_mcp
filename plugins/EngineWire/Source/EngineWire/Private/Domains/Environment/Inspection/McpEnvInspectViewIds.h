#pragma once

#include "CoreMinimal.h"

class AActor;
class FEditorViewportClient;
class FJsonObject;

// The level viewport's hit proxy map holds, per pixel, the id of what was drawn there (what a click selects by).
// describe_view counts it per actor; capture_passes paints it as an id image.
namespace McpEnvironmentHandlers {
enum class EMcpViewKind : uint8 { Background, Other, Actor };

struct FMcpViewProxy {
    EMcpViewKind Kind = EMcpViewKind::Background;
    AActor *Actor = nullptr;
};

// An actor the game draws, Other for what only the editor draws (icons, gizmos, shapes hidden in game) or what no
// actor owns (BSP), Background where nothing was drawn (sky, void).
FMcpViewProxy McpViewProxyOf(const FColor &Id);

// The id a hit proxy colour stands for, to cache lookups by.
inline uint32 McpViewIdKey(const FColor &Id)
{
    return (uint32(Id.R) << 16) | (uint32(Id.G) << 8) | uint32(Id.B);
}

// Every second pixel of every second row: a quarter of the pixels, plenty for shares of the frame.
constexpr int32 McpViewStride = 2;

// Rounded to 1 / Scale (10 for tenths). Dividing by the scale, not multiplying by a step, keeps 61.9 from printing
// as 61.900000000000006.
inline double McpViewRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

// describe_view's row for an actor the caller named that drew no pixel: reason "outside the frame" when its bounds
// project outside it, else inFrame (percent of the box they project to inside the frame) and what is drawn over that
// box (hiddenBy: up to three actors by share of the box, leaving out actors centred farther than it and the ground
// under it). The level editor keeps the horizontal field of view fixed (MaintainXFOV), so ViewFOV is horizontal.
TSharedPtr<FJsonObject> McpDescribeUnseenActor(AActor *Actor, const FEditorViewportClient &Client, const FIntPoint &Size,
                                               const TArray<FColor> &Ids, TMap<uint32, FMcpViewProxy> &Proxies);
} // namespace McpEnvironmentHandlers
