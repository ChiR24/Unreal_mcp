#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

static bool HandleCreateTemplateSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& TemplateName,
    const FString& DefaultMeshPath)
{
    // `name` is the spelling create_road_spline and its sibling templates publish; `actorName`
    // is the legacy one. Reading only `actorName` meant a caller following the schema always
    // got the default label instead of the name they asked for.
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        ActorName = GetJsonStringFieldSpline(Payload, TEXT("name"), TemplateName + TEXT("_Spline"));
    }
    FVector Location = GetJsonVectorFieldSpline(Payload, TEXT("location"));
    double Width = GetJsonNumberFieldSpline(Payload, TEXT("width"), 400.0);
    FString MaterialPath = GetJsonStringFieldSpline(Payload, TEXT("materialPath"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));

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

    // Caller-supplied route. The declared `points` parameter used to be ignored outright and
    // every template emitted the same hardcoded zigzag below, so the capability could only
    // ever produce a demo shape regardless of what was asked for.
    SplineComp->ClearSplinePoints(false);
    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("points"), PointsArray) && PointsArray && PointsArray->Num() >= 2)
    {
        for (const TSharedPtr<FJsonValue>& PointValue : *PointsArray)
        {
            const TSharedPtr<FJsonObject>* PointObj = nullptr;
            if (!PointValue.IsValid() || !PointValue->TryGetObject(PointObj) || !PointObj)
            {
                continue;
            }
            SplineComp->AddSplinePoint(GetJsonVectorFieldSpline(*PointObj, TEXT("position")),
                ESplineCoordinateSpace::Local, false);
        }
    }
    else
    {
        SplineComp->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local, false);
        SplineComp->AddSplinePoint(FVector(500, 0, 0), ESplineCoordinateSpace::Local, false);
        SplineComp->AddSplinePoint(FVector(1000, 200, 0), ESplineCoordinateSpace::Local, false);
        SplineComp->AddSplinePoint(FVector(1500, 200, 0), ESplineCoordinateSpace::Local, false);
    }

    bool bClosedLoop = false;
    Payload->TryGetBoolField(TEXT("closedLoop"), bClosedLoop);
    SplineComp->SetClosedLoop(bClosedLoop, false);
    SplineComp->UpdateSpline();

    // Deform a mesh along the route. `meshPath`, `width` and `materialPath` were all read and
    // then discarded, so the actor carried a bare spline with no renderable geometry and no
    // material slot — set_material on it failed with MATERIAL_SLOT_NOT_FOUND.
    int32 MeshSegments = 0;
    const FString ResolvedMeshPath = MeshPath.IsEmpty() ? DefaultMeshPath : MeshPath;
    if (!ResolvedMeshPath.IsEmpty())
    {
        if (UStaticMesh* SegmentMesh = LoadObject<UStaticMesh>(nullptr, *ResolvedMeshPath))
        {
            UMaterialInterface* SegmentMaterial = MaterialPath.IsEmpty()
                ? nullptr : LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            const int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
            const int32 SegmentCount = bClosedLoop ? NumPoints : NumPoints - 1;
            // The mesh cross-section spans Y/Z for a forward axis of X; a 100-unit unit cube
            // therefore scales by width/100 across and stays thin vertically for a road ribbon.
            const FVector2D SegmentScale(static_cast<float>(Width) / 100.0f, 0.08f);
            for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
            {
                USplineMeshComponent* SegmentComp = NewObject<USplineMeshComponent>(NewActor);
                SegmentComp->SetMobility(EComponentMobility::Movable);
                SegmentComp->AttachToComponent(SplineComp, FAttachmentTransformRules::KeepRelativeTransform);
                SegmentComp->RegisterComponent();
                NewActor->AddInstanceComponent(SegmentComp);
                SegmentComp->SetStaticMesh(SegmentMesh);
                if (SegmentMaterial)
                {
                    SegmentComp->SetMaterial(0, SegmentMaterial);
                }
                SegmentComp->SetForwardAxis(ESplineMeshAxis::X, false);

                // A road or wall you cannot drive into or stand on is not a road
                // or a wall. A freshly constructed spline mesh arrives with the
                // NoCollision profile here, so without this the whole ribbon is
                // scenery: vehicles and characters fall straight through it and
                // the failure only shows up at play time, far from this call.
                // Complex-as-simple is what makes it work: simple primitives do
                // NOT follow the spline deformation, so only the per-poly body
                // matches the shape actually drawn.
                SegmentComp->bUseDefaultCollision = false;
                SegmentComp->bAlwaysCreatePhysicsState = true;
                SegmentComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
                SegmentComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

                const int32 NextIndex = (SegmentIndex + 1) % NumPoints;
                SegmentComp->SetStartAndEnd(
                    SplineComp->GetLocationAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local),
                    SplineComp->GetTangentAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local),
                    SplineComp->GetLocationAtSplinePoint(NextIndex, ESplineCoordinateSpace::Local),
                    SplineComp->GetTangentAtSplinePoint(NextIndex, ESplineCoordinateSpace::Local),
                    false);
                SegmentComp->SetStartScale(SegmentScale, false);
                SegmentComp->SetEndScale(SegmentScale, true);
                ++MeshSegments;
            }
        }
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("templateType"), TemplateName);
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    Result->SetNumberField(TEXT("meshSegments"), MeshSegments);
    Result->SetBoolField(TEXT("closedLoop"), bClosedLoop);
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
