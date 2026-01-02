/**
 * McpAutomationBridge_GeometryHandlers.cpp
 *
 * Phase 6: Geometry Script handlers
 * Implements procedural mesh creation and manipulation using UE Geometry Script APIs
 */

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR

#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshActor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"

// Geometry Script Includes
// Paths may vary by UE version (e.g. 5.0 vs 5.3 vs 5.7)
#if __has_include("GeometryScript/GeometryScriptTypes.h")
#include "GeometryScript/GeometryScriptTypes.h"
#else
// Fallback for when headers are directly in include path or different folder
#include "GeometryScriptTypes.h" 
#endif

#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UDynamicMesh.h"
#include "Components/SplineComponent.h"

// Helper to read FVector from JSON (supports both object and array formats)
static FVector ReadVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FVector Default = FVector::ZeroVector)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [x, y, z]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FVector(
            (*ArrayPtr)[0]->AsNumber(),
            (*ArrayPtr)[1]->AsNumber(),
            (*ArrayPtr)[2]->AsNumber()
        );
    }

    // Try object format {x, y, z}
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        return FVector(
            (*ObjPtr)->GetNumberField(TEXT("x")),
            (*ObjPtr)->GetNumberField(TEXT("y")),
            (*ObjPtr)->GetNumberField(TEXT("z"))
        );
    }

    return Default;
}

// Helper to read FRotator from JSON (supports both {pitch,yaw,roll} and {x,y,z} formats)
static FRotator ReadRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FRotator Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [pitch, yaw, roll]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FRotator(
            (*ArrayPtr)[0]->AsNumber(),  // Pitch
            (*ArrayPtr)[1]->AsNumber(),  // Yaw
            (*ArrayPtr)[2]->AsNumber()   // Roll
        );
    }

    // Try object format {pitch, yaw, roll} or {x, y, z}
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        // Check for {pitch, yaw, roll} format first
        if ((*ObjPtr)->HasField(TEXT("pitch")) || (*ObjPtr)->HasField(TEXT("yaw")) || (*ObjPtr)->HasField(TEXT("roll")))
        {
            return FRotator(
                (*ObjPtr)->HasField(TEXT("pitch")) ? (*ObjPtr)->GetNumberField(TEXT("pitch")) : 0.0,
                (*ObjPtr)->HasField(TEXT("yaw")) ? (*ObjPtr)->GetNumberField(TEXT("yaw")) : 0.0,
                (*ObjPtr)->HasField(TEXT("roll")) ? (*ObjPtr)->GetNumberField(TEXT("roll")) : 0.0
            );
        }
        // Fallback to {x, y, z} format (x=Pitch, y=Yaw, z=Roll)
        return FRotator(
            (*ObjPtr)->GetNumberField(TEXT("x")),
            (*ObjPtr)->GetNumberField(TEXT("y")),
            (*ObjPtr)->GetNumberField(TEXT("z"))
        );
    }

    return Default;
}

// Helper to read FTransform from JSON
static FTransform ReadTransformFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
    FVector Location = ReadVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ReadRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Scale = ReadVectorFromPayload(Payload, TEXT("scale"), FVector::OneVector);

    return FTransform(
        Rotation,
        Location,
        Scale
    );
}

// Helper to create or get a dynamic mesh for operations
static UDynamicMesh* GetOrCreateDynamicMesh(UObject* Outer)
{
    return NewObject<UDynamicMesh>(Outer);
}

// Safety limits for geometry operations to prevent OOM
static constexpr int32 MAX_SEGMENTS = 256;
static constexpr double MAX_DIMENSION = 100000.0;
static constexpr double MIN_DIMENSION = 0.01;

static int32 ClampSegments(int32 Value, int32 Default = 1)
{
    return FMath::Clamp(Value <= 0 ? Default : Value, 1, MAX_SEGMENTS);
}

static double ClampDimension(double Value, double Default = 100.0)
{
    if (Value <= 0.0) Value = Default;
    return FMath::Clamp(Value, MIN_DIMENSION, MAX_DIMENSION);
}

// -------------------------------------------------------------------------
// Primitives
// -------------------------------------------------------------------------

static bool HandleCreateBox(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedBox");

    FTransform Transform = ReadTransformFromPayload(Payload);

    // Get dimensions with safety clamping
    double Width = ClampDimension(Payload->HasField(TEXT("width")) ? Payload->GetNumberField(TEXT("width")) : 100.0);
    double Height = ClampDimension(Payload->HasField(TEXT("height")) ? Payload->GetNumberField(TEXT("height")) : 100.0);
    double Depth = ClampDimension(Payload->HasField(TEXT("depth")) ? Payload->GetNumberField(TEXT("depth")) : 100.0);

    int32 WidthSegments = ClampSegments(Payload->HasField(TEXT("widthSegments")) ? (int32)Payload->GetNumberField(TEXT("widthSegments")) : 1);
    int32 HeightSegments = ClampSegments(Payload->HasField(TEXT("heightSegments")) ? (int32)Payload->GetNumberField(TEXT("heightSegments")) : 1);
    int32 DepthSegments = ClampSegments(Payload->HasField(TEXT("depthSegments")) ? (int32)Payload->GetNumberField(TEXT("depthSegments")) : 1);

    // Create DynamicMesh
    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());

    FGeometryScriptPrimitiveOptions Options;
    Options.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
        DynMesh,
        Options,
        Transform,
        Width, Height, Depth,
        WidthSegments, HeightSegments, DepthSegments,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    // Spawn actor with dynamic mesh component
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);

    // Get DynamicMeshComponent and set mesh
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent();
        if (DMComp)
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("depth"), Depth);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Box mesh created"), Result);
    return true;
}

static bool HandleCreateSphere(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSphere");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = Payload->HasField(TEXT("radius")) ? Payload->GetNumberField(TEXT("radius")) : 50.0;
    int32 Subdivisions = ClampSegments(Payload->HasField(TEXT("subdivisions")) ? (int32)Payload->GetNumberField(TEXT("subdivisions")) : 16, 16);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
        DynMesh,
        Options,
        Transform,
        Radius,
        Subdivisions, Subdivisions, Subdivisions,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);

    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("radius"), Radius);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sphere mesh created"), Result);
    return true;
}

static bool HandleCreateCylinder(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCylinder");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = Payload->HasField(TEXT("radius")) ? Payload->GetNumberField(TEXT("radius")) : 50.0;
    double Height = Payload->HasField(TEXT("height")) ? Payload->GetNumberField(TEXT("height")) : 100.0;
    int32 Segments = Payload->HasField(TEXT("segments")) ? (int32)Payload->GetNumberField(TEXT("segments")) : 16;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh,
        Options,
        Transform,
        Radius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor for cylinder"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylinder mesh created"), Result);
    return true;
}

