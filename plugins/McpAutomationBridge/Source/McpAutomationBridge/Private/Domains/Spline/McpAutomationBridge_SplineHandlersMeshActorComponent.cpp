#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// create_spline_mesh_component {actorName}: adds a USplineMeshComponent to a level actor
// (the blueprintPath route lives in McpAutomationBridge_SplineHandlersMeshBlueprint.cpp).
// Dogfood #212: the schema advertises actorName but only the Blueprint route existed.
bool HandleCreateSplineMeshComponentOnActor(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ActorName,
    const FString& ComponentName,
    const FString& MeshPath,
    const FString& ForwardAxis)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    AActor* Actor = World ? FindActorByName(World, ActorName) : nullptr;
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    UStaticMesh* Mesh = nullptr;
    if (!MeshPath.IsEmpty())
    {
        const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
        Mesh = SafeMeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
        if (!Mesh)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Mesh not found: %s"), *MeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
            return true;
        }
    }
    if (FindObject<UObject>(Actor, *ComponentName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Component '%s' already exists on %s"), *ComponentName, *ActorName), nullptr, TEXT("ALREADY_EXISTS"));
        return true;
    }
    Actor->Modify();
    USplineMeshComponent* MeshComp = NewObject<USplineMeshComponent>(Actor, FName(*ComponentName), RF_Transactional);
    if (!MeshComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create spline mesh component"), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }
    if (Mesh)
    {
        MeshComp->SetStaticMesh(Mesh);
    }
    const ESplineMeshAxis::Type Axis = ParseSplineMeshAxis(ForwardAxis);
    MeshComp->SetForwardAxis(Axis);
    // Follow the first spline segment when the actor already carries a spline.
    if (USplineComponent* Spline = FindSplineComponent(Actor))
    {
        if (Spline->GetNumberOfSplinePoints() >= 2)
        {
            MeshComp->SetStartAndEnd(
                Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
                Spline->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local),
                Spline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local),
                Spline->GetTangentAtSplinePoint(1, ESplineCoordinateSpace::Local));
        }
    }
    if (USceneComponent* Root = Actor->GetRootComponent())
    {
        MeshComp->SetupAttachment(Root);
    }
    MeshComp->RegisterComponent();
    Actor->AddInstanceComponent(MeshComp);
    Actor->PostEditChange();
    Actor->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Result->SetStringField(TEXT("componentName"), MeshComp->GetName());
    Result->SetStringField(TEXT("componentPath"), MeshComp->GetPathName());
    Result->SetStringField(TEXT("meshPath"), Mesh ? Mesh->GetPathName() : FString());
    Result->SetStringField(TEXT("forwardAxis"), ForwardAxis);
    McpHandlerUtils::AddVerification(Result, Actor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline mesh component '%s' added to %s"), *MeshComp->GetName(), *Actor->GetActorLabel()), Result);
    return true;
}
