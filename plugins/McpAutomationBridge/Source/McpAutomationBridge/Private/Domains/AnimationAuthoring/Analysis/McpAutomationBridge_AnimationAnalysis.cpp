#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#include "AnimPose.h"

// analyze_animation: an Animation Sequence's motion read from its own frames, with no level or actor: how far and how
// fast the root travels, when each foot is planted and how fast a planted foot still moves, how closely the last frame
// meets the first, and the sharpest rotation spikes. get_animation_info gave length, notifies and curves only.
namespace McpAnimationAuthoring {
namespace {
constexpr int32 McpDefaultAnalysisSamples = 300;
constexpr int32 McpMaxAnalysisSamples = 2000;
constexpr int32 McpMaxContactsReported = 32;
constexpr int32 McpPopsReported = 3;

double McpRound(double Value, double Scale = 10.0)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

TSharedPtr<FJsonValue> McpVectorJson(const FVector& Value)
{
    return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(McpRound(Value.X)),
        MakeShared<FJsonValueNumber>(McpRound(Value.Y)), MakeShared<FJsonValueNumber>(McpRound(Value.Z))});
}

// The feet to check: the bones named, else every bone whose name holds "foot" but not "ik" (foot_l, not ik_foot_l).
TArray<FName> McpAnalysisFeet(const TSharedPtr<FJsonObject>& Params, const TArray<FName>& Bones)
{
    TArray<FName> Feet;
    const TArray<TSharedPtr<FJsonValue>>* Given = nullptr;
    if (Params->TryGetArrayField(TEXT("feet"), Given) && Given)
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

// One foot over the sampled frames: planted where it is within ContactHeight of its lowest point.
TSharedPtr<FJsonObject> McpDescribeFoot(const FName Foot, const TArray<FVector>& Path, const TArray<double>& Times, double ContactHeight)
{
    double Lowest = TNumericLimits<double>::Max();
    for (const FVector& Point : Path)
    {
        Lowest = FMath::Min(Lowest, Point.Z);
    }
    TArray<TSharedPtr<FJsonValue>> Contacts;
    int32 Planted = 0;
    int32 Moves = 0;
    double PlantedSpeed = 0.0;
    for (int32 Index = 0, Start = INDEX_NONE; Index <= Path.Num(); ++Index)
    {
        const bool bDown = Index < Path.Num() && Path[Index].Z <= Lowest + ContactHeight;
        Planted += bDown ? 1 : 0;
        if (bDown && Start != INDEX_NONE && Index > Start)
        {
            PlantedSpeed += FVector::Dist2D(Path[Index], Path[Index - 1]) / FMath::Max(Times[Index] - Times[Index - 1], 1e-6);
            ++Moves;
        }
        if (bDown && Start == INDEX_NONE)
        {
            Start = Index;
        }
        if (!bDown && Start != INDEX_NONE)
        {
            if (Contacts.Num() < McpMaxContactsReported)
            {
                Contacts.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                    MakeShared<FJsonValueNumber>(McpRound(Times[Start], 100.0)), MakeShared<FJsonValueNumber>(McpRound(Times[Index - 1], 100.0))}));
            }
            Start = INDEX_NONE;
        }
    }
    const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("bone"), Foot.ToString());
    Entry->SetNumberField(TEXT("lowest"), McpRound(Lowest));
    Entry->SetArrayField(TEXT("contacts"), Contacts);
    Entry->SetNumberField(TEXT("plantedRatio"), McpRound(double(Planted) / FMath::Max(Path.Num(), 1), 100.0));
    Entry->SetNumberField(TEXT("plantedSpeed"), McpRound(Moves > 0 ? PlantedSpeed / Moves : 0.0));
    return Entry;
}
} // namespace

