#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// create_spline_mesh_component {actorName}: adds a USplineMeshComponent to a level actor
// (the blueprintPath route lives in McpSplineMeshBP.cpp).
// The schema advertises actorName but only the Blueprint route existed.
bool HandleCreateSplineMeshComponentOnActor(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ActorName,
    const FString& ComponentName,
    const FString& MeshPath,
    const FString& ForwardAxis)
{
    AActor* Actor = ResolveSplineActor(Self, RequestId, Socket, ActorName);
    if (!Actor)
    {
        return true;
    }
    UStaticMesh* Mesh = nullptr;
    if (!MeshPath.IsEmpty())
    {
        const FString SafeMeshPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("meshPath"), MeshPath);
        if (SafeMeshPath.IsEmpty())
        {
            return true;
        }
        Mesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
        if (!Mesh)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Mesh not found: %s"), *MeshPath), TEXT("MESH_NOT_FOUND"));
            return true;
        }
    }
    if (FindObject<UObject>(Actor, *ComponentName))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Component '%s' already exists on %s"), *ComponentName, *ActorName), TEXT("ALREADY_EXISTS"));
        return true;
    }
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Spline Mesh Component")),
                                            EMcpMutationDurability::EditorStateOnly, {Actor});
    USplineMeshComponent* MeshComp = NewObject<USplineMeshComponent>(Actor, FName(*ComponentName), RF_Transactional);
    if (!MeshComp)
    {
        Transaction.Cancel();
        Self->SendAutomationError(Socket, RequestId,
            TEXT("Failed to create spline mesh component"), TEXT("CREATE_FAILED"));
        return true;
    }
    if (Mesh)
    {
        MeshComp->SetStaticMesh(Mesh);
    }
    ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
    ParseSplineMeshAxis(ForwardAxis, Axis); // checked by the caller
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
    Result->SetStringField(TEXT("actorName"), McpActorRef(Actor));
    Result->SetStringField(TEXT("componentName"), MeshComp->GetName());
    Result->SetStringField(TEXT("componentPath"), MeshComp->GetPathName());
    Result->SetStringField(TEXT("meshPath"), Mesh ? Mesh->GetPathName() : FString());
    Result->SetStringField(TEXT("forwardAxis"), SplineMeshAxisName(MeshComp->GetForwardAxis()));
    McpHandlerUtils::AddVerification(Result, Actor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline mesh component '%s' added to %s"), *MeshComp->GetName(), *Actor->GetActorLabel()), Result);
    return true;
}