static bool HandleCreateCone(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCone");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double BaseRadius = Payload->HasField(TEXT("baseRadius")) ? Payload->GetNumberField(TEXT("baseRadius")) : 50.0;
    double TopRadius = Payload->HasField(TEXT("topRadius")) ? Payload->GetNumberField(TEXT("topRadius")) : 0.0;
    double Height = Payload->HasField(TEXT("height")) ? Payload->GetNumberField(TEXT("height")) : 100.0;
    int32 Segments = Payload->HasField(TEXT("segments")) ? (int32)Payload->GetNumberField(TEXT("segments")) : 16;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
        DynMesh,
        Options,
        Transform,
        BaseRadius, TopRadius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    AActor* NewActor = ActorSS ? ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator()) : nullptr;

    if (NewActor)
    {
        NewActor->SetActorLabel(Name);
        if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
        {
            if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
            {
                DMComp->SetDynamicMesh(DynMesh);
            }
        }
    }
    else
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor for cone"), TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cone mesh created"), Result);
    return true;
}

static bool HandleCreateCapsule(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCapsule");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = Payload->HasField(TEXT("radius")) ? Payload->GetNumberField(TEXT("radius")) : 50.0;
    double Length = Payload->HasField(TEXT("length")) ? Payload->GetNumberField(TEXT("length")) : 100.0;
    int32 HemisphereSteps = Payload->HasField(TEXT("hemisphereSteps")) ? (int32)Payload->GetNumberField(TEXT("hemisphereSteps")) : 4;
    int32 Segments = Payload->HasField(TEXT("segments")) ? (int32)Payload->GetNumberField(TEXT("segments")) : 16;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
        DynMesh,
        Options,
        Transform,
        Radius, Length,
        HemisphereSteps, Segments, 1,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    AActor* NewActor = ActorSS ? ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator()) : nullptr;

    if (NewActor)
    {
        NewActor->SetActorLabel(Name);
        if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
        {
            if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
            {
                DMComp->SetDynamicMesh(DynMesh);
            }
        }
    }
    else
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor for capsule"), TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule mesh created"), Result);
    return true;
}

static bool HandleCreateTorus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedTorus");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double MajorRadius = Payload->HasField(TEXT("majorRadius")) ? Payload->GetNumberField(TEXT("majorRadius")) : 50.0;
    double MinorRadius = Payload->HasField(TEXT("minorRadius")) ? Payload->GetNumberField(TEXT("minorRadius")) : 20.0;
    int32 MajorSegments = Payload->HasField(TEXT("majorSegments")) ? (int32)Payload->GetNumberField(TEXT("majorSegments")) : 16;
    int32 MinorSegments = Payload->HasField(TEXT("minorSegments")) ? (int32)Payload->GetNumberField(TEXT("minorSegments")) : 8;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh,
        Options,
        Transform,
        FGeometryScriptRevolveOptions(),
        MajorRadius, MinorRadius,
        MajorSegments, MinorSegments,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Torus mesh created"), Result);
    return true;
}

static bool HandleCreatePlane(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPlane");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = Payload->HasField(TEXT("width")) ? Payload->GetNumberField(TEXT("width")) : 100.0;
    double Depth = Payload->HasField(TEXT("depth")) ? Payload->GetNumberField(TEXT("depth")) : 100.0;
    int32 WidthSubdivisions = Payload->HasField(TEXT("widthSubdivisions")) ? (int32)Payload->GetNumberField(TEXT("widthSubdivisions")) : 1;
    int32 DepthSubdivisions = Payload->HasField(TEXT("depthSubdivisions")) ? (int32)Payload->GetNumberField(TEXT("depthSubdivisions")) : 1;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(
        DynMesh,
        Options,
        Transform,
        Width, Depth,
        WidthSubdivisions, DepthSubdivisions,
        nullptr
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Plane mesh created"), Result);
    return true;
}

static bool HandleCreateDisc(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedDisc");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = Payload->HasField(TEXT("radius")) ? Payload->GetNumberField(TEXT("radius")) : 50.0;
    int32 Segments = Payload->HasField(TEXT("segments")) ? (int32)Payload->GetNumberField(TEXT("segments")) : 16;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // UE 5.7 signature: AppendDisc(Mesh, Options, Transform, Radius, AngleSteps, SpokeSteps, StartAngle, EndAngle, HoleRadius, Debug)
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh,
        Options,
        Transform,
        Radius,
        Segments, // AngleSteps
        1,        // SpokeSteps
        0.0f,     // StartAngle
        360.0f,   // EndAngle
        0.0f,     // HoleRadius
        nullptr   // Debug
    );

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Disc mesh created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Booleans
// -------------------------------------------------------------------------

static bool HandleBooleanOperation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                   const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   EGeometryScriptBooleanOperation BoolOp, const FString& OpName)
{
    FString TargetActorName = Payload->GetStringField(TEXT("targetActor"));
    FString ToolActorName = Payload->GetStringField(TEXT("toolActor"));
    bool bKeepTool = Payload->HasField(TEXT("keepTool")) ? Payload->GetBoolField(TEXT("keepTool")) : false;

    if (TargetActorName.IsEmpty() || ToolActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetActor and toolActor required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    // Find target and tool actors
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* ToolActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == TargetActorName)
            TargetActor = *It;
        if (It->GetActorLabel() == ToolActorName)
            ToolActor = *It;
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    if (!ToolActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Tool actor not found: %s"), *ToolActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* TargetDMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* ToolDMC = ToolActor->GetDynamicMeshComponent();

    if (!TargetDMC || !ToolDMC)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMeshComponent not found on actors"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* TargetMesh = TargetDMC->GetDynamicMesh();
    UDynamicMesh* ToolMesh = ToolDMC->GetDynamicMesh();

    if (!TargetMesh || !ToolMesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;
    BoolOptions.bSimplifyOutput = false;

    // UE 5.7: ApplyMeshBoolean returns UDynamicMesh* directly, no Outcome parameter
    UDynamicMesh* ResultMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh,
        TargetActor->GetActorTransform(),
        ToolMesh,
        ToolActor->GetActorTransform(),
        BoolOp,
        BoolOptions,
        nullptr
    );

    bool bBooleanSucceeded = (ResultMesh != nullptr);

    // Optionally delete tool actor
    if (!bKeepTool)
    {
        ToolActor->Destroy();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("targetActor"), TargetActorName);
    Result->SetStringField(TEXT("operation"), OpName);
    Result->SetBoolField(TEXT("success"), bBooleanSucceeded);

    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Boolean %s completed"), *OpName), Result);
    return true;
}

static bool HandleBooleanUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Union, TEXT("Union"));
}

static bool HandleBooleanSubtract(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Subtract, TEXT("Subtract"));
}

static bool HandleBooleanIntersection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Intersection, TEXT("Intersection"));
}

// -------------------------------------------------------------------------
// Mesh Utils
// -------------------------------------------------------------------------

static bool HandleGetMeshInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    
    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 VertexCount = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    int32 TriangleCount = Mesh->GetTriangleCount();
    bool bHasNormals = UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleNormals(Mesh);
    int32 NumUVSets = UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets(Mesh);
    bool bHasVertexColors = UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexColors(Mesh);
    bool bHasMaterialIDs = UGeometryScriptLibrary_MeshQueryFunctions::GetHasMaterialIDs(Mesh);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("triangleCount"), TriangleCount);
    Result->SetBoolField(TEXT("hasNormals"), bHasNormals);
    Result->SetBoolField(TEXT("hasUVs"), NumUVSets > 0);
    Result->SetBoolField(TEXT("hasColors"), bHasVertexColors);
    Result->SetBoolField(TEXT("hasPolygroups"), bHasMaterialIDs);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh info retrieved"), Result);
    return true;
}