TSharedPtr<FJsonObject> HandleAnalyzeAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath")));
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    UAnimSequence* Sequence = Cast<UAnimSequence>(Asset);
    if (!Sequence)
    {
        ANIM_ERROR_RESPONSE(Asset ? FString::Printf(TEXT("%s is a %s; analyze_animation reads an Animation Sequence (a montage or blend space plays sequences: analyze those)."), *Asset->GetName(), *Asset->GetClass()->GetName())
                                  : FString::Printf(TEXT("Could not load %s."), *AssetPath),
                            Asset ? TEXT("NOT_A_SEQUENCE") : TEXT("ASSET_NOT_FOUND"));
    }
    const int32 Keys = Sequence->GetNumberOfSampledKeys();
    const double Length = Sequence->GetPlayLength();
    if (!Sequence->GetSkeleton() || Keys < 2 || Length <= 0.0)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("%s has no skeleton or fewer than two frames to compare."), *Sequence->GetName()), TEXT("EMPTY_ANIMATION"));
    }
    const int32 Count = FMath::Min(Keys, FMath::Clamp(static_cast<int32>(GetJsonNumberField(Params, TEXT("maxSamples"), McpDefaultAnalysisSamples)), 2, McpMaxAnalysisSamples));
    const double ContactHeight = GetJsonNumberField(Params, TEXT("contactHeight"), 5.0);
    const FName Root = Sequence->GetSkeleton()->GetReferenceSkeleton().GetBoneName(0);
    FAnimPoseEvaluationOptions Options;
    Options.bExtractRootMotion = false; // the root bone keeps its motion, so the root path and planted feet read it
    FAnimPose Pose;
    TArray<FName> Bones;
    TArray<FName> Feet;
    TArray<double> Times;
    TArray<FTransform> RootPath;
    TArray<TArray<FVector>> FeetPaths;
    TArray<FTransform> FirstLocal;
    TArray<FTransform> PreviousLocal;
    TArray<double> PeakSpeed;
    TArray<double> PeakTime;
    for (int32 Sample = 0; Sample < Count; ++Sample)
    {
        // Spread evenly, with the first and the last frame always in.
        const int32 Frame = FMath::RoundToInt(double(Sample) * (Keys - 1) / (Count - 1));
        UAnimPoseExtensions::GetAnimPoseAtFrame(Sequence, Frame, Options, Pose);
        if (Sample == 0)
        {
            UAnimPoseExtensions::GetBoneNames(Pose, Bones);
            Feet = McpAnalysisFeet(Params, Bones);
            for (const FName& Foot : Feet)
            {
                if (!Bones.Contains(Foot))
                {
                    ANIM_ERROR_RESPONSE(FString::Printf(TEXT("bone '%s' is not in skeleton %s; list_bones lists its bones."), *Foot.ToString(), *Sequence->GetSkeleton()->GetName()), TEXT("BONE_NOT_FOUND"));
                }
            }
            FeetPaths.SetNum(Feet.Num());
            PeakSpeed.Init(0.0, Bones.Num());
            PeakTime.Init(0.0, Bones.Num());
        }
        const double Time = Frame * Length / (Keys - 1);
        RootPath.Add(UAnimPoseExtensions::GetBonePose(Pose, Root, EAnimPoseSpaces::World));
        for (int32 Index = 0; Index < Feet.Num(); ++Index)
        {
            FeetPaths[Index].Add(UAnimPoseExtensions::GetBonePose(Pose, Feet[Index], EAnimPoseSpaces::World).GetLocation());
        }
        TArray<FTransform> Local;
        Local.Reserve(Bones.Num());
        for (int32 Index = 0; Index < Bones.Num(); ++Index)
        {
            Local.Add(UAnimPoseExtensions::GetBonePose(Pose, Bones[Index], EAnimPoseSpaces::Local));
            const double Step = Sample > 0 ? Time - Times.Last() : 0.0;
            const double Speed = Step > 0.0 ? FMath::RadiansToDegrees(PreviousLocal[Index].GetRotation().AngularDistance(Local[Index].GetRotation())) / Step : 0.0;
            if (Speed > PeakSpeed[Index])
            {
                PeakSpeed[Index] = Speed;
                PeakTime[Index] = Time;
            }
        }
        Times.Add(Time);
        if (Sample == 0)
        {
            FirstLocal = Local;
        }
        PreviousLocal = MoveTemp(Local);
    }

    // The root's path: where it ends, how far it went and how fast, and how much it turned.
    double Distance = 0.0;
    double TopSpeed = 0.0;
    for (int32 Index = 1; Index < RootPath.Num(); ++Index)
    {
        const double Step = FVector::Dist(RootPath[Index].GetLocation(), RootPath[Index - 1].GetLocation());
        Distance += Step;
        TopSpeed = FMath::Max(TopSpeed, Step / FMath::Max(Times[Index] - Times[Index - 1], 1e-6));
    }
    const TSharedPtr<FJsonObject> RootMotion = MakeShared<FJsonObject>();
    RootMotion->SetBoolField(TEXT("enabled"), Sequence->bEnableRootMotion);
    RootMotion->SetField(TEXT("displacement"), McpVectorJson(RootPath.Last().GetLocation() - RootPath[0].GetLocation()));
    RootMotion->SetNumberField(TEXT("distance"), McpRound(Distance));
    RootMotion->SetNumberField(TEXT("averageSpeed"), McpRound(Distance / Length));
    RootMotion->SetNumberField(TEXT("peakSpeed"), McpRound(TopSpeed));
    RootMotion->SetNumberField(TEXT("turn"), McpRound((RootPath.Last().GetRotation() * RootPath[0].GetRotation().Inverse()).Rotator().Yaw));

    // The loop seam: every bone but the root, last frame against first, in its parent's space.
    double SeamPosition = 0.0;
    double SeamRotation = 0.0;
    FName SeamBone;
    for (int32 Index = 0; Index < Bones.Num(); ++Index)
    {
        if (Bones[Index] == Root)
        {
            continue;
        }
        SeamPosition = FMath::Max(SeamPosition, FVector::Dist(FirstLocal[Index].GetLocation(), PreviousLocal[Index].GetLocation()));
        const double Turn = FMath::RadiansToDegrees(FirstLocal[Index].GetRotation().AngularDistance(PreviousLocal[Index].GetRotation()));
        if (Turn > SeamRotation)
        {
            SeamRotation = Turn;
            SeamBone = Bones[Index];
        }
    }
    const TSharedPtr<FJsonObject> Loop = MakeShared<FJsonObject>();
    Loop->SetNumberField(TEXT("positionError"), McpRound(SeamPosition, 100.0));
    Loop->SetNumberField(TEXT("rotationError"), McpRound(SeamRotation));
    Loop->SetStringField(TEXT("worstBone"), SeamBone.ToString());

    TArray<TSharedPtr<FJsonValue>> FeetJson;
    for (int32 Index = 0; Index < Feet.Num(); ++Index)
    {
        FeetJson.Add(MakeShared<FJsonValueObject>(McpDescribeFoot(Feet[Index], FeetPaths[Index], Times, ContactHeight)));
    }
    // The sharpest rotation spikes, one per bone: a pop shows as a bone turning far faster than the rest.
    TArray<int32> Order;
    for (int32 Index = 0; Index < Bones.Num(); ++Index)
    {
        Order.Add(Index);
    }
    Order.Sort([&PeakSpeed](int32 A, int32 B) { return PeakSpeed[A] > PeakSpeed[B]; });
    TArray<TSharedPtr<FJsonValue>> Pops;
    for (int32 Rank = 0; Rank < FMath::Min(McpPopsReported, Order.Num()); ++Rank)
    {
        const TSharedPtr<FJsonObject> Pop = MakeShared<FJsonObject>();
        Pop->SetStringField(TEXT("bone"), Bones[Order[Rank]].ToString());
        Pop->SetNumberField(TEXT("time"), McpRound(PeakTime[Order[Rank]], 100.0));
        Pop->SetNumberField(TEXT("degreesPerSecond"), McpRound(PeakSpeed[Order[Rank]], 1.0));
        Pops.Add(MakeShared<FJsonValueObject>(Pop));
    }

    Response->SetStringField(TEXT("assetPath"), Sequence->GetPathName());
    Response->SetNumberField(TEXT("length"), McpRound(Length, 1000.0));
    Response->SetNumberField(TEXT("frames"), Keys);
    Response->SetNumberField(TEXT("frameRate"), McpRound((Keys - 1) / Length, 100.0));
    Response->SetNumberField(TEXT("samples"), Count);
    Response->SetObjectField(TEXT("rootMotion"), RootMotion);
    Response->SetObjectField(TEXT("loop"), Loop);
    Response->SetArrayField(TEXT("feet"), FeetJson);
    Response->SetArrayField(TEXT("pops"), Pops);
    McpHandlerUtils::MarkNoAssetsChanged(Response);
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("%s: %.2f s, %d frames; the root travels %.1f cm (%.1f cm/s); %d foot bone(s); loop seam %.2f cm, %.1f deg."),
        *Sequence->GetName(), Length, Keys, Distance, Distance / Length, Feet.Num(), SeamPosition, SeamRotation));
    return Response;
}

} // namespace McpAnimationAuthoring
