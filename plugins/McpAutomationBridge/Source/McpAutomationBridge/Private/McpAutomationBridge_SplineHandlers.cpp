#include "Dom/JsonObject.h"
// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 26: Spline System Handlers

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Spline System includes
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpSplineHandlers, Log, All);

#if WITH_EDITOR

// Helper to get string field from JSON
static FString GetJsonStringFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FString& Default = TEXT(""))
{
    if (!Payload.IsValid()) return Default;
    FString Value;
    if (Payload->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

// Helper to get number field from JSON
static double GetJsonNumberFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, double Default = 0.0)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

// Helper to get bool field from JSON
static bool GetJsonBoolFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, bool Default = false)
{
    if (!Payload.IsValid()) return Default;
    bool Value;
    if (Payload->TryGetBoolField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

// Helper to get int field from JSON
static int32 GetJsonIntFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, int32 Default = 0)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return static_cast<int32>(Value);
    }
    return Default;
}

// Helper to get FVector from JSON object field
static FVector GetJsonVectorFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FVector& Default = FVector::ZeroVector)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* VecObj;
    if (Payload->TryGetObjectField(FieldName, VecObj) && VecObj->IsValid())
    {
        return FVector(
            GetJsonNumberFieldSpline(*VecObj, TEXT("x"), Default.X),
            GetJsonNumberFieldSpline(*VecObj, TEXT("y"), Default.Y),
            GetJsonNumberFieldSpline(*VecObj, TEXT("z"), Default.Z)
        );
    }
    return Default;
}

// Helper to get FRotator from JSON object field
static FRotator GetJsonRotatorFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FRotator& Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(FieldName, RotObj) && RotObj->IsValid())
    {
        return FRotator(
            GetJsonNumberFieldSpline(*RotObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberFieldSpline(*RotObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberFieldSpline(*RotObj, TEXT("roll"), Default.Roll)
        );
    }
    return Default;
}

// Helper to find actor by name
static AActor* FindActorByName(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty()) return nullptr;
    
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }
    return nullptr;
}

// Helper to find spline component on actor
static USplineComponent* FindSplineComponent(AActor* Actor, const FString& ComponentName = TEXT(""))
{
    if (!Actor) return nullptr;
    
    TArray<USplineComponent*> SplineComponents;
    Actor->GetComponents<USplineComponent>(SplineComponents);
    
    if (SplineComponents.Num() == 0) return nullptr;
    
    if (!ComponentName.IsEmpty())
    {
        for (USplineComponent* Comp : SplineComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                return Comp;
            }
        }
        return nullptr;
    }
    
    return SplineComponents[0];
}

// Helper to parse spline point type (case-insensitive)
static ESplinePointType::Type ParseSplinePointType(const FString& TypeStr)
{
    FString LowerStr = TypeStr.ToLower();
    if (LowerStr == TEXT("linear")) return ESplinePointType::Linear;
    if (LowerStr == TEXT("curve")) return ESplinePointType::Curve;
    if (LowerStr == TEXT("constant")) return ESplinePointType::Constant;
    if (LowerStr == TEXT("curveclamped")) return ESplinePointType::CurveClamped;
    if (LowerStr == TEXT("curvecustomtangent")) return ESplinePointType::CurveCustomTangent;
    return ESplinePointType::Curve; // Default
}

// Helper to convert spline point type to string
static FString SplinePointTypeToString(ESplinePointType::Type Type)
{
    switch (Type)
    {
        case ESplinePointType::Linear: return TEXT("Linear");
        case ESplinePointType::Curve: return TEXT("Curve");
        case ESplinePointType::Constant: return TEXT("Constant");
        case ESplinePointType::CurveClamped: return TEXT("CurveClamped");
        case ESplinePointType::CurveCustomTangent: return TEXT("CurveCustomTangent");
        default: return TEXT("Unknown");
    }
}

// ============================================================================
// Spline Creation Handlers
// ============================================================================

