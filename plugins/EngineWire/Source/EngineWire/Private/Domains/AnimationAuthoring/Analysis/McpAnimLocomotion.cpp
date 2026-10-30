// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AnimationAuthoring/Analysis/McpAnimLocomotion.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"

namespace McpAnimLocomotion
{
namespace
{
constexpr int32 LocomotionSamples = 120;
constexpr double LocomotionRootTravel = 10.0; // cm/s: a root slower than this stays put

// The mean horizontal velocity of the feet between two planted samples in a row (within ContactHeight of the foot's
// lowest point), over every foot.
FVector2D LocomotionSlide(const TArray<TArray<FVector>>& Paths, const TArray<double>& Times, double ContactHeight)
{
    FVector2D Slide = FVector2D::ZeroVector;
    int32 Steps = 0;
    for (const TArray<FVector>& Path : Paths)
    {
        double Lowest = TNumericLimits<double>::Max();
        for (const FVector& Point : Path)
        {
            Lowest = FMath::Min(Lowest, Point.Z);
        }
        for (int32 Index = 1; Index < Path.Num(); ++Index)
        {
            const double Step = Times[Index] - Times[Index - 1];
            if (Step > 0.0 && Path[Index].Z <= Lowest + ContactHeight && Path[Index - 1].Z <= Lowest + ContactHeight)
            {
                Slide += FVector2D(Path[Index].X - Path[Index - 1].X, Path[Index].Y - Path[Index - 1].Y) / Step;
                ++Steps;
            }
        }
    }
    return Steps > 0 ? Slide / Steps : FVector2D::ZeroVector;
}
} // namespace

TArray<FName> FeetOf(const TSharedPtr<FJsonObject>& Params, const TArray<FName>& Bones)
{
    TArray<FName> Feet;
    const TArray<TSharedPtr<FJsonValue>>* Given = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(TEXT("feet"), Given) && Given)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Given)
        {
            Feet.Add(FName(*Value->AsString()));
        }
        return Feet;
    }
    for (const FName& Bone : Bones)
    {
        const FString Name = Bone.ToString();
        if (Name.Contains(TEXT("foot")) && !Name.Contains(TEXT("ik")))
        {
            Feet.Add(Bone);
        }
    }
    return Feet;
}

FFeetSample SampleFeet(UAnimSequence* Sequence, const TSharedPtr<FJsonObject>& Params)
{
    FFeetSample Sample;
    const int32 Keys = Sequence->GetNumberOfSampledKeys();
    const double Length = Sequence->GetPlayLength();
    if (!Sequence->GetSkeleton() || Keys < 2 || Length <= 0.0)
    {
        Sample.ErrorCode = TEXT("EMPTY_ANIMATION");
        Sample.Error = FString::Printf(TEXT("%s has no skeleton or fewer than two frames to read its motion from."), *Sequence->GetName());
        return Sample;
    }
    const int32 Count = FMath::Min(Keys, LocomotionSamples);
    const FName Root = Sequence->GetSkeleton()->GetReferenceSkeleton().GetBoneName(0);
    FAnimPoseEvaluationOptions Options;
    Options.bExtractRootMotion = false; // the root bone keeps its travel
    FAnimPose Pose;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        // Spread evenly, the first and the last frame always in.
        const int32 Frame = FMath::RoundToInt(double(Index) * (Keys - 1) / (Count - 1));
        UAnimPoseExtensions::GetAnimPoseAtFrame(Sequence, Frame, Options, Pose);
        if (Index == 0)
        {
            TArray<FName> Bones;
            UAnimPoseExtensions::GetBoneNames(Pose, Bones);
            Sample.Feet = FeetOf(Params, Bones);
            for (const FName& Foot : Sample.Feet)
            {
                if (!Bones.Contains(Foot))
                {
                    Sample.ErrorCode = TEXT("BONE_NOT_FOUND");
                    Sample.Error = FString::Printf(TEXT("bone '%s' is not in %s's skeleton; list_bones lists its bones."), *Foot.ToString(), *Sequence->GetName());
                    return Sample;
                }
            }
            Sample.Paths.SetNum(Sample.Feet.Num());
        }
        Sample.Times.Add(Frame * Length / (Keys - 1));
        Sample.RootLast = UAnimPoseExtensions::GetBonePose(Pose, Root, EAnimPoseSpaces::World).GetLocation();
        Sample.RootFirst = Index == 0 ? Sample.RootLast : Sample.RootFirst;
        for (int32 Foot = 0; Foot < Sample.Feet.Num(); ++Foot)
        {
            Sample.Paths[Foot].Add(UAnimPoseExtensions::GetBonePose(Pose, Sample.Feet[Foot], EAnimPoseSpaces::World).GetLocation());
        }
    }
    return Sample;
}

FMotion Measure(UAnimSequence* Sequence, const TSharedPtr<FJsonObject>& Params, double ContactHeight)
{
    FMotion Motion;
    const FFeetSample Sample = SampleFeet(Sequence, Params);
    if (!Sample.ErrorCode.IsEmpty())
    {
        Motion.ErrorCode = Sample.ErrorCode;
        Motion.Error = Sample.Error;
        return Motion;
    }
    // A root that travels is the clip's motion (its feet ride on it, planted or not); a root that stays is an in-place
    // clip, whose planted feet slide backward at the ground speed.
    const double Length = Sequence->GetPlayLength();
    const FVector2D RootVelocity((Sample.RootLast.X - Sample.RootFirst.X) / Length, (Sample.RootLast.Y - Sample.RootFirst.Y) / Length);
    Motion.bFromFeet = RootVelocity.Size() < LocomotionRootTravel;
    Motion.Velocity = Motion.bFromFeet ? -LocomotionSlide(Sample.Paths, Sample.Times, ContactHeight) : RootVelocity;
    return Motion;
}
} // namespace McpAnimLocomotion
