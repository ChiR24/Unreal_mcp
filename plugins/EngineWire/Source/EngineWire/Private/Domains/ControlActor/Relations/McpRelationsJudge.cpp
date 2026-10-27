// Copyright (c) 2024 EngineWire Contributors
//
// One relation between two named actors, for an assertion (McpContracts). on is judged as describe_relations judges
// it, among the actors around the pair (a lamp on a book on a table rests on the book, not the table), and a picture
// hanging on a wall is on it too; inside, nextTo, against, facing and near are measured on the pair alone: inside any
// container (not only the tightest), against as nextTo with its back to the other, facing within 20 degrees (not only
// the nearest), near within the distance asked (100 cm unless said), gap to gap.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/Debug/Explain/McpExplain.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "GameFramework/Actor.h"

namespace McpRelations
{
namespace
{
constexpr double FacingDegrees = 20.0;
constexpr double NearDefault = 100.0;
constexpr double JudgeReach = 500.0; // cm around the pair whose actors may be what the subject rests on
constexpr double SideTouch = 2.0;    // cm between sides that touch, as describe_relations' nextTo allows

// fix_placement for the one actor, for its sunk and floating kinds.
TSharedPtr<FJsonObject> PlacementFix(const AActor* Actor)
{
    const TArray<TSharedPtr<FJsonValue>> Names = {MakeShared<FJsonValueString>(McpActorRef(Actor))};
    const TArray<TSharedPtr<FJsonValue>> Kinds = {MakeShared<FJsonValueString>(TEXT("sunk")), MakeShared<FJsonValueString>(TEXT("floating"))};
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetArrayField(TEXT("actorNames"), Names);
    Params->SetArrayField(TEXT("kinds"), Kinds);
    return McpExplain::MakeNextCall(TEXT("control_actor"), TEXT("fix_placement"), Params);
}
} // namespace

McpBoxSat::FFootprint DrawnFootprint(const AActor& Actor)
{
    const FRotator Turn(0.0, Actor.GetActorRotation().Yaw, 0.0);
    const FVector Origin = Actor.GetActorLocation();
    const FBox Local = McpPlacement::DrawnBoundsIn(Actor, FTransform(Turn, Origin));
    const FBox Box = Local.IsValid ? Local : McpPlacement::DrawnBounds(Actor).ShiftBy(-Origin);
    const FVector Center = Local.IsValid ? Turn.RotateVector(Box.GetCenter()) : Box.GetCenter();
    McpBoxSat::FFootprint Out;
    Out.Center = FVector2D(Origin.X + Center.X, Origin.Y + Center.Y);
    Out.Half = FVector2D(Box.GetExtent());
    Out.Yaw = Local.IsValid ? FMath::DegreesToRadians(Turn.Yaw) : 0.0;
    return Out;
}

bool JudgePair(UWorld* World, AActor* A, const FString& Kind, AActor* B, double Within, FString& OutActual,
               TSharedPtr<FJsonObject>& OutNextCall)
{
    const FBox BoxA = McpPlacement::DrawnBounds(*A);
    const FBox BoxB = McpPlacement::DrawnBounds(*B);
    if (Kind == TEXT("inside"))
    {
        const bool bInside = McpBoxSat::Outside(DrawnFootprint(*A), DrawnFootprint(*B)) <= 1.0 && BoxA.Min.Z >= BoxB.Min.Z - 1.0 &&
                             BoxA.Max.Z <= BoxB.Max.Z + 1.0;
        OutActual = bInside ? TEXT("it is within the other's bounds") : TEXT("part of it is outside the other's bounds");
        return bInside;
    }
    if (Kind == TEXT("near"))
    {
        const double Limit = Within >= 0.0 ? Within : NearDefault;
        const double Gap = FMath::Sqrt(BoxA.ComputeSquaredDistanceToBox(BoxB));
        OutActual = FString::Printf(TEXT("%.0f cm apart (near is within %.0f)"), Gap, Limit);
        return Gap <= Limit;
    }
    if (Kind == TEXT("facing"))
    {
        // Its front looks at it when the line ahead of it meets it (a chair square to the long side of a table), or points
        // within 20 degrees of its centre (a lamp across a room).
        FVector Forward = A->GetActorForwardVector();
        Forward.Z = 0.0;
        double Off = 0.0;
        const bool bFaces = Forward.Normalize() && McpBoxSat::Faces(DrawnFootprint(*A), DrawnFootprint(*B), FMath::DegreesToRadians(FacingDegrees), Off);
        OutActual = bFaces ? FString(TEXT("its front points at it"))
            : Forward.IsZero() ? FString(TEXT("it points straight up or down"))
            : FString::Printf(TEXT("its front points %.0f degrees away from it"), FMath::RadiansToDegrees(Off));
        return bFaces;
    }
    if (Kind == TEXT("nextTo") || Kind == TEXT("against"))
    {
        const FContact Met = Contact(DrawnFootprint(*A), BoxA, DrawnFootprint(*B), BoxB);
        OutActual = Met.bTouching ? FString::Printf(TEXT("touching %s"), *Met.Side)
            : Met.Gap < -SideTouch ? FString(TEXT("seen from above they overlap: one stands on, over or in the other, not beside it"))
            : Met.Gap > SideTouch ? FString::Printf(TEXT("they do not touch side by side (%.0f cm apart)"), Met.Gap)
            : FString(TEXT("they meet only at an edge or a corner, or at different heights"));
        if (!Met.bTouching || Kind == TEXT("nextTo"))
        {
            return Met.bTouching;
        }
        // against: the side that touches is its back, so its front points away from the other.
        FVector Forward = A->GetActorForwardVector();
        Forward.Z = 0.0;
        const FVector Away(-Met.Toward.X, -Met.Toward.Y, 0.0);
        const double Off = Forward.Normalize() ? FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, Away))) : 180.0;
        OutActual += Off <= FacingDegrees ? FString(TEXT(", its back to it"))
                                          : FString::Printf(TEXT(", but its front points %.0f degrees off facing away from it"), Off);
        return Off <= FacingDegrees;
    }
    // on: what the subject rests on (or hangs on) among the actors around the pair.
    const FBox Pair = BoxA + BoxB;
    int32 InScope = 0;
    const TArray<FItem> Items = CollectItems(World, {A, B}, true, Pair.GetCenter(), Pair.GetExtent().Size() + JudgeReach, 400, InScope);
    const int32 Subject = Items.IndexOfByPredicate([A](const FItem& Item) { return Item.Actor == A; });
    if (Subject == INDEX_NONE)
    {
        OutActual = TEXT("it is hidden, draws nothing, or is a light or volume: it rests on nothing");
        return false;
    }
    // A rug or a floor tile counts as ground, which describe_relations never sets on anything: measured on the pair.
    if (Items[Subject].bGround)
    {
        const double Step = BoxA.Min.Z - BoxB.Max.Z;
        const double Apart = FlatGap(BoxA, BoxB);
        const bool bRests = FMath::Abs(Step) <= SupportTolerance && Apart <= 0.0;
        OutActual = bRests ? FString(TEXT("it lies on it"))
            : Apart > 0.0 ? FString::Printf(TEXT("it is beside it, not on it (%.0f cm apart seen from above)"), Apart)
            : FString::Printf(TEXT("its bottom is %.0f cm %s the other's top"), FMath::Abs(Step), Step < 0.0 ? TEXT("below") : TEXT("above"));
        return bRests;
    }
    TArray<FRelation> Relations;
    FindPairRelations(World, Items, {TEXT("on"), TEXT("mounted")}, Relations);
    const FRelation* Found = Relations.FindByPredicate([Subject](const FRelation& Relation) { return Relation.Subject == Subject; });
    const bool bHangs = Found && FCString::Strcmp(Found->Kind, TEXT("mounted")) == 0;
    if (Found && Items[Found->Object].Actor == B)
    {
        OutActual = !Found->Detail.IsEmpty() ? Found->Detail : FString(bHangs ? TEXT("it hangs on it") : TEXT("it rests on it"));
        return true;
    }
    if (Found)
    {
        OutActual = FString::Printf(TEXT("it %s '%s' instead"), bHangs ? TEXT("hangs on") : TEXT("rests on"), *Items[Found->Object].Name);
        return false;
    }
    OutActual = TEXT("it rests on nothing: it floats, or sinks into what is under it");
    OutNextCall = PlacementFix(A);
    return false;
}
} // namespace McpRelations
