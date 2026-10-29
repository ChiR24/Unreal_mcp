// control_actor snap_to_grid: actors moved onto a grid, as the editor's grid snapping places a dragged actor: each
// location axis asked for rounded to the nearest multiple of gridCm (default the editor's own position grid), and, with
// rotationDeg, each rotation axis to a multiple of that. Every name is found before anything moves, so a refusal changes
// nothing; the moves are one undo step (none when nothing moves), and dryRun reports them without moving anything. Where an actor lands is checked
// like any other move (sunk, floating, overlapping).

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
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
double SnapValue(double Value, double Step)
{
    return Step > 0.0 ? FMath::RoundToDouble(Value / Step) * Step : Value;
}

TArray<TSharedPtr<FJsonValue>> Triple(double X, double Y, double Z)
{
    // Rounded for the reply: a grid of 0.1 must not read back as 32.300000000000004.
    const auto Round = [](double V) { return FMath::RoundToDouble(V * 1000.0) / 1000.0; };
    return {MakeShared<FJsonValueNumber>(Round(X)), MakeShared<FJsonValueNumber>(Round(Y)), MakeShared<FJsonValueNumber>(Round(Z))};
}
} // namespace

bool HandleSnapToGrid(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was moved."), Code);
        return true;
    };
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    double Grid = GEditor ? static_cast<double>(GEditor->GetGridSize()) : 10.0;
    Payload->TryGetNumberField(TEXT("gridCm"), Grid);
    double RotationStep = 0.0;
    Payload->TryGetNumberField(TEXT("rotationDeg"), RotationStep);
    const FString Axes = GetJsonStringField(Payload, TEXT("axes"), TEXT("xyz")).ToLower();
    const bool bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    bool bAxesValid = !Axes.IsEmpty();
    for (const TCHAR Axis : Axes)
    {
        bAxesValid &= Axis == TEXT('x') || Axis == TEXT('y') || Axis == TEXT('z');
    }
    if (Names.Num() == 0 || Grid <= 0.0 || RotationStep < 0.0 || !bAxesValid)
    {
        return Refuse(TEXT("actorNames names the actors (1-500); gridCm and rotationDeg are above 0; axes is any of x, y and z (xy keeps "
                           "heights)."), TEXT("INVALID_ARGUMENT"));
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
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<UObject*> Moving;
    TArray<TPair<FVector, FRotator>> Targets;
    for (AActor* Actor : Actors)
    {
        const FVector From = Actor->GetActorLocation();
        const FRotator FromRotation = Actor->GetActorRotation();
        const FVector To(Axes.Contains(TEXT("x")) ? SnapValue(From.X, Grid) : From.X, Axes.Contains(TEXT("y")) ? SnapValue(From.Y, Grid) : From.Y,
            Axes.Contains(TEXT("z")) ? SnapValue(From.Z, Grid) : From.Z);
        const FRotator ToRotation(SnapValue(FromRotation.Pitch, RotationStep), SnapValue(FromRotation.Yaw, RotationStep),
            SnapValue(FromRotation.Roll, RotationStep));
        if (!To.Equals(From, 0.001) || !ToRotation.Equals(FromRotation, 0.001))
        {
            Moving.Add(Actor);
            Targets.Emplace(To, ToRotation);
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Actor->GetActorLabel());
        Row->SetArrayField(TEXT("from"), Triple(From.X, From.Y, From.Z));
        Row->SetArrayField(TEXT("to"), Triple(To.X, To.Y, To.Z));
        if (RotationStep > 0.0)
        {
            Row->SetArrayField(TEXT("rotation"), Triple(ToRotation.Pitch, ToRotation.Yaw, ToRotation.Roll));
        }
        Row->SetNumberField(TEXT("movedCm"), FMath::RoundToDouble(FVector::Dist(From, To) * 10.0) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // The undo step covers only actors that move: one already on the grid would make a step that changes nothing.
    if (!bDryRun && Moving.Num() > 0)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Snap To Grid")), EMcpMutationDurability::EditorStateOnly, Moving);
        for (int32 Index = 0; Index < Moving.Num(); ++Index)
        {
            AActor* Actor = CastChecked<AActor>(Moving[Index]);
            Actor->Modify();
            Actor->SetActorLocationAndRotation(Targets[Index].Key, Targets[Index].Value, false, nullptr, ETeleportType::TeleportPhysics);
            Actor->PostEditMove(true);
        }
        Transaction.DescribeInto(Result);
    }
    const int32 Moved = Moving.Num();
    Result->SetArrayField(TEXT("actors"), Rows);
    Result->SetNumberField(TEXT("gridCm"), Grid);
    Result->SetNumberField(TEXT("moved"), Moved);
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %d of %d actor(s) onto the %g cm grid%s."),
        bDryRun ? TEXT("Would snap") : TEXT("Snapped"), Moved, Actors.Num(), Grid, RotationStep > 0.0 ? *FString::Printf(TEXT(" and %g degree steps"),
        RotationStep) : TEXT("")), Result);
    return true;
}
} // namespace McpRelations
