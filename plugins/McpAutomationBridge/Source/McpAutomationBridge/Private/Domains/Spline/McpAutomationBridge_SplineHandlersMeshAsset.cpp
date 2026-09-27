#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleSetSplineMeshAsset(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));

    if (ActorName.IsEmpty() || MeshPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and meshPath are required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    const FString SafeMeshPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("meshPath"), MeshPath);
    if (SafeMeshPath.IsEmpty())
    {
        return true;
    }

    AActor* Actor = nullptr;
    USplineMeshComponent* TargetComp = ResolveSplineMeshTarget(Self, RequestId, Socket, ActorName, ComponentName, Actor);
    if (!TargetComp)
    {
        return true;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
    if (!Mesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Mesh not found: %s"), *SafeMeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    TargetComp->SetStaticMesh(Mesh);
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("meshPath"), SafeMeshPath);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline mesh asset set"), Result);
    return true;
}

bool HandleConfigureSplineMeshAxis(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    FString ForwardAxis = GetJsonStringField(Payload, TEXT("forwardAxis"), TEXT("X"));

    AActor* Actor = nullptr;
    USplineMeshComponent* TargetComp = ResolveSplineMeshTarget(Self, RequestId, Socket, ActorName, ComponentName, Actor);
    if (!TargetComp)
    {
        return true;
    }

    const ESplineMeshAxis::Type Axis = ParseSplineMeshAxis(ForwardAxis);

    TargetComp->SetForwardAxis(Axis);
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("forwardAxis"), ForwardAxis);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline mesh forward axis set to %s"), *ForwardAxis), Result);
    return true;
}
