#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h" // ESceneCaptureSource: unscoped in 5.0, so it cannot be forward-declared

class AActor;
class FEditorViewportClient;
class FJsonObject;
class FJsonValue;
class UWorld;

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

// describe_view's coveredBy for a named actor in view: the nearer actors drawn over at least 5% of its box (up to three,
// by share of the box), the same measure hiddenBy takes for one that is not seen.
TArray<TSharedPtr<FJsonValue>> McpCoveringActors(AActor *Actor, const FEditorViewportClient &Client, const FIntPoint &Size,
                                                 const TArray<FColor> &Ids, TMap<uint32, FMcpViewProxy> &Proxies);

// describe_view points: per normalized {x, y}, what is drawn at that pixel (drawn, actorName) and what the line from
// the camera through it hits first against Visibility collision (hit: location, normal, distance, actorName,
// componentName). A spot no actor is drawn on moves to the nearest pixel within Tolerance where one is (snappedTo).
// False with OutError for a malformed point or an orthographic viewport.
bool McpDescribeViewPoints(const FEditorViewportClient &Client, const FIntPoint &Size, const TArray<FColor> &Ids,
                           TMap<uint32, FMcpViewProxy> &Proxies, const TArray<TSharedPtr<FJsonValue>> &Points,
                           int32 Tolerance, TArray<TSharedPtr<FJsonValue>> &OutRows, FString &OutError);

// describe_view labels: the frame, at most 960 px wide, with each box drawn and numbered from 1 in list order; a
// base64 PNG.
FString McpLabelledViewPng(const TArray<FColor> &Colors, const FIntPoint &Size, const TArray<FIntRect> &Boxes);

// The level viewport as describe_view reads it: its client, size, pixels and hit proxy ids.
struct FMcpViewCapture {
    FEditorViewportClient *Client = nullptr;
    FIntPoint Size = FIntPoint::ZeroValue;
    TArray<FColor> Colors;
    TArray<FColor> Ids;
};

// Reads the level viewport, first moved to the payload's location and rotation when it names them (and drawn a few
// frames so occlusion catches up). False with OutError and OutCode (PIE_RUNNING, VIEWPORT_NOT_READY,
// VIEWPORT_NOT_AVAILABLE, CAPTURE_FAILED) when it cannot be read; What names the caller in the Play refusal. With
// bGameView the frame is drawn as the game shows it (no axes, icons or gizmos) and the viewport's own setting put back.
bool McpCaptureView(const TSharedPtr<FJsonObject> &Payload, const TCHAR *What, bool bGameView, FMcpViewCapture &Out,
                    FString &OutError, FString &OutCode);

// A scene capture of World from the view into a float target of Size: SCS_SceneDepth (cm), SCS_Normal (world normal),
// SCS_SceneColorHDR (the scene colour before tonemapping). An OrthoWidth above 0 makes it orthographic, that many cm
// across; ShowOnly draws only those actors.
TArray<FLinearColor> McpCapturePass(UWorld *World, const FVector &Location, const FRotator &Rotation, float Fov,
                                    const FIntPoint &Size, ESceneCaptureSource Source, float OrthoWidth = 0.f,
                                    const TArray<AActor *> *ShowOnly = nullptr);
} // namespace McpEnvironmentHandlers
