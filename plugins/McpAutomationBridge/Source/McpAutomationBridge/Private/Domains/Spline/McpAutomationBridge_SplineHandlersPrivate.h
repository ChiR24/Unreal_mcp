#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

class AActor;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class USplineComponent;
class USplineMeshComponent;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpSplineHandlers, Log, All);

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"


USplineComponent* FindSplineComponent(AActor* Actor, const FString& ComponentName = TEXT(""));
// Each of these replies with the refusal itself and returns null / empty on failure.
AActor* ResolveSplineActor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName);
USplineComponent* ResolveSplineTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName, AActor*& OutActor);
USplineMeshComponent* ResolveSplineMeshTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName, const FString& ComponentName, AActor*& OutActor);
// Spawns an empty, labelled actor whose root is a registered instance spline; null if the spawn failed.
USplineComponent* SpawnSplineActor(UWorld* World, const FString& Name, const FVector& Location, const FRotator& Rotation);
FString RequireSplineProjectPath(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Field, const FString& Path);
USplineMeshComponent* FindSplineMeshComponent(AActor* Actor, const FString& ComponentName = TEXT(""));
ESplineMeshAxis::Type ParseSplineMeshAxis(const FString& ForwardAxis);
ESplinePointType::Type ParseSplinePointType(const FString& TypeStr);
// Sets the point's arriveTangent and leaveTangent (local space) from the payload: a lone
// arriveTangent sets both, a lone leaveTangent keeps the current arrive. False when neither is given.
inline bool ApplySplinePointTangents(USplineComponent* Spline, int32 Index, const TSharedPtr<FJsonObject>& Payload)
{
    const bool bArrive = Payload->HasField(TEXT("arriveTangent"));
    const bool bLeave = Payload->HasField(TEXT("leaveTangent"));
    if (!bArrive && !bLeave)
    {
        return false;
    }
    const FVector Arrive = bArrive ? ExtractVectorField(Payload, TEXT("arriveTangent"), FVector::ZeroVector)
                                   : Spline->GetArriveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local);
    const FVector Leave = bLeave ? ExtractVectorField(Payload, TEXT("leaveTangent"), FVector::ZeroVector)
                                 : (bArrive ? Arrive : Spline->GetLeaveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local));
    Spline->SetTangentsAtSplinePoint(Index, Arrive, Leave, ESplineCoordinateSpace::Local, true);
    return true;
}
// Appends each points[] entry {position (or location), arriveTangent, leaveTangent, rotation,
// scale} as a local-space point of PointType. Only `location` was read before, so points sent
// with the declared `position` all landed at the origin.
inline void AddSplinePointsFromJson(USplineComponent* Spline, const TArray<TSharedPtr<FJsonValue>>& Points,
                                    ESplinePointType::Type PointType = ESplinePointType::Curve)
{
    for (const TSharedPtr<FJsonValue>& Value : Points)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Point) || !Point)
        {
            continue;
        }
        const TCHAR* LocationKey = (*Point)->HasField(TEXT("position")) ? TEXT("position") : TEXT("location");
        Spline->AddSplinePoint(ExtractVectorField(*Point, LocationKey, FVector::ZeroVector), ESplineCoordinateSpace::Local, false);
        const int32 Index = Spline->GetNumberOfSplinePoints() - 1;
        Spline->SetSplinePointType(Index, PointType, false);
        if ((*Point)->HasField(TEXT("rotation")))
        {
            Spline->SetRotationAtSplinePoint(Index, ExtractRotatorField(*Point, TEXT("rotation"), FRotator::ZeroRotator), ESplineCoordinateSpace::Local, false);
        }
        if ((*Point)->HasField(TEXT("scale")))
        {
            Spline->SetScaleAtSplinePoint(Index, ExtractVectorField(*Point, TEXT("scale"), FVector::OneVector), false);
        }
        ApplySplinePointTangents(Spline, Index, *Point);
    }
}
FString SplinePointTypeToString(ESplinePointType::Type Type);

void SetSplineConfigValue(AActor* Target, const FString& Key, const FString& Value);
AActor* ResolveSplineConfigTarget(UWorld* World, const FString& ActorName);
FString GetSplineConfigTargetName(AActor* Target);
bool GetConfiguredSplineBool(AActor* Actor, UWorld* World, const FString& Key, bool DefaultValue);
double GetConfiguredSplineNumber(AActor* Actor, UWorld* World, const FString& Key, double DefaultValue);
FString BoolToSplineConfigString(bool bValue);

bool HandleCreateSplineActor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddSplinePoint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveSplinePoint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplinePointPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplinePointTangents(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplinePointRotation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplinePointScale(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplineType(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateSplineMeshComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSplineMeshComponentOnActor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName, const FString& ComponentName, const FString& MeshPath, const FString& ForwardAxis);
bool HandleSetSplineMeshAsset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureSplineMeshAxis(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetSplineMeshMaterial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleScatterMeshesAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureMeshSpacing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureMeshRandomization(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateRoadSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateRiverSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateFenceSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateWallSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateCableSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreatePipeSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleGetSplinesInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
