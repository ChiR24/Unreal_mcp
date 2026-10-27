// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// manage_sequence frame_shot: a camera placed so one or two subjects sit where they should on screen. Screen points run
// from -1 (left, bottom) to 1 (right, top); the thirds are at +-1/3. The camera never rolls, and its rotation is solved
// in closed form for each place tried (Aim), so the search is over where it stands: around one subject (its direction,
// at the distance where the subject spans the share of the frame asked), or, for two, over the places that see them
// as far apart as their screen points are (Lino and Christie's toric space, 2015). Candidates spread around the
// subjects are scored, and the best few refined by Nelder-Mead.
namespace McpFrameShot
{
/** The frame as the lens sees it: tangents of half the horizontal and vertical field of view. */
struct FLens
{
    double TanHalfH = 0.5;
    double TanHalfV = 0.28;
};

struct FSubject
{
    FVector Center = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector; // half the size of its box
    FVector Forward = FVector::ForwardVector;
};

struct FProblem
{
    TArray<FSubject> Subjects;  // one or two
    TArray<FVector2D> Targets;  // each subject's screen point
    FLens Lens;
    double Fill = 0.55;         // one subject: the share of the frame its box spans
    TFunction<double(const FVector& Camera, int32 Subject)> Occluded; // share of the sight lines to it blocked (0-1)
    TFunction<bool(const FVector& Camera)> Inside;                    // a camera there would sit in something solid
};

struct FShot
{
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    TArray<FVector2D> Screen;   // where each subject's centre lands
    TArray<double> Occluded;
    bool bInside = false;
    double Fill = 0.0;          // the share of the frame the first subject's box spans
    double Cost = 0.0;
    int32 Evaluations = 0;
};

/** The roll-free rotation of a camera at Camera that puts Toward on screen at Target. */
FRotator Aim(const FVector& Camera, const FVector& Toward, const FVector2D& Target, const FLens& Lens);

/** Where Point lands on screen; false when it is behind the camera. */
bool Project(const FVector& Point, const FVector& Camera, const FRotator& Rotation, const FLens& Lens, FVector2D& OutScreen);

/** The best camera found for Problem (deterministic: the same problem gives the same shot). */
FShot Solve(const FProblem& Problem);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpFrameShot
