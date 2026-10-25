#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
// What a template draws without a meshPath, and the cross-section it gives the mesh. Every template drew the same
// flat ribbon (a wall or a pipe came out as a road), and nothing at all without a meshPath.
struct FMcpSplineKind
{
    const TCHAR* Name;
    const TCHAR* StockMesh;
    ESplineMeshAxis::Type StockAxis; // the stock mesh's length axis
    double Width;                    // across the route; the diameter of a round kind
    double Height;                   // up from the route; a round kind takes Width
    bool bRound;
    bool bStandsOnRoute;             // the mesh's bottom on the route (a wall), not its middle (a road)
};

const TCHAR* const StockCube = TEXT("/Engine/BasicShapes/Cube.Cube");
const TCHAR* const StockCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

// 0 (nothing asked, a mesh of the caller's) keeps the mesh's own size on that axis.
double FitScale(double Want, double Have)
{
    return Want > 0.0 && Have > 1e-4 ? Want / Have : 1.0;
}
}

static bool HandleCreateTemplateSpline(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FMcpSplineKind& Kind)
{
    // `name` is the spelling the templates publish and wins over the legacy `actorName`.
    FString ActorName = McpGetFirstStringField(Payload, {TEXT("name"), TEXT("actorName")});
    if (ActorName.IsEmpty())
    {
        ActorName = FString(Kind.Name) + TEXT("_Spline");
    }
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    const FString MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
    const bool bStock = MeshPath.IsEmpty();
    const bool bHasWidth = Payload->HasField(TEXT("width"));
    const bool bHasHeight = Kind.bStandsOnRoute && Payload->HasField(TEXT("height"));
    const double Width = bHasWidth ? GetJsonNumberField(Payload, TEXT("width"), 0.0) : (bStock ? Kind.Width : 0.0);
    const double Height = Kind.bRound ? Width
        : bHasHeight ? GetJsonNumberField(Payload, TEXT("height"), 0.0) : (bStock ? Kind.Height : 0.0);
    if ((bHasWidth && Width <= 0.0) || (bHasHeight && Height <= 0.0))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("width and height must be greater than 0; nothing was created."), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Everything loads before anything spawns: a mesh or material that failed to load left a bare spline and a success.
    UStaticMesh* SegmentMesh = nullptr;
    if (bStock)
    {
        SegmentMesh = LoadObject<UStaticMesh>(nullptr, Kind.StockMesh);
    }
    else
    {
        const FString SafeMeshPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("meshPath"), MeshPath);
        if (SafeMeshPath.IsEmpty())
        {
            return true;
        }
        SegmentMesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
    }
    if (!SegmentMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Static mesh not found: %s; nothing was created."), bStock ? Kind.StockMesh : *MeshPath),
            nullptr, TEXT("MESH_NOT_FOUND"));
        return true;
    }
    UMaterialInterface* SegmentMaterial = nullptr;
    if (!MaterialPath.IsEmpty())
    {
        const FString SafeMaterialPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("materialPath"), MaterialPath);
        if (SafeMaterialPath.IsEmpty())
        {
            return true;
        }
        SegmentMaterial = LoadObject<UMaterialInterface>(nullptr, *SafeMaterialPath);
        if (!SegmentMaterial)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Material not found: %s; nothing was created."), *MaterialPath), nullptr, TEXT("MATERIAL_NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    USplineComponent* SplineComp = SpawnSplineActor(World, ActorName, Location, FRotator::ZeroRotator);
    if (!SplineComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }
    AActor* NewActor = SplineComp->GetOwner();

    // The caller's route, local to location; without one, a short demo route.
    SplineComp->ClearSplinePoints(false);
    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("points"), PointsArray) && PointsArray && PointsArray->Num() >= 2)
    {
        AddSplinePointsFromJson(SplineComp, *PointsArray);
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

    // The spline mesh maps the mesh's two cross-section axes to across the route, then up: Y and Z for a mesh that
    // runs along X, X and Y for one along Z (the stock cylinder).
    const ESplineMeshAxis::Type Axis = bStock ? Kind.StockAxis : ESplineMeshAxis::X;
    const int32 SideAxis = Axis == ESplineMeshAxis::Z ? 0 : 1;
    const int32 UpAxis = Axis == ESplineMeshAxis::Z ? 1 : 2;
    const FBoxSphereBounds Bounds = SegmentMesh->GetBounds();
    const FVector2D SegmentScale(FitScale(Width, 2.0 * Bounds.BoxExtent[SideAxis]), FitScale(Height, 2.0 * Bounds.BoxExtent[UpAxis]));
    // A wall stands on its route: the mesh's lowest point goes on the spline instead of its middle.
    const FVector2D SegmentOffset(0.0, Kind.bStandsOnRoute
        ? (Bounds.BoxExtent[UpAxis] - Bounds.Origin[UpAxis]) * SegmentScale.Y : 0.0);

    int32 MeshSegments = 0;
    const int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
    const int32 SegmentCount = bClosedLoop ? NumPoints : NumPoints - 1;
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
        SegmentComp->SetForwardAxis(Axis, false);

        // A fresh spline mesh arrives with NoCollision, so a road or wall nothing can stand on or bump into only
        // showed at play time. Spline meshes deform their collision with the mesh, so it matches what is drawn.
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
        SegmentComp->SetStartOffset(SegmentOffset, false);
        SegmentComp->SetEndOffset(SegmentOffset, false);
        SegmentComp->SetStartScale(SegmentScale, false);
        SegmentComp->SetEndScale(SegmentScale, true);
        ++MeshSegments;
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), McpActorRef(NewActor));
    Result->SetStringField(TEXT("templateType"), Kind.Name);
    Result->SetNumberField(TEXT("pointCount"), NumPoints);
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    Result->SetNumberField(TEXT("meshSegments"), MeshSegments);
    Result->SetStringField(TEXT("meshPath"), SegmentMesh->GetPathName());
    Result->SetNumberField(TEXT("width"), SegmentScale.X * 2.0 * Bounds.BoxExtent[SideAxis]);
    Result->SetNumberField(TEXT("height"), SegmentScale.Y * 2.0 * Bounds.BoxExtent[UpAxis]);
    Result->SetBoolField(TEXT("closedLoop"), bClosedLoop);
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s spline '%s' created: %d segments of %s"), Kind.Name, *ActorName, MeshSegments,
                        *SegmentMesh->GetName()), Result);
    return true;
}

bool HandleCreateRoadSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("Road"), StockCube, ESplineMeshAxis::X, 400.0, 8.0, false, false});
}

bool HandleCreateRiverSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("River"), StockCube, ESplineMeshAxis::X, 400.0, 8.0, false, false});
}

bool HandleCreateFenceSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("Fence"), StockCube, ESplineMeshAxis::X, 10.0, 100.0, false, true});
}

bool HandleCreateWallSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("Wall"), StockCube, ESplineMeshAxis::X, 20.0, 300.0, false, true});
}

bool HandleCreateCableSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("Cable"), StockCylinder, ESplineMeshAxis::Z, 4.0, 4.0, true, false});
}

bool HandleCreatePipeSpline(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, {TEXT("Pipe"), StockCylinder, ESplineMeshAxis::Z, 50.0, 50.0, true, false});
}