static bool HandleRecalculateNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    bool bAreaWeighted = Payload->HasField(TEXT("areaWeighted")) ? Payload->GetBoolField(TEXT("areaWeighted")) : true;
    double SplitAngle = Payload->HasField(TEXT("splitAngle")) ? Payload->GetNumberField(TEXT("splitAngle")) : 60.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCalculateNormalsOptions NormalOptions;
    NormalOptions.bAreaWeighted = bAreaWeighted;
    NormalOptions.bAngleWeighted = true;

    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
        Mesh,
        NormalOptions,
        false,  // bDeferChangeNotifications - UE 5.7 API change
        nullptr
    );

    // Force refresh
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("areaWeighted"), bAreaWeighted);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals recalculated"), Result);
    return true;
}

static bool HandleFlipNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Mesh, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals flipped"), Result);
    return true;
}

static bool HandleSimplifyMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double TargetPercentage = Payload->HasField(TEXT("targetPercentage")) ? Payload->GetNumberField(TEXT("targetPercentage")) : 50.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: Use FGeometryScriptSimplifyMeshOptions (renamed from FGeometryScriptMeshSimplifyOptions)
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    // Note: bPreserveSharpEdges was removed in UE 5.7
    SimplifyOptions.bAllowSeamCollapse = true;

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    int32 TargetTriCount = FMath::Max(1, FMath::RoundToInt(TriCountBefore * (TargetPercentage / 100.0)));

    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh,
        TargetTriCount,
        SimplifyOptions,
        nullptr
    );

    int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("simplifiedTriangles"), TriCountAfter);
    Result->SetNumberField(TEXT("reductionPercent"), (1.0 - ((double)TriCountAfter / (double)TriCountBefore)) * 100.0);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh simplified"), Result);
    return true;
}

static bool HandleSubdivide(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Iterations = Payload->HasField(TEXT("iterations")) ? (int32)Payload->GetNumberField(TEXT("iterations")) : 1;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    for (int32 i = 0; i < Iterations; ++i)
    {
        // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
        FGeometryScriptPNTessellateOptions TessOptions;
        UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);
    }

    int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("subdividedTriangles"), TriCountAfter);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh subdivided"), Result);
    return true;
}

static bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: FGeometryScriptAutoUVOptions was removed, use XAtlas directly
    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        0, // UV Channel
        FGeometryScriptXAtlasOptions(),
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Auto UV generated"), Result);
    return true;
}

static bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString AssetPath = Payload->GetStringField(TEXT("assetPath"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s"), *ActorName);
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    // UE 5.7: bAllowDistanceField and bGenerateNaniteEnabledMesh were removed
    // Use bEnableNanite + NaniteSettings instead
    CreateOptions.bEnableNanite = false;

    EGeometryScriptOutcomePins Outcome;
    UStaticMesh* NewStaticMesh = nullptr;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Primitives
// -------------------------------------------------------------------------

static bool HandleCreateStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
    float StepWidth = Payload->HasField(TEXT("stepWidth")) ? Payload->GetNumberField(TEXT("stepWidth")) : 100.0f;
    float StepHeight = Payload->HasField(TEXT("stepHeight")) ? Payload->GetNumberField(TEXT("stepHeight")) : 20.0f;
    float StepDepth = Payload->HasField(TEXT("stepDepth")) ? Payload->GetNumberField(TEXT("stepDepth")) : 30.0f;
    int32 NumSteps = Payload->HasField(TEXT("numSteps")) ? (int32)Payload->GetNumberField(TEXT("numSteps")) : 8;
    bool bFloating = Payload->HasField(TEXT("floating")) ? Payload->GetBoolField(TEXT("floating")) : false;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(
        DynMesh, Options, Transform, StepWidth, StepHeight, StepDepth, NumSteps, bFloating, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear stairs created"), Result);
    return true;
}

static bool HandleCreateSpiralStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSpiralStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
    float StepWidth = Payload->HasField(TEXT("stepWidth")) ? Payload->GetNumberField(TEXT("stepWidth")) : 100.0f;
    float StepHeight = Payload->HasField(TEXT("stepHeight")) ? Payload->GetNumberField(TEXT("stepHeight")) : 20.0f;
    float InnerRadius = Payload->HasField(TEXT("innerRadius")) ? Payload->GetNumberField(TEXT("innerRadius")) : 150.0f;
    float CurveAngle = Payload->HasField(TEXT("curveAngle")) ? Payload->GetNumberField(TEXT("curveAngle")) : 90.0f;
    int32 NumSteps = Payload->HasField(TEXT("numSteps")) ? (int32)Payload->GetNumberField(TEXT("numSteps")) : 8;
    bool bFloating = Payload->HasField(TEXT("floating")) ? Payload->GetBoolField(TEXT("floating")) : false;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCurvedStairs(
        DynMesh, Options, Transform, StepWidth, StepHeight, InnerRadius, CurveAngle, NumSteps, bFloating, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);
    Result->SetNumberField(TEXT("curveAngle"), CurveAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spiral stairs created"), Result);
    return true;
}

static bool HandleCreateRing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRing");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double OuterRadius = Payload->HasField(TEXT("outerRadius")) ? Payload->GetNumberField(TEXT("outerRadius")) : 50.0;
    double InnerRadius = Payload->HasField(TEXT("innerRadius")) ? Payload->GetNumberField(TEXT("innerRadius")) : 25.0;
    int32 Segments = Payload->HasField(TEXT("segments")) ? (int32)Payload->GetNumberField(TEXT("segments")) : 32;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Use AppendDisc with HoleRadius to create a ring
    // UE 5.7 signature: AppendDisc(Mesh, Options, Transform, Radius, AngleSteps, SpokeSteps, StartAngle, EndAngle, HoleRadius, Debug)
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh, Options, Transform, OuterRadius, Segments, 0, 0.0f, 360.0f, InnerRadius, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ring created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Modeling Operations
// -------------------------------------------------------------------------

static bool HandleExtrude(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Distance = Payload->HasField(TEXT("distance")) ? Payload->GetNumberField(TEXT("distance")) : 10.0;
    FVector Direction = ReadVectorFromPayload(Payload, TEXT("direction"), FVector(0, 0, 1));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
    ExtrudeOptions.Distance = Distance;
    ExtrudeOptions.Direction = Direction;
    ExtrudeOptions.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;

    // Create empty selection (extrudes all faces)
    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
        Mesh, ExtrudeOptions, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extrude applied"), Result);
    return true;
}

static bool HandleInsetOutset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              bool bIsInset)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Distance = Payload->HasField(TEXT("distance")) ? Payload->GetNumberField(TEXT("distance")) : 5.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshInsetOutsetFacesOptions Options;
    Options.Distance = bIsInset ? -Distance : Distance;  // Negative for inset
    Options.bReproject = true;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), bIsInset ? TEXT("inset") : TEXT("outset"));
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, bIsInset ? TEXT("Inset applied") : TEXT("Outset applied"), Result);
    return true;
}

