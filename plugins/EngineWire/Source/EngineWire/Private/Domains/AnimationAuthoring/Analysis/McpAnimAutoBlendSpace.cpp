// Copyright (c) 2024 EngineWire Contributors
//
// auto_blend_space: a locomotion blend space laid out from its clips' own motion. Each clip's ground velocity
// (McpAnimLocomotion), turned into the character's frame, is placed by McpAnimBlendLayout on the axes asked; the axes
// are set to hold every sample and the samples replaced (or added to), in one undo step.

#include "Domains/AnimationAuthoring/McpAnimSupport.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Domains/AnimationAuthoring/Analysis/McpAnimBlendLayout.h"
#include "Domains/AnimationAuthoring/Analysis/McpAnimLocomotion.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAnimationAuthoring {
namespace {
double AutoBlendRound(double Value)
{
    return FMath::RoundToDouble(Value * 10.0) / 10.0;
}

// The character's forward in its clips' component space (the UE mannequin faces +Y there).
bool AutoBlendForward(const FString& Text, FVector2D& OutForward)
{
    static const TCHAR* const Names[] = {TEXT("+Y"), TEXT("-Y"), TEXT("+X"), TEXT("-X")};
    static const FVector2D Axes[] = {FVector2D(0.0, 1.0), FVector2D(0.0, -1.0), FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0)};
    for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Names)); ++Index)
    {
        if (Text.Equals(Names[Index], ESearchCase::IgnoreCase))
        {
            OutForward = Axes[Index];
            return true;
        }
    }
    return false;
}

TSharedPtr<FJsonObject> AutoBlendRow(const FString& Path, const FVector2D& Velocity, const McpAnimBlendLayout::FClipPlace& Place, bool bFromFeet)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("animationPath"), Path);
    Row->SetNumberField(TEXT("speed"), AutoBlendRound(Place.Speed));
    Row->SetNumberField(TEXT("direction"), AutoBlendRound(Place.Direction));
    Row->SetNumberField(TEXT("forwardSpeed"), AutoBlendRound(Velocity.X));
    Row->SetNumberField(TEXT("rightSpeed"), AutoBlendRound(Velocity.Y));
    Row->SetStringField(TEXT("source"), bFromFeet ? TEXT("feet") : TEXT("root"));
    Row->SetBoolField(TEXT("idle"), Place.bIdle);
    TArray<TSharedPtr<FJsonValue>> Positions;
    for (const FVector& Position : Place.Positions)
    {
        Positions.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Position.X), MakeShared<FJsonValueNumber>(Position.Y)}));
    }
    Row->SetArrayField(TEXT("positions"), Positions);
    if (Place.Refused > 0)
    {
        Row->SetNumberField(TEXT("refused"), Place.Refused);
    }
    return Row;
}
} // namespace

