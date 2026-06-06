#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleScatterMeshesAlongSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));
    bool bAlignToSpline = GetJsonBoolFieldSpline(Payload, TEXT("alignToSpline"), true);

    FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (SafeMeshPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe meshPath: %s. Path must be relative to project (e.g., /Game/...)"), *MeshPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = FindSplineComponent(Actor);
    if (!SplineComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No spline component found on actor"), nullptr, TEXT("NO_SPLINE"));
        return true;
    }

    const bool bHasSpacing = Payload.IsValid() && Payload->HasField(TEXT("spacing"));
    const bool bHasUseRandomOffset = Payload.IsValid() && Payload->HasField(TEXT("useRandomOffset"));
    const bool bHasRandomOffsetRange = Payload.IsValid() && Payload->HasField(TEXT("randomOffsetRange"));
    const bool bHasRandomizeScale = Payload.IsValid() && Payload->HasField(TEXT("randomizeScale"));
    const bool bHasMinScale = Payload.IsValid() && Payload->HasField(TEXT("minScale"));
    const bool bHasMaxScale = Payload.IsValid() && Payload->HasField(TEXT("maxScale"));
    const bool bHasRandomizeRotation = Payload.IsValid() && Payload->HasField(TEXT("randomizeRotation"));
    const bool bHasRotationRange = Payload.IsValid() && Payload->HasField(TEXT("rotationRange"));

    double Spacing = bHasSpacing
        ? GetJsonNumberFieldSpline(Payload, TEXT("spacing"), 100.0)
        : GetConfiguredSplineNumber(Actor, World, TEXT("meshSpacing"), 100.0);
    const bool bUseRandomOffset = bHasUseRandomOffset
        ? GetJsonBoolFieldSpline(Payload, TEXT("useRandomOffset"), false)
        : GetConfiguredSplineBool(Actor, World, TEXT("useRandomOffset"), false);
    const double RandomOffsetRange = bHasRandomOffsetRange
        ? GetJsonNumberFieldSpline(Payload, TEXT("randomOffsetRange"), 0.0)
        : GetConfiguredSplineNumber(Actor, World, TEXT("randomOffsetRange"), 0.0);
    const bool bRandomizeScale = bHasRandomizeScale
        ? GetJsonBoolFieldSpline(Payload, TEXT("randomizeScale"), false)
        : GetConfiguredSplineBool(Actor, World, TEXT("randomizeScale"), false);
    const double MinScale = bHasMinScale
        ? GetJsonNumberFieldSpline(Payload, TEXT("minScale"), 0.8)
        : GetConfiguredSplineNumber(Actor, World, TEXT("minScale"), 0.8);
    const double MaxScale = bHasMaxScale
        ? GetJsonNumberFieldSpline(Payload, TEXT("maxScale"), 1.2)
        : GetConfiguredSplineNumber(Actor, World, TEXT("maxScale"), 1.2);
    const bool bRandomizeRotation = bHasRandomizeRotation
        ? GetJsonBoolFieldSpline(Payload, TEXT("randomizeRotation"), false)
        : GetConfiguredSplineBool(Actor, World, TEXT("randomizeRotation"), false);
    const double RotationRange = bHasRotationRange
        ? GetJsonNumberFieldSpline(Payload, TEXT("rotationRange"), 360.0)
        : GetConfiguredSplineNumber(Actor, World, TEXT("rotationRange"), 360.0);

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
    if (!Mesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Mesh not found: %s"), *SafeMeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    if (Spacing <= 0.0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("spacing must be greater than 0"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    if (RandomOffsetRange < 0.0 || MinScale <= 0.0 || MaxScale <= 0.0 || MinScale > MaxScale || RotationRange < 0.0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid spline mesh randomization configuration"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    float SplineLength = SplineComp->GetSplineLength();
    int32 MeshCount = FMath::FloorToInt(SplineLength / Spacing);
    TArray<FString> CreatedMeshes;

    for (int32 i = 0; i <= MeshCount; i++)
    {
        float Distance = static_cast<float>(i * Spacing);
        if (bUseRandomOffset && RandomOffsetRange > 0.0)
        {
            Distance += FMath::FRandRange(static_cast<float>(-RandomOffsetRange), static_cast<float>(RandomOffsetRange));
            Distance = FMath::Clamp(Distance, 0.0f, SplineLength);
        }
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline
            ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
            : FRotator::ZeroRotator;

        if (bRandomizeRotation && RotationRange > 0.0)
        {
            Rotation.Yaw += FMath::FRandRange(static_cast<float>(-RotationRange), static_cast<float>(RotationRange));
        }

        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Actor);
        if (MeshComp)
        {
            MeshComp->SetStaticMesh(Mesh);
            MeshComp->SetWorldLocationAndRotation(Location, Rotation);
            if (bRandomizeScale)
            {
                const float UniformScale = FMath::FRandRange(static_cast<float>(MinScale), static_cast<float>(MaxScale));
                MeshComp->SetWorldScale3D(FVector(UniformScale));
            }
            MeshComp->RegisterComponent();
            Actor->AddInstanceComponent(MeshComp);
            MeshComp->AttachToComponent(SplineComp, FAttachmentTransformRules::KeepWorldTransform);
            CreatedMeshes.Add(MeshComp->GetName());
        }
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("meshesCreated"), CreatedMeshes.Num());
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetNumberField(TEXT("spacing"), Spacing);
    Result->SetBoolField(TEXT("useRandomOffset"), bUseRandomOffset);
    Result->SetNumberField(TEXT("randomOffsetRange"), RandomOffsetRange);
    Result->SetBoolField(TEXT("randomizeScale"), bRandomizeScale);
    Result->SetNumberField(TEXT("minScale"), MinScale);
    Result->SetNumberField(TEXT("maxScale"), MaxScale);
    Result->SetBoolField(TEXT("randomizeRotation"), bRandomizeRotation);
    Result->SetNumberField(TEXT("rotationRange"), RotationRange);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Scattered %d meshes along spline"), CreatedMeshes.Num()), Result);
    return true;
}
#endif
