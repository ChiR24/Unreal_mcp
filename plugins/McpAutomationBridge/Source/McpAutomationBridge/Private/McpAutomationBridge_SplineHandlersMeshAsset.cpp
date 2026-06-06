#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

static USplineMeshComponent* FindSplineMeshComponent(AActor* Actor, const FString& ComponentName)
{
    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);

    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                return Comp;
            }
        }
    }
    else if (MeshComponents.Num() > 0)
    {
        return MeshComponents[0];
    }

    return nullptr;
}

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
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));

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
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"));
    FString ForwardAxis = GetJsonStringFieldSpline(Payload, TEXT("forwardAxis"), TEXT("X"));

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

    ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
    if (ForwardAxis == TEXT("Y")) Axis = ESplineMeshAxis::Y;
    else if (ForwardAxis == TEXT("Z")) Axis = ESplineMeshAxis::Z;

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