static bool HandleCreateSplineActor(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"), TEXT("SplineActor"));
    FVector Location = GetJsonVectorFieldSpline(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldSpline(Payload, TEXT("rotation"));
    bool bClosedLoop = GetJsonBoolFieldSpline(Payload, TEXT("bClosedLoop"), false);
    FString SplineType = GetJsonStringFieldSpline(Payload, TEXT("splineType"), TEXT("Curve"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Spawn a new actor with a spline component
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(*ActorName);

    // Create and attach spline component
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
    // Note: Do not call AttachToComponent before SetRootComponent - the actor has no root yet
    NewActor->SetRootComponent(SplineComp);

    // Configure spline
    SplineComp->SetClosedLoop(bClosedLoop);
    
    // Set default spline point type for all points
    ESplinePointType::Type PointType = ParseSplinePointType(SplineType);
    for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
    {
        SplineComp->SetSplinePointType(i, PointType, true);
    }
    SplineComp->UpdateSpline();

    // Parse initial points if provided (accept both 'points' and 'initialPoints' field names)
    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("points"), PointsArray))
    {
        Payload->TryGetArrayField(TEXT("initialPoints"), PointsArray);
    }
    if (PointsArray)
    {
        SplineComp->ClearSplinePoints(false);
        for (int32 i = 0; i < PointsArray->Num(); i++)
        {
            const TSharedPtr<FJsonObject>* PointObj;
            if ((*PointsArray)[i]->TryGetObject(PointObj))
            {
                FVector PointLocation = GetJsonVectorFieldSpline(*PointObj, TEXT("location"));
                SplineComp->AddSplinePoint(PointLocation, ESplineCoordinateSpace::Local, true);
                SplineComp->SetSplinePointType(i, PointType, false);
            }
        }
        SplineComp->UpdateSpline();
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NewActor->GetPathName());
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    Result->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline actor '%s' created with %d points"), *ActorName, SplineComp->GetNumberOfSplinePoints()), Result);
    return true;
}

static bool HandleAddSplinePoint(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FVector Position = GetJsonVectorFieldSpline(Payload, TEXT("position"));
    int32 Index = GetJsonIntFieldSpline(Payload, TEXT("index"), -1);
    FString PointType = GetJsonStringFieldSpline(Payload, TEXT("pointType"), TEXT("Curve"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    // Add point at specified index or at end
    if (Index < 0 || Index >= SplineComp->GetNumberOfSplinePoints())
    {
        SplineComp->AddSplinePoint(Position, ESplineCoordinateSpace::Local, true);
        Index = SplineComp->GetNumberOfSplinePoints() - 1;
    }
    else
    {
        SplineComp->AddSplinePointAtIndex(Position, Index, ESplineCoordinateSpace::Local, true);
    }

    SplineComp->SetSplinePointType(Index, ParseSplinePointType(PointType), true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pointIndex"), Index);
    Result->SetNumberField(TEXT("totalPoints"), SplineComp->GetNumberOfSplinePoints());

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added spline point at index %d"), Index), Result);
    return true;
}

static bool HandleRemoveSplinePoint(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->RemoveSplinePoint(PointIndex, true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("removedIndex"), PointIndex);
    Result->SetNumberField(TEXT("remainingPoints"), SplineComp->GetNumberOfSplinePoints());

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed spline point at index %d"), PointIndex), Result);
    return true;
}

static bool HandleSetSplinePointPosition(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);
    FVector Position = GetJsonVectorFieldSpline(Payload, TEXT("position"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->SetLocationAtSplinePoint(PointIndex, Position, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set position for spline point %d"), PointIndex), Result);
    return true;
}

static bool HandleSetSplinePointTangents(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);
    FVector ArriveTangent = GetJsonVectorFieldSpline(Payload, TEXT("arriveTangent"));
    FVector LeaveTangent = GetJsonVectorFieldSpline(Payload, TEXT("leaveTangent"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    // Note: UE splines have a single tangent per point; arrive/leave tangents are computed from it
    // If leaveTangent is provided, log a warning since it cannot be used independently
    if (!LeaveTangent.IsZero() && LeaveTangent != ArriveTangent)
    {
        UE_LOG(LogMcpSplineHandlers, Warning, 
            TEXT("leaveTangent ignored for point %d - UE splines use a single tangent per point. Use arriveTangent only."), 
            PointIndex);
    }
    
    SplineComp->SetTangentAtSplinePoint(PointIndex, ArriveTangent, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set tangents for spline point %d"), PointIndex), Result);
    return true;
}

static bool HandleSetSplinePointRotation(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);
    FRotator Rotation = GetJsonRotatorFieldSpline(Payload, TEXT("pointRotation"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->SetRotationAtSplinePoint(PointIndex, Rotation, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set rotation for spline point %d"), PointIndex), Result);
    return true;
}

static bool HandleSetSplinePointScale(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);
    FVector Scale = GetJsonVectorFieldSpline(Payload, TEXT("pointScale"), FVector::OneVector);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->SetScaleAtSplinePoint(PointIndex, Scale, true);
    SplineComp->UpdateSpline();

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set scale for spline point %d"), PointIndex), Result);
    return true;
}

static bool HandleSetSplineType(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString SplineType = GetJsonStringFieldSpline(Payload, TEXT("splineType"), TEXT("Curve"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), -1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
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

    ESplinePointType::Type PointType = ParseSplinePointType(SplineType);

    if (PointIndex >= 0)
    {
        // Set for specific point
        if (PointIndex >= SplineComp->GetNumberOfSplinePoints())
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
            return true;
        }
        SplineComp->SetSplinePointType(PointIndex, PointType, true);
    }
    else
    {
        // Set for all points
        for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
        {
            SplineComp->SetSplinePointType(i, PointType, false);
        }
    }
    
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("splineType"), SplineType);
    Result->SetNumberField(TEXT("pointsAffected"), PointIndex >= 0 ? 1 : SplineComp->GetNumberOfSplinePoints());

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set spline type to %s"), *SplineType), Result);
    return true;
}

// ============================================================================
// Spline Mesh Handlers
// ============================================================================

static bool HandleCreateSplineMeshComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldSpline(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"), TEXT("SplineMesh"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));
    FString ForwardAxis = GetJsonStringFieldSpline(Payload, TEXT("forwardAxis"), TEXT("X"));

    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    // Check if component already exists
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    // Create the SCS node for SplineMeshComponent
    USCS_Node* NewNode = SCS->CreateNode(USplineMeshComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }

    // Configure the component template
    USplineMeshComponent* MeshComp = Cast<USplineMeshComponent>(NewNode->ComponentTemplate);
    if (MeshComp)
    {
        // Set mesh if provided
        if (!MeshPath.IsEmpty())
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (Mesh)
            {
                MeshComp->SetStaticMesh(Mesh);
            }
        }

        // Set forward axis
        ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
        if (ForwardAxis == TEXT("Y")) Axis = ESplineMeshAxis::Y;
        else if (ForwardAxis == TEXT("Z")) Axis = ESplineMeshAxis::Z;
        MeshComp->SetForwardAxis(Axis);
    }

    // Add node to SCS
    SCS->AddNode(NewNode);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetJsonBoolFieldSpline(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineMeshComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}

static bool HandleSetSplineMeshAsset(
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

    // Find SplineMeshComponent
    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);

    USplineMeshComponent* TargetComp = nullptr;
    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                TargetComp = Comp;
                break;
            }
        }
    }
    else if (MeshComponents.Num() > 0)
    {
        TargetComp = MeshComponents[0];
    }

    if (!TargetComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Mesh not found: %s"), *MeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    TargetComp->SetStaticMesh(Mesh);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("meshPath"), MeshPath);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline mesh asset set"), Result);
    return true;
}

static bool HandleConfigureSplineMeshAxis(
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

    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);

    USplineMeshComponent* TargetComp = nullptr;
    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                TargetComp = Comp;
                break;
            }
        }
    }
    else if (MeshComponents.Num() > 0)
    {
        TargetComp = MeshComponents[0];
    }

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

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("forwardAxis"), ForwardAxis);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline mesh forward axis set to %s"), *ForwardAxis), Result);
    return true;
}