TSharedPtr<FJsonObject> HandleAutoBlendSpace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    using namespace McpAnimBlendLayout;
    const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
    FProperty* AxesProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters"));
    FBlendParameter* BlendAxes = BlendSpace && AxesProp ? AxesProp->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace) : nullptr;
    if (!BlendAxes)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
    }
    if (BlendSpace->IsA<UAimOffsetBlendSpace>() || BlendSpace->IsA<UAimOffsetBlendSpace1D>())
    {
        ANIM_ERROR_RESPONSE(TEXT("An Aim Offset blends aim poses by angle, not clips by how they move: add its samples with add_aim_offset_sample."), TEXT("INVALID_ARGUMENT"));
    }
    const bool b1D = BlendSpace->IsA<UBlendSpace1D>();
    TArray<EQuantity> Quantities;
    for (const FString& Text : McpHandlerUtils::GetStringArrayField(Params, TEXT("axes")))
    {
        EQuantity Quantity = EQuantity::Speed;
        if (!ParseQuantity(Text, Quantity))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Unknown axis '%s': use speed, direction, forwardSpeed or rightSpeed."), *Text), TEXT("INVALID_ARGUMENT"));
        }
        Quantities.Add(Quantity);
    }
    if (Quantities.Num() == 0)
    {
        Quantities = b1D ? TArray<EQuantity>{EQuantity::Speed} : TArray<EQuantity>{EQuantity::Direction, EQuantity::Speed};
    }
    FVector2D Forward(0.0, 1.0);
    if (Quantities.Num() != (b1D ? 1 : 2) || (Quantities.Num() == 2 && Quantities[0] == Quantities[1]) ||
        !AutoBlendForward(GetJsonStringField(Params, TEXT("forwardAxis"), TEXT("+Y")), Forward))
    {
        ANIM_ERROR_RESPONSE(TEXT("A Blend Space 1D takes one axis and a Blend Space two different ones; forwardAxis is +Y, -Y, +X or -X."), TEXT("INVALID_ARGUMENT"));
    }
    const FVector2D Right(-Forward.Y, Forward.X); // forward turned +90 degrees in yaw
    const double ContactHeight = GetJsonNumberField(Params, TEXT("contactHeight"), 5.0);
    const bool bKeep = GetJsonBoolField(Params, TEXT("keepSamples"), false);
    const bool bDryRun = GetJsonBoolField(Params, TEXT("dryRun"), false);

    // Every clip loaded, checked and measured before anything changes. Replacing, they are checked against each other,
    // not against the samples about to go.
    TArray<UAnimSequence*> Clips;
    TArray<FVector2D> Velocities;
    TArray<bool> FromFeet;
    const TArray<FString> Paths = McpHandlerUtils::GetStringArrayField(Params, TEXT("animationPaths"));
    for (const FString& Path : Paths)
    {
        UAnimSequence* Clip = LoadAnimSequenceFromPath(Path);
        if (!Clip)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s. Nothing was changed."), *Path), TEXT("ANIMATION_NOT_FOUND"));
        }
        const bool bFits = BlendSpace->IsAnimationCompatible(Clip) && BlendSpace->IsAnimationCompatibleWithSkeleton(Clip) &&
                           (bKeep ? BlendSpace->ValidateAnimationSequence(Clip) : Clips.Num() == 0 || Clips[0]->GetAdditiveAnimType() == Clip->GetAdditiveAnimType());
        if (!bFits)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("%s does not fit %s: another skeleton, or an additive type the blend space or the other clips do not share. Nothing was changed."), *Clip->GetName(), *BlendSpace->GetName()), TEXT("ANIMATION_INCOMPATIBLE"));
        }
        const McpAnimLocomotion::FMotion Motion = McpAnimLocomotion::Measure(Clip, Params, ContactHeight);
        if (!Motion.Error.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(Motion.Error + TEXT(" Nothing was changed."), Motion.ErrorCode);
        }
        Clips.Add(Clip);
        Velocities.Add(FVector2D(FVector2D::DotProduct(Motion.Velocity, Forward), FVector2D::DotProduct(Motion.Velocity, Right)));
        FromFeet.Add(Motion.bFromFeet);
    }
    if (Clips.Num() == 0)
    {
        ANIM_ERROR_RESPONSE(TEXT("animationPaths names the clips to lay out (1-32)."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<FBlendLayoutAxis> Axes;
    TArray<FClipPlace> Places;
    Layout(Velocities, Quantities, Axes, Places);
    // Kept samples stay inside the axes, and a position one of them holds is refused.
    for (const FBlendSample& Kept : bKeep ? BlendSpace->GetBlendSamples() : TArray<FBlendSample>())
    {
        for (int32 Axis = 0; Axis < Axes.Num(); ++Axis)
        {
            Cover(Axes[Axis], Kept.SampleValue[Axis]);
        }
        for (FClipPlace& Place : Places)
        {
            Place.Refused += Place.Positions.RemoveAll([&Kept](const FVector& Position) { return Position.Equals(Kept.SampleValue, 1e-3); });
        }
    }

    int32 Placed = 0;
    int32 Refused = 0;
    for (const FClipPlace& Place : Places)
    {
        Placed += Place.Positions.Num();
        Refused += Place.Refused;
    }
    if (!bDryRun)
    {
        const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpAutoBlendSpace", "Layout", "Lay Out Blend Space"), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{BlendSpace});
        BlendSpace->Modify();
        for (int32 Axis = 0; Axis < Axes.Num(); ++Axis)
        {
            BlendAxes[Axis].DisplayName = QuantityName(Axes[Axis].Quantity);
            BlendAxes[Axis].Min = static_cast<float>(Axes[Axis].Min);
            BlendAxes[Axis].Max = static_cast<float>(Axes[Axis].Max);
            BlendAxes[Axis].bWrapInput = Axes[Axis].bWrap;
        }
        for (int32 Index = bKeep ? -1 : BlendSpace->GetNumberOfBlendSamples() - 1; Index >= 0; --Index)
        {
            BlendSpace->DeleteSample(Index); // from the last, so the indices left do not shift
        }
        for (int32 Clip = 0; Clip < Clips.Num(); ++Clip)
        {
            for (const FVector& Position : Places[Clip].Positions)
            {
                if (BlendSpace->AddSample(Clips[Clip], Position) == INDEX_NONE)
                {
                    --Placed;
                    ++Refused;
                }
            }
        }
        BlendSpace->ValidateSampleData();
        BlendSpace->PostEditChange();
        BlendSpace->ResampleData();
        BlendSpace->MarkPackageDirty();
        Transaction.DescribeInto(Response);
    }

    TArray<TSharedPtr<FJsonValue>> AxesJson;
    for (const FBlendLayoutAxis& Axis : Axes)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("axisName"), QuantityName(Axis.Quantity));
        Row->SetNumberField(TEXT("minValue"), Axis.Min);
        Row->SetNumberField(TEXT("maxValue"), Axis.Max);
        Row->SetBoolField(TEXT("wrapInput"), Axis.bWrap);
        AxesJson.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Clip = 0; Clip < Clips.Num(); ++Clip)
    {
        Rows.Add(MakeShared<FJsonValueObject>(AutoBlendRow(Clips[Clip]->GetPathName(), Velocities[Clip], Places[Clip], FromFeet[Clip])));
    }
    Response->SetStringField(TEXT("assetPath"), BlendSpace->GetPathName());
    Response->SetArrayField(TEXT("axes"), AxesJson);
    Response->SetArrayField(TEXT("clips"), Rows);
    Response->SetNumberField(TEXT("samples"), bDryRun ? Placed : BlendSpace->GetNumberOfBlendSamples());
    if (bDryRun)
    {
        McpHandlerUtils::MarkNoAssetsChanged(Response);
    }
    else
    {
        SaveAnimAssetAndReport(BlendSpace, GetJsonBoolField(Params, TEXT("save"), true), Response);
    }
    const FString Refusals = Refused > 0 ? FString::Printf(TEXT(" %d position(s) refused: another clip or a kept sample holds them."), Refused) : FString();
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("%s %d clip(s) as %d sample(s) on %s.%s"), bDryRun ? TEXT("Would lay out") : TEXT("Laid out"), Clips.Num(), Placed,
                                          *FString::JoinBy(Axes, TEXT(" and "), [](const FBlendLayoutAxis& Axis) { return FString::Printf(TEXT("%s (%g to %g)"), QuantityName(Axis.Quantity), Axis.Min, Axis.Max); }),
                                          *Refusals));
    return Response;
}

} // namespace McpAnimationAuthoring