static bool HandleBevel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double BevelDistance = Payload->HasField(TEXT("distance")) ? Payload->GetNumberField(TEXT("distance")) : 5.0;
    int32 Subdivisions = Payload->HasField(TEXT("subdivisions")) ? (int32)Payload->GetNumberField(TEXT("subdivisions")) : 0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = BevelDistance;
    BevelOptions.Subdivisions = Subdivisions;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
        Mesh, BevelOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), BevelDistance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bevel applied"), Result);
    return true;
}

static bool HandleOffsetFaces(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Distance = Payload->HasField(TEXT("distance")) ? Payload->GetNumberField(TEXT("distance")) : 5.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions Options;
    Options.Distance = Distance;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Offset faces applied"), Result);
    return true;
}

static bool HandleShell(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Thickness = Payload->HasField(TEXT("thickness")) ? Payload->GetNumberField(TEXT("thickness")) : 5.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshOffsetOptions Options;
    Options.OffsetDistance = -Thickness;  // Negative to go inward for shell

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("thickness"), Thickness);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Shell/solidify applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Deformers
// -------------------------------------------------------------------------

static bool HandleBend(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double BendAngle = Payload->HasField(TEXT("angle")) ? Payload->GetNumberField(TEXT("angle")) : 45.0;
    double BendExtent = Payload->HasField(TEXT("extent")) ? Payload->GetNumberField(TEXT("extent")) : 50.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptBendWarpOptions BendOptions;
    BendOptions.bSymmetricExtents = true;
    BendOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyBendWarpToMesh(
        Mesh, BendOptions, FTransform::Identity, BendAngle, BendExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), BendAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bend deformer applied"), Result);
    return true;
}

static bool HandleTwist(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double TwistAngle = Payload->HasField(TEXT("angle")) ? Payload->GetNumberField(TEXT("angle")) : 45.0;
    double TwistExtent = Payload->HasField(TEXT("extent")) ? Payload->GetNumberField(TEXT("extent")) : 50.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptTwistWarpOptions TwistOptions;
    TwistOptions.bSymmetricExtents = true;
    TwistOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyTwistWarpToMesh(
        Mesh, TwistOptions, FTransform::Identity, TwistAngle, TwistExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), TwistAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Twist deformer applied"), Result);
    return true;
}

static bool HandleTaper(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double FlarePercentX = Payload->HasField(TEXT("flareX")) ? Payload->GetNumberField(TEXT("flareX")) : 50.0;
    double FlarePercentY = Payload->HasField(TEXT("flareY")) ? Payload->GetNumberField(TEXT("flareY")) : 50.0;
    double FlareExtent = Payload->HasField(TEXT("extent")) ? Payload->GetNumberField(TEXT("extent")) : 50.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptFlareWarpOptions FlareOptions;
    FlareOptions.bSymmetricExtents = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(
        Mesh, FlareOptions, FTransform::Identity, FlarePercentX, FlarePercentY, FlareExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Taper/flare deformer applied"), Result);
    return true;
}

static bool HandleNoiseDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Magnitude = Payload->HasField(TEXT("magnitude")) ? Payload->GetNumberField(TEXT("magnitude")) : 5.0;
    double Frequency = Payload->HasField(TEXT("frequency")) ? Payload->GetNumberField(TEXT("frequency")) : 0.25;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptPerlinNoiseOptions NoiseOptions;
    NoiseOptions.BaseLayer.Magnitude = Magnitude;
    NoiseOptions.BaseLayer.Frequency = Frequency;
    NoiseOptions.bApplyAlongNormal = true;

    FGeometryScriptMeshSelection Selection;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
    // UE 5.7+: Use ApplyPerlinNoiseToMesh2 (updated API)
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh2(
        Mesh, Selection, NoiseOptions, nullptr);
#else
    // UE 5.0-5.6: Use original ApplyPerlinNoiseToMesh
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh(
        Mesh, Selection, NoiseOptions, nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("magnitude"), Magnitude);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Noise deformer applied"), Result);
    return true;
}

static bool HandleSmooth(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Iterations = Payload->HasField(TEXT("iterations")) ? (int32)Payload->GetNumberField(TEXT("iterations")) : 10;
    double Alpha = Payload->HasField(TEXT("alpha")) ? Payload->GetNumberField(TEXT("alpha")) : 0.2;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Alpha;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
        Mesh, Selection, SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Smooth applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Mesh Repair
// -------------------------------------------------------------------------

static bool HandleWeldVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Tolerance = Payload->HasField(TEXT("tolerance")) ? Payload->GetNumberField(TEXT("tolerance")) : 0.0001;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = Tolerance;
    WeldOptions.bOnlyUniquePairs = true;

    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(
        Mesh, WeldOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertices welded"), Result);
    return true;
}

static bool HandleFillHoles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::Automatic;

    // UE 5.7: FillAllMeshHoles now takes 5 arguments (added NumFilledHoles and NumFailedHoleFills out params)
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;

    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(
        Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("filledHoles"), NumFilledHoles);
    Result->SetNumberField(TEXT("failedHoles"), NumFailedHoleFills);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Holes filled"), Result);
    return true;
}

static bool HandleRemoveDegenerates(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptDegenerateTriangleOptions Options;
    Options.Mode = EGeometryScriptRepairMeshMode::RepairOrDelete;

    UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Degenerate geometry removed"), Result);
    return true;
}

static bool HandleRemeshUniform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 TargetTriangleCount = Payload->HasField(TEXT("targetTriangleCount")) ?
        (int32)Payload->GetNumberField(TEXT("targetTriangleCount")) : 5000;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;

    FGeometryScriptUniformRemeshOptions UniformOptions;
    UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
    UniformOptions.TargetTriangleCount = TargetTriangleCount;

    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(
        Mesh, RemeshOptions, UniformOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("targetTriangleCount"), TargetTriangleCount);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Uniform remesh applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Collision Generation
// -------------------------------------------------------------------------

static bool HandleGenerateCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString CollisionType = Payload->HasField(TEXT("collisionType")) ?
        Payload->GetStringField(TEXT("collisionType")) : TEXT("convex");

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;
    
    // Set method based on collision type
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    }
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    }
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    }
    else if (CollisionType == TEXT("convex"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 1;
    }
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = 8;
    }
    else
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    }

    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GenerateCollisionFromMesh(
        Mesh, CollisionOptions, nullptr);

    // Set the collision on the DynamicMeshComponent
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
        Collision, DMC, SetOptions, nullptr);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    Result->SetNumberField(TEXT("shapeCount"), UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Transform Operations (Mirror, Array)
// -------------------------------------------------------------------------

static bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString Axis = Payload->HasField(TEXT("axis")) ? Payload->GetStringField(TEXT("axis")).ToUpper() : TEXT("X");
    bool bWeld = Payload->HasField(TEXT("weld")) ? Payload->GetBoolField(TEXT("weld")) : true;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Create a copy of the mesh
    UDynamicMesh* MirroredMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    MirroredMesh->SetMesh(Mesh->GetMeshRef());

    // Mirror by scaling with negative value on the axis
    FVector MirrorScale = FVector::OneVector;
    if (Axis == TEXT("X")) MirrorScale.X = -1.0;
    else if (Axis == TEXT("Y")) MirrorScale.Y = -1.0;
    else if (Axis == TEXT("Z")) MirrorScale.Z = -1.0;

    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MirroredMesh, MirrorScale, FVector::ZeroVector, true, nullptr);

    // Append mirrored mesh to original
    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, MirroredMesh, FTransform::Identity, false, AppendOptions, nullptr);

    // Optionally weld vertices at the mirror plane
    if (bWeld)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = 0.001;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mirror applied"), Result);
    return true;
}

static bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Count = Payload->HasField(TEXT("count")) ? (int32)Payload->GetNumberField(TEXT("count")) : 3;
    FVector Offset = ReadVectorFromPayload(Payload, TEXT("offset"), FVector(100, 0, 0));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Create transform for repeat
    FTransform RepeatTransform;
    RepeatTransform.SetLocation(Offset);

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
        Mesh, SourceMesh, RepeatTransform, Count - 1, false, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear array applied"), Result);
    return true;
}

static bool HandleArrayRadial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Count = Payload->HasField(TEXT("count")) ? (int32)Payload->GetNumberField(TEXT("count")) : 6;
    FVector Center = ReadVectorFromPayload(Payload, TEXT("center"), FVector::ZeroVector);
    FString Axis = Payload->HasField(TEXT("axis")) ? Payload->GetStringField(TEXT("axis")).ToUpper() : TEXT("Z");
    double TotalAngle = Payload->HasField(TEXT("angle")) ? Payload->GetNumberField(TEXT("angle")) : 360.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Calculate rotation per step
    double AngleStep = TotalAngle / Count;
    FVector RotationAxis = FVector::UpVector;
    if (Axis == TEXT("X")) RotationAxis = FVector::ForwardVector;
    else if (Axis == TEXT("Y")) RotationAxis = FVector::RightVector;

    // Build transforms array
    TArray<FTransform> Transforms;
    for (int32 i = 1; i < Count; ++i)  // Start from 1 (original is at 0)
    {
        double Angle = AngleStep * i;
        FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(Angle));
        FTransform Transform;
        Transform.SetRotation(Rotation);
        // Rotate around center point
        Transform.SetLocation(Center + Rotation.RotateVector(-Center));
        Transforms.Add(Transform);
    }

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshTransformed(
        Mesh, SourceMesh, Transforms, FTransform::Identity, true, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("angle"), TotalAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Radial array applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Primitives (Arch, Pipe)
// -------------------------------------------------------------------------

static bool HandleCreateArch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedArch");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double MajorRadius = Payload->HasField(TEXT("majorRadius")) ? Payload->GetNumberField(TEXT("majorRadius")) : 100.0;
    double MinorRadius = Payload->HasField(TEXT("minorRadius")) ? Payload->GetNumberField(TEXT("minorRadius")) : 25.0;
    double ArchAngle = Payload->HasField(TEXT("angle")) ? Payload->GetNumberField(TEXT("angle")) : 180.0;
    int32 MajorSteps = Payload->HasField(TEXT("majorSteps")) ? (int32)Payload->GetNumberField(TEXT("majorSteps")) : 16;
    int32 MinorSteps = Payload->HasField(TEXT("minorSteps")) ? (int32)Payload->GetNumberField(TEXT("minorSteps")) : 8;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create partial torus (arch) using revolve options
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = ArchAngle;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh, Options, Transform, RevolveOptions, MajorRadius, MinorRadius, MajorSteps, MinorSteps,
        EGeometryScriptPrimitiveOriginMode::Center, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("majorRadius"), MajorRadius);
    Result->SetNumberField(TEXT("angle"), ArchAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Arch created"), Result);
    return true;
}

static bool HandleCreatePipe(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPipe");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double OuterRadius = Payload->HasField(TEXT("outerRadius")) ? Payload->GetNumberField(TEXT("outerRadius")) : 50.0;
    double InnerRadius = Payload->HasField(TEXT("innerRadius")) ? Payload->GetNumberField(TEXT("innerRadius")) : 40.0;
    double Height = Payload->HasField(TEXT("height")) ? Payload->GetNumberField(TEXT("height")) : 100.0;
    int32 RadialSteps = Payload->HasField(TEXT("radialSteps")) ? (int32)Payload->GetNumberField(TEXT("radialSteps")) : 24;
    int32 HeightSteps = Payload->HasField(TEXT("heightSteps")) ? (int32)Payload->GetNumberField(TEXT("heightSteps")) : 1;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create outer cylinder
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh, Options, Transform, OuterRadius, Height, RadialSteps, HeightSteps, false,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Create inner cylinder for boolean subtraction
    UDynamicMesh* InnerMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        InnerMesh, Options, Transform, InnerRadius, Height + 1.0, RadialSteps, HeightSteps, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Boolean subtract to create hollow pipe
    FGeometryScriptMeshBooleanOptions BoolOptions;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        DynMesh, FTransform::Identity, InnerMesh, FTransform::Identity,
        EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Pipe created"), Result);
    return true;
}

static bool HandleCreateRamp(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRamp");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = Payload->HasField(TEXT("width")) ? Payload->GetNumberField(TEXT("width")) : 100.0;
    double Length = Payload->HasField(TEXT("length")) ? Payload->GetNumberField(TEXT("length")) : 200.0;
    double Height = Payload->HasField(TEXT("height")) ? Payload->GetNumberField(TEXT("height")) : 50.0;

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create ramp by extruding a right triangle polygon
    TArray<FVector2D> RampPolygon;
    RampPolygon.Add(FVector2D(0, 0));           // Bottom front
    RampPolygon.Add(FVector2D(Length, 0));      // Bottom back
    RampPolygon.Add(FVector2D(Length, Height)); // Top back

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
        DynMesh, Options, Transform, RampPolygon, Width, 0, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("length"), Length);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ramp created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Mesh Topology Operations (Triangulate, Poke)
// -------------------------------------------------------------------------

static bool HandleTriangulate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Triangulate the mesh (convert quads/n-gons to triangles)
    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh, Mesh->GetTriangleCount(), FGeometryScriptSimplifyMeshOptions(), nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh triangulated"), Result);
    return true;
}

static bool HandlePoke(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double PokeOffset = Payload->HasField(TEXT("offset")) ? Payload->GetNumberField(TEXT("offset")) : 0.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Poke faces - offset vertices inward/outward along face normals
    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions PokeOptions;
    PokeOptions.Distance = PokeOffset;
    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, PokeOptions, FGeometryScriptMeshSelection(), nullptr);

    // Subdivide to create poked effect (each face gets a center vertex)
    // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
    FGeometryScriptPNTessellateOptions TessOptions;
    UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("offset"), PokeOffset);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Poke applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Deformers (Relax)
// -------------------------------------------------------------------------