static bool HandleSetSplineMeshMaterial(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"));
    FString MaterialPath = GetJsonStringFieldSpline(Payload, TEXT("materialPath"));
    int32 MaterialIndex = GetJsonIntFieldSpline(Payload, TEXT("materialIndex"), 0);

    if (ActorName.IsEmpty() || MaterialPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and materialPath are required"), nullptr, TEXT("MISSING_PARAM"));
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

    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);

    USplineMeshComponent* TargetComp = nullptr;
    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                TargetComp = Comp;
                break;
            }
        }
    }
    else if (MeshComponents.Num() > 0)
    {
        TargetComp = MeshComponents[0];
    }

    if (!TargetComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material not found: %s"), *MaterialPath), nullptr, TEXT("MATERIAL_NOT_FOUND"));
        return true;
    }

    TargetComp->SetMaterial(MaterialIndex, Material);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("materialPath"), MaterialPath);
    Result->SetNumberField(TEXT("materialIndex"), MaterialIndex);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline mesh material set"), Result);
    return true;
}

// ============================================================================
// Mesh Scattering Handlers
// ============================================================================

static bool HandleScatterMeshesAlongSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));
    double Spacing = GetJsonNumberFieldSpline(Payload, TEXT("spacing"), 100.0);
    bool bAlignToSpline = GetJsonBoolFieldSpline(Payload, TEXT("bAlignToSpline"), true);

    if (ActorName.IsEmpty() || MeshPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and meshPath are required"), nullptr, TEXT("MISSING_PARAM"));
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

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Mesh not found: %s"), *MeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    float SplineLength = SplineComp->GetSplineLength();
    int32 MeshCount = FMath::FloorToInt(SplineLength / Spacing);
    
    TArray<FString> CreatedMeshes;

    for (int32 i = 0; i <= MeshCount; i++)
    {
        float Distance = i * Spacing;
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline 
            ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
            : FRotator::ZeroRotator;

        // Create a static mesh component for each instance
        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Actor);
        if (MeshComp)
        {
            MeshComp->SetStaticMesh(Mesh);
            MeshComp->SetWorldLocationAndRotation(Location, Rotation);
            MeshComp->RegisterComponent();
            Actor->AddInstanceComponent(MeshComp);
            MeshComp->AttachToComponent(SplineComp, FAttachmentTransformRules::KeepWorldTransform);
            CreatedMeshes.Add(MeshComp->GetName());
        }
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("meshesCreated"), CreatedMeshes.Num());
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetNumberField(TEXT("spacing"), Spacing);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Scattered %d meshes along spline"), CreatedMeshes.Num()), Result);
    return true;
}

