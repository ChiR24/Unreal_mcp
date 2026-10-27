// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Domains/Sequence/Planner/McpFrameShot.h"

class UCameraComponent;
class UWorld;
struct FCollisionQueryParams;

// frame_shot's camera side: the lens read from the camera, the world's sight lines, the undo step, the reply.
namespace McpFrameShot
{
/** The camera a shot moves: the actor named when it has a camera component, else one to spawn under that label. */
struct FFrameCamera
{
    AActor* Actor = nullptr;
    UCameraComponent* Component = nullptr;
    FString Label;
    bool bCine = false;
    double FocalLength = 0.0; // a cine camera: the focal length it will keep
    FLens Lens;
};

/** The share of nine sight lines from Camera to Box (its middle and a 3 x 3 grid across it as seen from there) that
 *  something blocks first; Params leaves out the subject, what hangs on it, and the camera. */
double HiddenShare(const UWorld& World, const FCollisionQueryParams& Params, const FVector& Camera, const FBox& Box);

/** The camera and the frame its lens gives; OutError when the actor is no camera or focalLength cannot apply. */
void ReadCamera(AActor* Actor, const FString& Label, double AskedFocalLength, double DefaultFocalLength, FFrameCamera& Out,
                FString& OutError);

/** Solve against the world: sight lines to each subject's box (past it and the camera), the camera kept out of solids. */
FShot SolveInWorld(const UWorld& World, const FProblem& Problem, const TArray<AActor*>& Subjects, const TArray<FBox>& Boxes,
                   AActor* Camera);

/** The camera moved or spawned, its lens and focus set, as one undo step; false with the undo gate's refusal. */
bool WriteShot(FFrameCamera& Camera, const FShot& Shot, double FocusDistance, const TSharedPtr<FJsonObject>& Result,
               FString& OutCode, FString& OutError);

TSharedPtr<FJsonObject> DescribeShot(const FProblem& Problem, const FShot& Shot, const FFrameCamera& Camera,
                                     const TArray<AActor*>& Subjects);

FString ShotMessage(const FProblem& Problem, const FShot& Shot, const FFrameCamera& Camera, const TArray<AActor*>& Subjects,
                    bool bSpawn, bool bDryRun);
} // namespace McpFrameShot