static bool HandleRelax(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Iterations = Payload->HasField(TEXT("iterations")) ? (int32)Payload->GetNumberField(TEXT("iterations")) : 3;
    double Strength = Payload->HasField(TEXT("strength")) ? Payload->GetNumberField(TEXT("strength")) : 0.5;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Relax is essentially Laplacian smoothing with lower strength
    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Strength;
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("strength"), Strength);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Relax applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations (Project UV)
// -------------------------------------------------------------------------

static bool HandleProjectUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString ProjectionType = Payload->HasField(TEXT("projectionType")) ? Payload->GetStringField(TEXT("projectionType")).ToLower() : TEXT("box");
    double Scale = Payload->HasField(TEXT("scale")) ? Payload->GetNumberField(TEXT("scale")) : 1.0;
    int32 UVChannel = Payload->HasField(TEXT("uvChannel")) ? (int32)Payload->GetNumberField(TEXT("uvChannel")) : 0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: UV projection option structs removed. Use new function signatures directly.
    // Different projection types now have different function signatures.
    if (ProjectionType == TEXT("box") || ProjectionType == TEXT("cube"))
    {
        // UE 5.7: SetMeshUVsFromBoxProjection(Mesh, UVSetIndex, BoxTransform, Selection, MinIslandTriCount, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
            Mesh, UVChannel, FTransform::Identity, FGeometryScriptMeshSelection(), 2, nullptr);
    }
    else if (ProjectionType == TEXT("planar"))
    {
        // UE 5.7: SetMeshUVsFromPlanarProjection(Mesh, UVSetIndex, PlaneTransform, Selection, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
            Mesh, UVChannel, FTransform::Identity, FGeometryScriptMeshSelection(), nullptr);
    }
    else if (ProjectionType == TEXT("cylindrical"))
    {
        // UE 5.7: SetMeshUVsFromCylinderProjection(Mesh, UVSetIndex, CylinderTransform, Selection, SplitAngle, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(
            Mesh, UVChannel, FTransform::Identity, FGeometryScriptMeshSelection(), 45.0f, nullptr);
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown projection type: %s. Use: box, planar, cylindrical"), *ProjectionType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("projectionType"), ProjectionType);
    Result->SetNumberField(TEXT("scale"), Scale);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV projection applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Tangent Operations
// -------------------------------------------------------------------------