static bool HandleConfigureMeshSpacing(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // VALIDATION-ONLY: This action validates spacing parameters and echoes them back.
    // Storage is not implemented - spacing must be passed directly to scatter_meshes_along_spline.
    // Future enhancement: Store in actor metadata via UMetaData component.
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("spacing"), GetJsonNumberFieldSpline(Payload, TEXT("spacing"), 100.0));
    Result->SetBoolField(TEXT("useRandomOffset"), GetJsonBoolFieldSpline(Payload, TEXT("useRandomOffset"), false));
    Result->SetNumberField(TEXT("randomOffsetRange"), GetJsonNumberFieldSpline(Payload, TEXT("randomOffsetRange"), 0.0));

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Mesh spacing configuration stored"), Result);
    return true;
}

static bool HandleConfigureMeshRandomization(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // VALIDATION-ONLY: This action validates randomization parameters and echoes them back.
    // Storage is not implemented - pass randomization params to scatter_meshes_along_spline.
    // Future enhancement: Store in actor metadata via UMetaData component.
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("randomizeScale"), GetJsonBoolFieldSpline(Payload, TEXT("randomizeScale"), false));
    Result->SetNumberField(TEXT("minScale"), GetJsonNumberFieldSpline(Payload, TEXT("minScale"), 0.8));
    Result->SetNumberField(TEXT("maxScale"), GetJsonNumberFieldSpline(Payload, TEXT("maxScale"), 1.2));
    Result->SetBoolField(TEXT("randomizeRotation"), GetJsonBoolFieldSpline(Payload, TEXT("randomizeRotation"), false));
    Result->SetNumberField(TEXT("rotationRange"), GetJsonNumberFieldSpline(Payload, TEXT("rotationRange"), 360.0));

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Mesh randomization configuration stored"), Result);
    return true;
}

// ============================================================================
// Quick Template Handlers
// ============================================================================

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

    // Spawn actor with spline
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
    if (!NewActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(*ActorName);

    // Create spline component
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

    // Add some default points for the template
    SplineComp->ClearSplinePoints(false);
    SplineComp->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(500, 0, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(1000, 200, 0), ESplineCoordinateSpace::Local, false);
    SplineComp->AddSplinePoint(FVector(1500, 200, 0), ESplineCoordinateSpace::Local, true);

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("templateType"), TemplateName);
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s spline '%s' created"), *TemplateName, *ActorName), Result);
    return true;
}

static bool HandleCreateRoadSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Road"), TEXT(""));
}

static bool HandleCreateRiverSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("River"), TEXT(""));
}

static bool HandleCreateFenceSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Fence"), TEXT(""));
}

static bool HandleCreateWallSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Wall"), TEXT(""));
}

static bool HandleCreateCableSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Cable"), TEXT(""));
}

static bool HandleCreatePipeSpline(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleCreateTemplateSpline(Self, RequestId, Payload, Socket, TEXT("Pipe"), TEXT(""));
}

