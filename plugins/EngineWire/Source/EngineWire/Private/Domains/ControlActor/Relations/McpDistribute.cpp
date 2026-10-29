// control_actor distribute_actors: actors spaced evenly along one axis, as a level designer lines up pillars, crates or
// lamps. In their order along the axis (axis default: the one their centres spread across most), the first and the
// last stay where they are and the ones between move so the centres are evenly spaced; with gapCm each one instead
// sits gapCm past the one before it, measured between their bounds, from the first. Only the coordinate along the axis
// changes. Every name is found before anything moves, so a refusal changes nothing; the moves are one undo step (none
// when nothing moves) and dryRun reports them without moving anything. Where an actor lands is checked like any move.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"

namespace McpRelations
{
namespace
{
double Round3(double Value)
{
    return FMath::RoundToDouble(Value * 1000.0) / 1000.0;
}

TArray<TSharedPtr<FJsonValue>> Triple(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(Round3(V.X)), MakeShared<FJsonValueNumber>(Round3(V.Y)), MakeShared<FJsonValueNumber>(Round3(V.Z))};
}
} // namespace

bool HandleDistributeActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was moved."), Code);
        return true;
    };
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    const FString AxisText = GetJsonStringField(Payload, TEXT("axis")).ToLower();
    const bool bGap = Payload->HasField(TEXT("gapCm"));
    const double Gap = GetJsonNumberField(Payload, TEXT("gapCm"), 0.0);
    const bool bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    if (!(AxisText.IsEmpty() || AxisText == TEXT("x") || AxisText == TEXT("y") || AxisText == TEXT("z")) || Gap < 0.0)
    {
        return Refuse(TEXT("axis is x, y or z; gapCm is 0 or more."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<AActor*> Actors;
    for (const FString& Name : Names)
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), TEXT("ACTOR_NOT_FOUND"));
        }
        Actors.AddUnique(Actor);
    }
    if (Actors.Num() < 3)
    {
        return Refuse(TEXT("distribute_actors spaces three or more different actors (two are always evenly spaced)."), TEXT("INVALID_ARGUMENT"));
    }
    // The axis the centres spread across most, unless one is named.
    FBox Spread(ForceInit);
    for (const AActor* Actor : Actors)
    {
        Spread += Actor->GetActorLocation();
    }
    const FVector Size = Spread.GetSize();
    const int32 Axis = AxisText == TEXT("x") ? 0 : AxisText == TEXT("y") ? 1 : AxisText == TEXT("z") ? 2
        : (Size.X >= Size.Y && Size.X >= Size.Z ? 0 : Size.Y >= Size.Z ? 1 : 2);
    Actors.StableSort([Axis](const AActor& A, const AActor& B) { return A.GetActorLocation()[Axis] < B.GetActorLocation()[Axis]; });
    TArray<double> Targets;
    const double First = Actors[0]->GetActorLocation()[Axis];
    const double Step = (Actors.Last()->GetActorLocation()[Axis] - First) / (Actors.Num() - 1);
    double Edge = 0.0;
    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        FVector Origin;
        FVector Extent;
        Actors[Index]->GetActorBounds(false, Origin, Extent);
        const double CentreToOrigin = Actors[Index]->GetActorLocation()[Axis] - Origin[Axis];
        double Target = First + Step * Index;
        if (bGap)
        {
            Target = Index == 0 ? First : Edge + Gap + Extent[Axis] + CentreToOrigin;
            Edge = Target - CentreToOrigin + Extent[Axis];
        }
        Targets.Add(Target);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<UObject*> Moving;
    TArray<FVector> Destinations;
    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        const FVector From = Actors[Index]->GetActorLocation();
        FVector To = From;
        To[Axis] = Targets[Index];
        if (!To.Equals(From, 0.001))
        {
            Moving.Add(Actors[Index]);
            Destinations.Add(To);
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Actors[Index]->GetActorLabel());
        Row->SetArrayField(TEXT("from"), Triple(From));
        Row->SetArrayField(TEXT("to"), Triple(To));
        Row->SetNumberField(TEXT("movedCm"), FMath::RoundToDouble(FVector::Dist(From, To) * 10.0) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // The undo step covers only actors that move: spacing already even would make a step that changes nothing.
    if (!bDryRun && Moving.Num() > 0)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Distribute Actors")), EMcpMutationDurability::EditorStateOnly, Moving);
        for (int32 Index = 0; Index < Moving.Num(); ++Index)
        {
            AActor* Actor = CastChecked<AActor>(Moving[Index]);
            Actor->Modify();
            Actor->SetActorLocation(Destinations[Index], false, nullptr, ETeleportType::TeleportPhysics);
            Actor->PostEditMove(true);
        }
        Transaction.DescribeInto(Result);
    }
    static const TCHAR* const AxisNames[] = {TEXT("x"), TEXT("y"), TEXT("z")};
    Result->SetArrayField(TEXT("actors"), Rows);
    Result->SetStringField(TEXT("axis"), AxisNames[Axis]);
    if (!bGap)
    {
        Result->SetNumberField(TEXT("stepCm"), Round3(Step));
    }
    Result->SetNumberField(TEXT("moved"), Moving.Num());
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %d of %d actor(s) along %s."),
        bDryRun ? TEXT("Would space") : TEXT("Spaced"), Moving.Num(), Actors.Num(), AxisNames[Axis]), Result);
    return true;
}
} // namespace McpRelations
