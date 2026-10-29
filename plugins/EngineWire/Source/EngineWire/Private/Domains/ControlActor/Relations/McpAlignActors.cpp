// control_actor align_actors: actors lined up on one axis, as a level designer squares up a row of crates, the tops of
// shelves or the fronts of buildings: each actor moves along axis only, so that the edge it is aligned by (the min, max
// or centre of its bounds, or its pivot) lands on the first actor's, or on value when one is given. Every name is found
// before anything moves, so a refusal changes nothing; the moves are one undo step (none when nothing moves), dryRun
// reports them without moving, and where an actor lands is checked like any move.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"

namespace McpRelations
{
namespace
{
/** Where an actor's edge lies along Axis: its bounds' min, max or centre, or its pivot. */
double EdgeOf(const AActor& Actor, int32 Axis, const FString& Edge)
{
    FVector Origin;
    FVector Extent;
    Actor.GetActorBounds(false, Origin, Extent);
    return Edge == TEXT("min") ? Origin[Axis] - Extent[Axis] : Edge == TEXT("max") ? Origin[Axis] + Extent[Axis]
        : Edge == TEXT("pivot") ? Actor.GetActorLocation()[Axis] : Origin[Axis];
}
} // namespace

bool HandleAlignActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was moved."), Code);
        return true;
    };
    const FString AxisText = GetJsonStringField(Payload, TEXT("axis")).ToLower();
    const FString Edge = GetJsonStringField(Payload, TEXT("edge"), TEXT("center")).ToLower();
    const bool bValue = Payload->HasField(TEXT("value"));
    const bool bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    const int32 Axis = AxisText == TEXT("x") ? 0 : AxisText == TEXT("y") ? 1 : AxisText == TEXT("z") ? 2 : INDEX_NONE;
    if (Axis == INDEX_NONE || !(Edge == TEXT("min") || Edge == TEXT("max") || Edge == TEXT("center") || Edge == TEXT("pivot")))
    {
        return Refuse(TEXT("axis is x, y or z; edge is min, max, center or pivot."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<AActor*> Actors;
    for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames")))
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), TEXT("ACTOR_NOT_FOUND"));
        }
        Actors.AddUnique(Actor);
    }
    if (Actors.Num() < (bValue ? 1 : 2))
    {
        return Refuse(TEXT("align_actors lines up two or more different actors with the first, or any with value."), TEXT("INVALID_ARGUMENT"));
    }
    const double Line = bValue ? GetJsonNumberField(Payload, TEXT("value"), 0.0) : EdgeOf(*Actors[0], Axis, Edge);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<UObject*> Moving;
    TArray<FVector> Destinations;
    for (AActor* Actor : Actors)
    {
        const FVector From = Actor->GetActorLocation();
        FVector To = From;
        To[Axis] += Line - EdgeOf(*Actor, Axis, Edge);
        if (!To.Equals(From, 0.001))
        {
            Moving.Add(Actor);
            Destinations.Add(To);
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Actor->GetActorLabel());
        Row->SetArrayField(TEXT("from"), McpHandlerUtils::VectorToJsonArray(From));
        Row->SetArrayField(TEXT("to"), McpHandlerUtils::VectorToJsonArray(To));
        Row->SetNumberField(TEXT("movedCm"), FMath::RoundToDouble(FVector::Dist(From, To) * 10.0) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // The undo step covers only actors that move: a row already in line would make a step that changes nothing.
    if (!bDryRun && Moving.Num() > 0)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Align Actors")), EMcpMutationDurability::EditorStateOnly, Moving);
        for (int32 Index = 0; Index < Moving.Num(); ++Index)
        {
            AActor* Actor = CastChecked<AActor>(Moving[Index]);
            Actor->Modify();
            Actor->SetActorLocation(Destinations[Index], false, nullptr, ETeleportType::TeleportPhysics);
            Actor->PostEditMove(true);
        }
        Transaction.DescribeInto(Result);
    }
    Result->SetArrayField(TEXT("actors"), Rows);
    Result->SetNumberField(TEXT("line"), FMath::RoundToDouble(Line * 1000.0) / 1000.0);
    Result->SetNumberField(TEXT("moved"), Moving.Num());
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %d of %d actor(s): their %s on %s = %.1f."),
        bDryRun ? TEXT("Would align") : TEXT("Aligned"), Moving.Num(), Actors.Num(), *Edge, *AxisText, Line), Result);
    return true;
}
} // namespace McpRelations
