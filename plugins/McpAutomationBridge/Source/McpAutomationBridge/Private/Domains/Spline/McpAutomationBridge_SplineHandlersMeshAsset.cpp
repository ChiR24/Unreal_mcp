#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

static bool ResolveSplineMeshActorAndWorld(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ActorName,
    AActor*& OutActor,
    UWorld*& OutWorld)
{
    OutWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!OutWorld)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return false;
    }

    OutActor = FindActorByName(OutWorld, ActorName);
    if (!OutActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return false;
    }

    return true;
}

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

    FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (SafeMeshPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe meshPath: %s. Path must be relative to project (e.g., /Game/...)"), *MeshPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    AActor* Actor = nullptr;
    UWorld* World = nullptr;
    if (!ResolveSplineMeshActorAndWorld(Self, RequestId, Socket, ActorName, Actor, World))
    {
        return true;
    }

    USplineMeshComponent* TargetComp = FindSplineMeshComponent(Actor, ComponentName);
    if (!TargetComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
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
    World->MarkPackageDirty();

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

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    AActor* Actor = nullptr;
    UWorld* World = nullptr;
    if (!ResolveSplineMeshActorAndWorld(Self, RequestId, Socket, ActorName, Actor, World))
    {
        return true;
    }

    USplineMeshComponent* TargetComp = FindSplineMeshComponent(Actor, ComponentName);
    if (!TargetComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    const ESplineMeshAxis::Type Axis = ParseSplineMeshAxis(ForwardAxis);

    TargetComp->SetForwardAxis(Axis);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("forwardAxis"), ForwardAxis);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline mesh forward axis set to %s"), *ForwardAxis), Result);
    return true;
}
#endif
