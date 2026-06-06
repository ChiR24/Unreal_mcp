#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

static bool HandleCreateTemplateSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& TemplateName,
    const FString& DefaultMeshPath)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"), TemplateName + TEXT("_Spline"));
    FVector Location = GetJsonVectorFieldSpline(Payload, TEXT("location"));
    double Width = GetJsonNumberFieldSpline(Payload, TEXT("width"), 400.0);
    FString MaterialPath = GetJsonStringFieldSpline(Payload, TEXT("materialPath"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
    if (!NewActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(*ActorName);

    USplineComponent* SplineComp = NewObject<USplineComponent>(NewActor, TEXT("SplineComponent"));
    if (!SplineComp)
    {
        NewActor->Destroy();
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create spline component"), nullptr, TEXT("COMPONENT_FAILED"));
        return true;
    }

    SplineComp->RegisterComponent();
    NewActor->AddInstanceComponent(SplineComp);
    NewActor->SetRootComponent(SplineComp);

    SplineComp->ClearSplinePoints(false);
    SplineComp->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(500, 0, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(1000, 200, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(1500, 200, 0), ESplineCoordinateSpace::Local, true);

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("templateType"), TemplateName);
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s spline '%s' created"), *TemplateName, *ActorName), Result);
    return true;
}

bool HandleCreateRoadSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Road"), TEXT(""));
}

bool HandleCreateRiverSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("River"), TEXT(""));
}

bool HandleCreateFenceSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Fence"), TEXT(""));
}

bool HandleCreateWallSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Wall"), TEXT(""));
}

bool HandleCreateCableSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Cable"), TEXT(""));
}

bool HandleCreatePipeSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Pipe"), TEXT(""));
}
#endif