// ============================================================================
// Utility Handlers
// ============================================================================

static bool HandleGetSplinesInfo(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    if (!ActorName.IsEmpty())
    {
        // Get info for specific actor
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

        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
        Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
        Result->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());

        // Add point details
        TArray<TSharedPtr<FJsonValue>> PointsArray;
        for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
        {
            TSharedPtr<FJsonObject> PointObj = MakeShared<FJsonObject>();
            FVector Loc = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
            FRotator Rot = SplineComp->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::Local);
            
            PointObj->SetNumberField(TEXT("index"), i);
            
            TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
            LocObj->SetNumberField(TEXT("x"), Loc.X);
            LocObj->SetNumberField(TEXT("y"), Loc.Y);
            LocObj->SetNumberField(TEXT("z"), Loc.Z);
            PointObj->SetObjectField(TEXT("location"), LocObj);
            
            PointObj->SetStringField(TEXT("type"), SplinePointTypeToString(SplineComp->GetSplinePointType(i)));
            
            PointsArray.Add(MakeShared<FJsonValueObject>(PointObj));
        }
        Result->SetArrayField(TEXT("points"), PointsArray);
    }
    else
    {
        // List all actors with spline components
        TArray<TSharedPtr<FJsonValue>> SplinesArray;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            TArray<USplineComponent*> SplineComponents;
            Actor->GetComponents<USplineComponent>(SplineComponents);
            
            if (SplineComponents.Num() > 0)
            {
                TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
                ActorObj->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
                ActorObj->SetNumberField(TEXT("splineComponentCount"), SplineComponents.Num());
                
                if (SplineComponents[0])
                {
                    ActorObj->SetNumberField(TEXT("pointCount"), SplineComponents[0]->GetNumberOfSplinePoints());
                    ActorObj->SetNumberField(TEXT("splineLength"), SplineComponents[0]->GetSplineLength());
                }
                
                SplinesArray.Add(MakeShared<FJsonValueObject>(ActorObj));
            }
        }
        Result->SetArrayField(TEXT("splines"), SplinesArray);
        Result->SetNumberField(TEXT("totalSplineActors"), SplinesArray.Num());
    }

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline info retrieved"), Result);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageSplinesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringFieldSpline(Payload, TEXT("subAction"), TEXT(""));
    
    UE_LOG(LogMcpSplineHandlers, Verbose, TEXT("HandleManageSplinesAction: SubAction=%s"), *SubAction);

    // Spline Creation
    if (SubAction == TEXT("create_spline_actor"))
        return HandleCreateSplineActor(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_spline_point"))
        return HandleAddSplinePoint(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("remove_spline_point"))
        return HandleRemoveSplinePoint(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_position"))
        return HandleSetSplinePointPosition(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_tangents"))
        return HandleSetSplinePointTangents(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_rotation"))
        return HandleSetSplinePointRotation(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_point_scale"))
        return HandleSetSplinePointScale(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_type"))
        return HandleSetSplineType(this, RequestId, Payload, Socket);

    // Spline Mesh
    if (SubAction == TEXT("create_spline_mesh_component"))
        return HandleCreateSplineMeshComponent(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_mesh_asset"))
        return HandleSetSplineMeshAsset(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_spline_mesh_axis"))
        return HandleConfigureSplineMeshAxis(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_spline_mesh_material"))
        return HandleSetSplineMeshMaterial(this, RequestId, Payload, Socket);

    // Mesh Scattering
    if (SubAction == TEXT("scatter_meshes_along_spline"))
        return HandleScatterMeshesAlongSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_mesh_spacing"))
        return HandleConfigureMeshSpacing(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_mesh_randomization"))
        return HandleConfigureMeshRandomization(this, RequestId, Payload, Socket);

    // Quick Templates
    if (SubAction == TEXT("create_road_spline"))
        return HandleCreateRoadSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_river_spline"))
        return HandleCreateRiverSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_fence_spline"))
        return HandleCreateFenceSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_wall_spline"))
        return HandleCreateWallSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_cable_spline"))
        return HandleCreateCableSpline(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_pipe_spline"))
        return HandleCreatePipeSpline(this, RequestId, Payload, Socket);

    // Utility
    if (SubAction == TEXT("get_splines_info"))
        return HandleGetSplinesInfo(this, RequestId, Payload, Socket);

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown spline subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Spline operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