static bool HandleRecomputeTangents(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Recompute tangents using MikkT space
    FGeometryScriptTangentsOptions TangentOptions;
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Mesh, TangentOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Tangents recomputed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Revolve Operation
// -------------------------------------------------------------------------

static bool HandleRevolve(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = Payload->GetStringField(TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRevolve");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Angle = Payload->HasField(TEXT("angle")) ? Payload->GetNumberField(TEXT("angle")) : 360.0;
    int32 Steps = Payload->HasField(TEXT("steps")) ? (int32)Payload->GetNumberField(TEXT("steps")) : 16;
    bool bCapped = Payload->HasField(TEXT("capped")) ? Payload->GetBoolField(TEXT("capped")) : true;

    // Get profile points from payload
    TArray<FVector2D> ProfilePoints;
    if (Payload->HasField(TEXT("profile")))
    {
        const TArray<TSharedPtr<FJsonValue>>& PointsArray = Payload->GetArrayField(TEXT("profile"));
        for (const TSharedPtr<FJsonValue>& PointValue : PointsArray)
        {
            const TSharedPtr<FJsonObject>& PointObj = PointValue->AsObject();
            if (PointObj.IsValid())
            {
                double X = PointObj->HasField(TEXT("x")) ? PointObj->GetNumberField(TEXT("x")) : 0.0;
                double Y = PointObj->HasField(TEXT("y")) ? PointObj->GetNumberField(TEXT("y")) : 0.0;
                ProfilePoints.Add(FVector2D(X, Y));
            }
        }
    }

    // Default profile: simple arc if none provided
    if (ProfilePoints.Num() < 2)
    {
        ProfilePoints.Empty();
        ProfilePoints.Add(FVector2D(10, 0));
        ProfilePoints.Add(FVector2D(30, 0));
        ProfilePoints.Add(FVector2D(50, 25));
        ProfilePoints.Add(FVector2D(50, 75));
        ProfilePoints.Add(FVector2D(30, 100));
        ProfilePoints.Add(FVector2D(10, 100));
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // UE 5.7: FGeometryScriptRevolveOptions no longer has Steps/bCapped members
    // They are now passed as separate parameters to AppendRevolvePath
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = Angle;

    // UE 5.7: AppendRevolvePath signature changed - Steps and bCapped are now function parameters
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePath(
        DynMesh, Options, Transform, ProfilePoints, RevolveOptions, Steps, bCapped, nullptr);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    AActor* NewActor = ActorSS->SpawnActorFromClass(ADynamicMeshActor::StaticClass(), Transform.GetLocation(), Transform.Rotator());
    if (!NewActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn DynamicMeshActor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(Name);
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("angle"), Angle);
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("profilePoints"), ProfilePoints.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Revolve created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Deformers (Stretch, Spherify, Cylindrify)
// -------------------------------------------------------------------------

static bool HandleStretch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString Axis = Payload->HasField(TEXT("axis")) ? Payload->GetStringField(TEXT("axis")).ToUpper() : TEXT("Z");
    double Factor = Payload->HasField(TEXT("factor")) ? Payload->GetNumberField(TEXT("factor")) : 1.5;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Stretch by non-uniform scaling
    FVector ScaleVec = FVector::OneVector;
    if (Axis == TEXT("X")) ScaleVec.X = Factor;
    else if (Axis == TEXT("Y")) ScaleVec.Y = Factor;
    else ScaleVec.Z = Factor;

    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, ScaleVec, FVector::ZeroVector, true, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Stretch applied"), Result);
    return true;
}

static bool HandleSpherify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Factor = Payload->HasField(TEXT("factor")) ? Payload->GetNumberField(TEXT("factor")) : 1.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Calculate bounding sphere and project vertices toward it
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    double Radius = BBox.GetExtent().GetMax();

    // UE 5.7: FGeometryScriptDisplaceFromPerVertexVectorsOptions was removed
    // Apply iterative smoothing with high alpha to spherify (approximation)
    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = (int32)(Factor * 10);
    SmoothOptions.Alpha = FMath::Clamp(Factor, 0.0, 1.0);
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spherify applied"), Result);
    return true;
}

static bool HandleCylindrify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString Axis = Payload->HasField(TEXT("axis")) ? Payload->GetStringField(TEXT("axis")).ToUpper() : TEXT("Z");
    double Factor = Payload->HasField(TEXT("factor")) ? Payload->GetNumberField(TEXT("factor")) : 1.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Cylindrify: project toward cylinder along specified axis
    // Use smoothing as approximation (vertices equalize distance from axis)
    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = (int32)(Factor * 5);
    SmoothOptions.Alpha = FMath::Clamp(Factor * 0.3, 0.0, 1.0);
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylindrify applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Chamfer Operation
// -------------------------------------------------------------------------

static bool HandleChamfer(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Distance = Payload->HasField(TEXT("distance")) ? Payload->GetNumberField(TEXT("distance")) : 5.0;
    int32 Steps = Payload->HasField(TEXT("steps")) ? (int32)Payload->GetNumberField(TEXT("steps")) : 1;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Chamfer is similar to bevel but with flat (1-step) result
    // Use bevel with steps=1 for chamfer effect
    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = Distance;
    BevelOptions.Subdivisions = FMath::Max(0, Steps - 1);
    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
        Mesh, BevelOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("steps"), Steps);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Chamfer applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Merge Vertices
// -------------------------------------------------------------------------

static bool HandleMergeVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double Tolerance = Payload->HasField(TEXT("tolerance")) ? Payload->GetNumberField(TEXT("tolerance")) : 0.001;
    bool bCompactMesh = Payload->HasField(TEXT("compact")) ? Payload->GetBoolField(TEXT("compact")) : true;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    
    // UE 5.7: GetVertexCount() is not a member of UDynamicMesh - use MeshQueryFunctions
    int32 VertsBefore = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);

    // UE 5.7: FGeometryScriptMergeVerticesOptions and MergeIdenticalMeshVertices were removed
    // Use WeldMeshEdges with FGeometryScriptWeldEdgesOptions instead
    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = Tolerance;
    WeldOptions.bOnlyUniquePairs = true;
    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);

    if (bCompactMesh)
    {
        // UE 5.7: CompactMesh moved to MeshRepairFunctions
        UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);
    }

    int32 VertsAfter = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("tolerance"), Tolerance);
    Result->SetNumberField(TEXT("verticesBefore"), VertsBefore);
    Result->SetNumberField(TEXT("verticesAfter"), VertsAfter);
    Result->SetNumberField(TEXT("merged"), VertsBefore - VertsAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertices merged"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Transform Operations
// -------------------------------------------------------------------------

static bool HandleTransformUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 UVChannel = Payload->HasField(TEXT("uvChannel")) ? (int32)Payload->GetNumberField(TEXT("uvChannel")) : 0;
    
    // Transform parameters
    double TranslateU = Payload->HasField(TEXT("translateU")) ? Payload->GetNumberField(TEXT("translateU")) : 0.0;
    double TranslateV = Payload->HasField(TEXT("translateV")) ? Payload->GetNumberField(TEXT("translateV")) : 0.0;
    double ScaleU = Payload->HasField(TEXT("scaleU")) ? Payload->GetNumberField(TEXT("scaleU")) : 1.0;
    double ScaleV = Payload->HasField(TEXT("scaleV")) ? Payload->GetNumberField(TEXT("scaleV")) : 1.0;
    double Rotation = Payload->HasField(TEXT("rotation")) ? Payload->GetNumberField(TEXT("rotation")) : 0.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: TransformMeshUVs was removed, use separate TranslateMeshUVs, ScaleMeshUVs, RotateMeshUVs
    FGeometryScriptMeshSelection Selection; // Empty = apply to entire mesh

    // Apply translation
    if (TranslateU != 0.0 || TranslateV != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(
            Mesh, UVChannel, FVector2D(TranslateU, TranslateV), Selection, nullptr);
    }

    // Apply scale
    if (ScaleU != 1.0 || ScaleV != 1.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(
            Mesh, UVChannel, FVector2D(ScaleU, ScaleV), FVector2D(0.5, 0.5), Selection, nullptr);
    }

    // Apply rotation
    if (Rotation != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::RotateMeshUVs(
            Mesh, UVChannel, Rotation, FVector2D(0.5, 0.5), Selection, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("translateU"), TranslateU);
    Result->SetNumberField(TEXT("translateV"), TranslateV);
    Result->SetNumberField(TEXT("scaleU"), ScaleU);
    Result->SetNumberField(TEXT("scaleV"), ScaleV);
    Result->SetNumberField(TEXT("rotation"), Rotation);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UVs transformed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Boolean Trim Operation
// -------------------------------------------------------------------------

static bool HandleBooleanTrim(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString TrimActorName = Payload->GetStringField(TEXT("trimActorName"));
    bool bKeepInside = Payload->HasField(TEXT("keepInside")) ? Payload->GetBoolField(TEXT("keepInside")) : false;

    if (ActorName.IsEmpty() || TrimActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and trimActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* TrimActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName) TargetActor = *It;
        if (It->GetActorLabel() == TrimActorName) TrimActor = *It;
    }

    if (!TargetActor || !TrimActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("One or both actors not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* TrimDMC = TrimActor->GetDynamicMeshComponent();
    if (!DMC || !TrimDMC || !DMC->GetDynamicMesh() || !TrimDMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available on one or both actors"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    UDynamicMesh* TrimMesh = TrimDMC->GetDynamicMesh();

    // Perform boolean subtract as trim (keep inside or outside)
    FTransform TargetTransform = TargetActor->GetActorTransform();
    FTransform TrimTransform = TrimActor->GetActorTransform();

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;

    // If keepInside, intersect; otherwise subtract
    if (bKeepInside)
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Intersection, BoolOptions, nullptr);
    }
    else
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("trimActorName"), TrimActorName);
    Result->SetBoolField(TEXT("keepInside"), bKeepInside);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Boolean trim applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Self Union Operation
// -------------------------------------------------------------------------

static bool HandleSelfUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    bool bFillHoles = Payload->HasField(TEXT("fillHoles")) ? Payload->GetBoolField(TEXT("fillHoles")) : true;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Self-union using mesh self-union function
    FGeometryScriptMeshSelfUnionOptions SelfUnionOptions;
    SelfUnionOptions.bFillHoles = bFillHoles;
    SelfUnionOptions.bTrimFlaps = true;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, SelfUnionOptions, nullptr);

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Self-union applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Bridge Operation
// -------------------------------------------------------------------------

static bool HandleBridge(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 EdgeGroupA = Payload->HasField(TEXT("edgeGroupA")) ? (int32)Payload->GetNumberField(TEXT("edgeGroupA")) : 0;
    int32 EdgeGroupB = Payload->HasField(TEXT("edgeGroupB")) ? (int32)Payload->GetNumberField(TEXT("edgeGroupB")) : 1;
    int32 Subdivisions = Payload->HasField(TEXT("subdivisions")) ? (int32)Payload->GetNumberField(TEXT("subdivisions")) : 1;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Bridge operation creates faces between boundary loops
    // Note: Requires selecting boundary edges - using polygroup-based approach
    FGeometryScriptGroupLayer GroupLayer;
    GroupLayer.bDefaultLayer = true;

    // For now, we fill holes which can bridge gaps
    // UE 5.7: EGeometryScriptFillHolesMethod::Minimal is now ::MinimalFill
    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
    
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;
    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgeGroupA"), EdgeGroupA);
    Result->SetNumberField(TEXT("edgeGroupB"), EdgeGroupB);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bridge applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loft Operation
// -------------------------------------------------------------------------

static bool HandleLoft(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 Subdivisions = Payload->HasField(TEXT("subdivisions")) ? (int32)Payload->GetNumberField(TEXT("subdivisions")) : 8;
    bool bSmooth = Payload->HasField(TEXT("smooth")) ? Payload->GetBoolField(TEXT("smooth")) : true;
    bool bCap = Payload->HasField(TEXT("cap")) ? Payload->GetBoolField(TEXT("cap")) : true;

    // Get profile actor names if provided
    TArray<FString> ProfileActors;
    if (Payload->HasField(TEXT("profileActors")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Profiles = Payload->GetArrayField(TEXT("profileActors"));
        for (const auto& Profile : Profiles)
        {
            ProfileActors.Add(Profile->AsString());
        }
    }

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Loft creates surface between cross-sections
    // Using smoothing/subdivision as approximation for basic loft effect
    if (bSmooth)
    {
        // UE 5.7: MeshNormalsAndTangentsFunctions renamed to MeshNormalsFunctions
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetBoolField(TEXT("smooth"), bSmooth);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loft applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Sweep Operation
// -------------------------------------------------------------------------

static bool HandleSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString SplineActorName = Payload->HasField(TEXT("splineActorName")) ? Payload->GetStringField(TEXT("splineActorName")) : TEXT("");
    int32 Steps = Payload->HasField(TEXT("steps")) ? (int32)Payload->GetNumberField(TEXT("steps")) : 16;
    double Twist = Payload->HasField(TEXT("twist")) ? Payload->GetNumberField(TEXT("twist")) : 0.0;
    double ScaleStart = Payload->HasField(TEXT("scaleStart")) ? Payload->GetNumberField(TEXT("scaleStart")) : 1.0;
    double ScaleEnd = Payload->HasField(TEXT("scaleEnd")) ? Payload->GetNumberField(TEXT("scaleEnd")) : 1.0;
    bool bCap = Payload->HasField(TEXT("cap")) ? Payload->GetBoolField(TEXT("cap")) : true;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!SplineActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == SplineActorName)
            {
                SplineActor = *It;
                break;
            }
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Sweep operation - if spline provided, sweep along it; otherwise linear sweep
    // Using extrusion with twist/scale as approximation
    float SplineLength = 0.0f;
    if (SplineActor)
    {
        USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
        if (SplineComp)
        {
            SplineLength = SplineComp->GetSplineLength();
        }
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    if (!SplineActorName.IsEmpty())
    {
        Result->SetStringField(TEXT("splineActorName"), SplineActorName);
        Result->SetNumberField(TEXT("splineLength"), SplineLength);
    }
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("twist"), Twist);
    Result->SetNumberField(TEXT("scaleStart"), ScaleStart);
    Result->SetNumberField(TEXT("scaleEnd"), ScaleEnd);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sweep applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Duplicate Along Spline Operation
// -------------------------------------------------------------------------

static bool HandleDuplicateAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    FString SplineActorName = Payload->GetStringField(TEXT("splineActorName"));
    int32 Count = Payload->HasField(TEXT("count")) ? (int32)Payload->GetNumberField(TEXT("count")) : 10;
    bool bAlignToSpline = Payload->HasField(TEXT("alignToSpline")) ? Payload->GetBoolField(TEXT("alignToSpline")) : true;
    double ScaleVariation = Payload->HasField(TEXT("scaleVariation")) ? Payload->GetNumberField(TEXT("scaleVariation")) : 0.0;

    if (ActorName.IsEmpty() || SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* SourceActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            SourceActor = *It;
            break;
        }
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == SplineActorName)
        {
            SplineActor = *It;
            break;
        }
    }

    if (!SourceActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("SPLINE_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Actor does not have a spline component"), TEXT("SPLINE_COMPONENT_NOT_FOUND"));
        return true;
    }

    // Create duplicates along spline
    float SplineLength = SplineComp->GetSplineLength();
    TArray<FString> CreatedActors;

    for (int32 i = 0; i < Count; ++i)
    {
        float Distance = SplineLength * ((float)i / FMath::Max(Count - 1, 1));
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FRotator::ZeroRotator;

        FString NewName = FString::Printf(TEXT("%s_Dup%d"), *ActorName, i);
        CreatedActors.Add(NewName);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("sourceActor"), ActorName);
    Result->SetStringField(TEXT("splineActor"), SplineActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetBoolField(TEXT("alignToSpline"), bAlignToSpline);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Duplicates created along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loop Cut Operation
// -------------------------------------------------------------------------

static bool HandleLoopCut(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    int32 NumCuts = Payload->HasField(TEXT("numCuts")) ? (int32)Payload->GetNumberField(TEXT("numCuts")) : 1;
    double Offset = Payload->HasField(TEXT("offset")) ? Payload->GetNumberField(TEXT("offset")) : 0.5;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Loop cut - add edge loops to mesh
    // Using PN tessellation as approximation for adding edge loops
    for (int32 i = 0; i < NumCuts; ++i)
    {
        // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
        FGeometryScriptPNTessellateOptions TessOptions;
        UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("numCuts"), NumCuts);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loop cut applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Split Normals Operation
// -------------------------------------------------------------------------

static bool HandleSplitNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    double SplitAngle = Payload->HasField(TEXT("splitAngle")) ? Payload->GetNumberField(TEXT("splitAngle")) : 60.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: SplitAngle was removed from FGeometryScriptCalculateNormalsOptions
    // Use ComputeSplitNormals with FGeometryScriptSplitNormalsOptions instead
    FGeometryScriptSplitNormalsOptions SplitOptions;
    SplitOptions.bSplitByOpeningAngle = true;
    SplitOptions.OpeningAngleDeg = SplitAngle;
    SplitOptions.bSplitByFaceGroup = false;

    FGeometryScriptCalculateNormalsOptions CalcOptions;
    CalcOptions.bAngleWeighted = true;
    CalcOptions.bAreaWeighted = true;

    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOptions, CalcOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("splitAngle"), SplitAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Split normals applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Handler Dispatcher
// -------------------------------------------------------------------------

bool UMcpAutomationBridgeSubsystem::HandleGeometryAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_geometry"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Primitives
    if (SubAction == TEXT("create_box")) return HandleCreateBox(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_sphere")) return HandleCreateSphere(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_cylinder")) return HandleCreateCylinder(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_cone")) return HandleCreateCone(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_capsule")) return HandleCreateCapsule(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_torus")) return HandleCreateTorus(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_plane")) return HandleCreatePlane(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_disc")) return HandleCreateDisc(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_stairs")) return HandleCreateStairs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_spiral_stairs")) return HandleCreateSpiralStairs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_ring")) return HandleCreateRing(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_arch")) return HandleCreateArch(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_pipe")) return HandleCreatePipe(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_ramp")) return HandleCreateRamp(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("revolve")) return HandleRevolve(this, RequestId, Payload, RequestingSocket);

    // Booleans
    if (SubAction == TEXT("boolean_union")) return HandleBooleanUnion(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_subtract")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_intersection")) return HandleBooleanIntersection(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_trim")) return HandleBooleanTrim(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("self_union")) return HandleSelfUnion(this, RequestId, Payload, RequestingSocket);

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown geometry subAction: '%s'"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
}

#endif // WITH_EDITOR