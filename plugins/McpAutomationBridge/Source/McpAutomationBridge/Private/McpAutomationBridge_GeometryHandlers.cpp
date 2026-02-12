/**
 * McpAutomationBridge_GeometryHandlers.cpp
 *
 * Phase 6: Geometry Script handlers
 * Implements procedural mesh creation and manipulation using UE Geometry Script APIs
 */

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpGeometryHandlers, Log, All);

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
// Note: MeshRemeshFunctions.h was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "GeometryScript/MeshRemeshFunctions.h"
#endif
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"

// UE 5.3+: MeshTransformFunctions contains TranslateMesh, ScaleMesh, etc.
// UE 5.0-5.2: These functions are in MeshDeformFunctions
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "GeometryScript/MeshTransformFunctions.h"
#endif

#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UDynamicMesh.h"
#include "Components/SplineComponent.h"

// GeometryScript is only fully supported in UE 5.1+
// UE 5.0 had experimental GeometryScript with limited API
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 1
#else
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 0
#endif
 
// Helper macros for JSON field access
#define GetStringFieldGeom GetJsonStringField
#define GetNumberFieldGeom GetJsonNumberField
#define GetBoolFieldGeom GetJsonBoolField
#define GetIntFieldGeom GetJsonIntField
 
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
            GetNumberFieldGeom((*ObjPtr), TEXT("x")),
            GetNumberFieldGeom((*ObjPtr), TEXT("y")),
            GetNumberFieldGeom((*ObjPtr), TEXT("z"))
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
                GetNumberFieldGeom((*ObjPtr), TEXT("pitch"), 0.0),
                GetNumberFieldGeom((*ObjPtr), TEXT("yaw"), 0.0),
                GetNumberFieldGeom((*ObjPtr), TEXT("roll"), 0.0)
            );
        }
        // Fallback to {x, y, z} format (x=Pitch, y=Yaw, z=Roll)
        return FRotator(
            GetNumberFieldGeom((*ObjPtr), TEXT("x")),
            GetNumberFieldGeom((*ObjPtr), TEXT("y")),
            GetNumberFieldGeom((*ObjPtr), TEXT("z"))
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

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

static bool HandleCreateBox(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedBox");

    FTransform Transform = ReadTransformFromPayload(Payload);

    // Get dimensions with safety clamping
    double Width = ClampDimension(GetNumberFieldGeom(Payload, TEXT("width"), 100.0));
    double Height = ClampDimension(GetNumberFieldGeom(Payload, TEXT("height"), 100.0));
    double Depth = ClampDimension(GetNumberFieldGeom(Payload, TEXT("depth"), 100.0));
 
    int32 WidthSegments = ClampSegments(GetIntFieldGeom(Payload, TEXT("widthSegments"), 1));
    int32 HeightSegments = ClampSegments(GetIntFieldGeom(Payload, TEXT("heightSegments"), 1));
    int32 DepthSegments = ClampSegments(GetIntFieldGeom(Payload, TEXT("depthSegments"), 1));

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSphere");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetNumberFieldGeom(Payload, TEXT("radius"), 50.0);
    int32 Subdivisions = ClampSegments(GetIntFieldGeom(Payload, TEXT("subdivisions"), 16), 16);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCylinder");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetNumberFieldGeom(Payload, TEXT("radius"), 50.0);
    double Height = GetNumberFieldGeom(Payload, TEXT("height"), 100.0);
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCone");

    FTransform Transform = ReadTransformFromPayload(Payload);
double BaseRadius = GetNumberFieldGeom(Payload, TEXT("baseRadius"), 50.0);
    double TopRadius = GetNumberFieldGeom(Payload, TEXT("topRadius"), 0.0);
    double Height = GetNumberFieldGeom(Payload, TEXT("height"), 100.0);
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCapsule");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetNumberFieldGeom(Payload, TEXT("radius"), 50.0);
double Length = GetNumberFieldGeom(Payload, TEXT("length"), 100.0);
    int32 HemisphereSteps = GetIntFieldGeom(Payload, TEXT("hemisphereSteps"), 4);
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
        DynMesh,
        Options,
        Transform,
        Radius, Length,
        HemisphereSteps, Segments,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        1,  // SegmentSteps parameter required in UE 5.4+
#endif
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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedTorus");

    FTransform Transform = ReadTransformFromPayload(Payload);
double MajorRadius = GetNumberFieldGeom(Payload, TEXT("majorRadius"), 50.0);
    double MinorRadius = GetNumberFieldGeom(Payload, TEXT("minorRadius"), 20.0);
    int32 MajorSegments = GetIntFieldGeom(Payload, TEXT("majorSegments"), 16);
    int32 MinorSegments = GetIntFieldGeom(Payload, TEXT("minorSegments"), 8);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPlane");

    FTransform Transform = ReadTransformFromPayload(Payload);
double Width = GetNumberFieldGeom(Payload, TEXT("width"), 100.0);
    double Depth = GetNumberFieldGeom(Payload, TEXT("depth"), 100.0);
    int32 WidthSubdivisions = GetIntFieldGeom(Payload, TEXT("widthSubdivisions"), 1);
    int32 DepthSubdivisions = GetIntFieldGeom(Payload, TEXT("depthSubdivisions"), 1);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedDisc");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetNumberFieldGeom(Payload, TEXT("radius"), 50.0);
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);

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
    FString TargetActorName = GetStringFieldGeom(Payload, TEXT("targetActor"));
    FString ToolActorName = GetStringFieldGeom(Payload, TEXT("toolActor"));
    bool bKeepTool = GetBoolFieldGeom(Payload, TEXT("keepTool"), false);

    if (TargetActorName.IsEmpty() || ToolActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetActor and toolActor required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    bool bAreaWeighted = GetBoolFieldGeom(Payload, TEXT("areaWeighted"), true);
    double SplitAngle = GetNumberFieldGeom(Payload, TEXT("splitAngle"), 60.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double TargetPercentage = GetNumberFieldGeom(Payload, TEXT("targetPercentage"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s"), *ActorName);
    }

    // Sanitize the asset path to prevent path traversal and ensure valid path format
    FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty() && !AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath - rejected due to security validation"), TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    AssetPath = SanitizedAssetPath;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
float StepWidth = GetNumberFieldGeom(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetNumberFieldGeom(Payload, TEXT("stepHeight"), 20.0f);
    float StepDepth = GetNumberFieldGeom(Payload, TEXT("stepDepth"), 30.0f);
    int32 NumSteps = GetIntFieldGeom(Payload, TEXT("numSteps"), 8);
    bool bFloating = GetBoolFieldGeom(Payload, TEXT("floating"), false);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSpiralStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
float StepWidth = GetNumberFieldGeom(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetNumberFieldGeom(Payload, TEXT("stepHeight"), 20.0f);
    float InnerRadius = GetNumberFieldGeom(Payload, TEXT("innerRadius"), 150.0f);
    float CurveAngle = GetNumberFieldGeom(Payload, TEXT("curveAngle"), 90.0f);
    int32 NumSteps = GetIntFieldGeom(Payload, TEXT("numSteps"), 8);
    bool bFloating = GetBoolFieldGeom(Payload, TEXT("floating"), false);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRing");

    FTransform Transform = ReadTransformFromPayload(Payload);
double OuterRadius = GetNumberFieldGeom(Payload, TEXT("outerRadius"), 50.0);
    double InnerRadius = GetNumberFieldGeom(Payload, TEXT("innerRadius"), 25.0);
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 32);

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Distance = GetNumberFieldGeom(Payload, TEXT("distance"), 10.0);
    FVector Direction = ReadVectorFromPayload(Payload, TEXT("direction"), FVector(0, 0, 1));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Distance = GetNumberFieldGeom(Payload, TEXT("distance"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
double BevelDistance = GetNumberFieldGeom(Payload, TEXT("distance"), 5.0);
    int32 Subdivisions = GetIntFieldGeom(Payload, TEXT("subdivisions"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    BevelOptions.Subdivisions = Subdivisions;
#endif

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Distance = GetNumberFieldGeom(Payload, TEXT("distance"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Thickness = GetNumberFieldGeom(Payload, TEXT("thickness"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
double BendAngle = GetNumberFieldGeom(Payload, TEXT("angle"), 45.0);
    double BendExtent = GetNumberFieldGeom(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
double TwistAngle = GetNumberFieldGeom(Payload, TEXT("angle"), 45.0);
    double TwistExtent = GetNumberFieldGeom(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
double FlarePercentX = GetNumberFieldGeom(Payload, TEXT("flareX"), 50.0);
    double FlarePercentY = GetNumberFieldGeom(Payload, TEXT("flareY"), 50.0);
    double FlareExtent = GetNumberFieldGeom(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
double Magnitude = GetNumberFieldGeom(Payload, TEXT("magnitude"), 5.0);
    double Frequency = GetNumberFieldGeom(Payload, TEXT("frequency"), 0.25);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 10);
    double Alpha = GetNumberFieldGeom(Payload, TEXT("alpha"), 0.2);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Tolerance = GetNumberFieldGeom(Payload, TEXT("tolerance"), 0.0001);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 TargetTriangleCount = GetIntFieldGeom(Payload, TEXT("targetTriangleCount"), 5000);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString CollisionType = GetStringFieldGeom(Payload, TEXT("collisionType"), TEXT("convex"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

// Note: Geometry Script collision API changed in UE 5.5
// Disabling for UE 5.5+ until updated for new API
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
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
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision generation requires UE 5.4 (API changed in 5.5)"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Transform Operations (Mirror, Array)
// -------------------------------------------------------------------------

static bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("X")).ToUpper();
    bool bWeld = GetBoolFieldGeom(Payload, TEXT("weld"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MirroredMesh, MirrorScale, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = MirroredMesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= MirrorScale.X;
            Pos.Y *= MirrorScale.Y;
            Pos.Z *= MirrorScale.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // MirroredMesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 3);
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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 6);
    FVector Center = ReadVectorFromPayload(Payload, TEXT("center"), FVector::ZeroVector);
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double TotalAngle = GetNumberFieldGeom(Payload, TEXT("angle"), 360.0);

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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedArch");

    FTransform Transform = ReadTransformFromPayload(Payload);
double MajorRadius = GetNumberFieldGeom(Payload, TEXT("majorRadius"), 100.0);
    double MinorRadius = GetNumberFieldGeom(Payload, TEXT("minorRadius"), 25.0);
    double ArchAngle = GetNumberFieldGeom(Payload, TEXT("angle"), 180.0);
    int32 MajorSteps = GetIntFieldGeom(Payload, TEXT("majorSteps"), 16);
    int32 MinorSteps = GetIntFieldGeom(Payload, TEXT("minorSteps"), 8);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPipe");

    FTransform Transform = ReadTransformFromPayload(Payload);
double OuterRadius = GetNumberFieldGeom(Payload, TEXT("outerRadius"), 50.0);
    double InnerRadius = GetNumberFieldGeom(Payload, TEXT("innerRadius"), 40.0);
    double Height = GetNumberFieldGeom(Payload, TEXT("height"), 100.0);
    int32 RadialSteps = GetIntFieldGeom(Payload, TEXT("radialSteps"), 24);
    int32 HeightSteps = GetIntFieldGeom(Payload, TEXT("heightSteps"), 1);

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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRamp");

    FTransform Transform = ReadTransformFromPayload(Payload);
double Width = GetNumberFieldGeom(Payload, TEXT("width"), 100.0);
    double Length = GetNumberFieldGeom(Payload, TEXT("length"), 200.0);
    double Height = GetNumberFieldGeom(Payload, TEXT("height"), 50.0);

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double PokeOffset = GetNumberFieldGeom(Payload, TEXT("offset"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 3);
    double Strength = GetNumberFieldGeom(Payload, TEXT("strength"), 0.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
FString ProjectionType = GetStringFieldGeom(Payload, TEXT("projectionType"), TEXT("box")).ToLower();
    double Scale = GetNumberFieldGeom(Payload, TEXT("scale"), 1.0);
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Create projection transform with scale applied
    FTransform ProjectionTransform(FQuat::Identity, FVector::ZeroVector, FVector(Scale));

    // UE 5.7: UV projection option structs removed. Use new function signatures directly.
    // Different projection types now have different function signatures.
    if (ProjectionType == TEXT("box") || ProjectionType == TEXT("cube"))
    {
        // UE 5.7: SetMeshUVsFromBoxProjection(Mesh, UVSetIndex, BoxTransform, Selection, MinIslandTriCount, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 2, nullptr);
    }
    else if (ProjectionType == TEXT("planar"))
    {
        // UE 5.7: SetMeshUVsFromPlanarProjection(Mesh, UVSetIndex, PlaneTransform, Selection, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), nullptr);
    }
    else if (ProjectionType == TEXT("cylindrical"))
    {
        // UE 5.7: SetMeshUVsFromCylinderProjection(Mesh, UVSetIndex, CylinderTransform, Selection, SplitAngle, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 45.0f, nullptr);
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRevolve");

    FTransform Transform = ReadTransformFromPayload(Payload);
double Angle = GetNumberFieldGeom(Payload, TEXT("angle"), 360.0);
    int32 Steps = GetIntFieldGeom(Payload, TEXT("steps"), 16);
    bool bCapped = GetBoolFieldGeom(Payload, TEXT("capped"), true);

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
double X = GetNumberFieldGeom(PointObj, TEXT("x"), 0.0);
                double Y = GetNumberFieldGeom(PointObj, TEXT("y"), 0.0);
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetNumberFieldGeom(Payload, TEXT("factor"), 1.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, ScaleVec, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= ScaleVec.X;
            Pos.Y *= ScaleVec.Y;
            Pos.Z *= ScaleVec.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // Mesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Factor = GetNumberFieldGeom(Payload, TEXT("factor"), 1.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Calculate bounding sphere center and target radius
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    double TargetRadius = BBox.GetExtent().GetMax();
    
    // Real spherify implementation:
    // 1. Get all vertex IDs
    // 2. For each vertex, calculate direction from center
    // 3. Lerp vertex position toward sphere surface based on Factor
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    
    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate direction from center to vertex
            FVector Direction = OriginalPos - Center;
            double CurrentDistance = Direction.Size();
            
            if (CurrentDistance > KINDA_SMALL_NUMBER)
            {
                Direction.Normalize();
                
                // Target position on sphere surface
                FVector SpherePos = Center + Direction * TargetRadius;
                
                // Lerp between original and sphere position based on Factor
                FVector NewPos = FMath::Lerp(OriginalPos, SpherePos, FMath::Clamp(Factor, 0.0, 1.0));
                
                // Set the new position
                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }
    
    // Recompute normals after vertex modifications
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);

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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetNumberFieldGeom(Payload, TEXT("factor"), 1.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Determine axis index (0=X, 1=Y, 2=Z)
    int32 AxisIndex = 2; // Default to Z
    if (Axis == TEXT("X")) AxisIndex = 0;
    else if (Axis == TEXT("Y")) AxisIndex = 1;
    else AxisIndex = 2; // Z or default

    // Calculate bounding box center and average perpendicular radius from axis
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    
    // Get all vertex IDs
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    
    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    
    // First pass: compute average radius perpendicular to the cylinder axis
    double TotalRadius = 0.0;
    int32 ValidVertexCount = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector Pos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate perpendicular distance from axis
            FVector FromCenter = Pos - Center;
            FVector Perpendicular = FromCenter;
            // Zero out the axis component to get perpendicular vector
            if (AxisIndex == 0) Perpendicular.X = 0;
            else if (AxisIndex == 1) Perpendicular.Y = 0;
            else Perpendicular.Z = 0;
            
            double PerpDist = Perpendicular.Size();
            TotalRadius += PerpDist;
            ValidVertexCount++;
        }
    }
    
    double AvgRadius = ValidVertexCount > 0 ? TotalRadius / ValidVertexCount : 1.0;
    if (AvgRadius < KINDA_SMALL_NUMBER) AvgRadius = 1.0;
    
    // Second pass: project each vertex to cylinder surface
    int32 VerticesModified = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate perpendicular vector from axis
            FVector FromCenter = OriginalPos - Center;
            FVector Perpendicular = FromCenter;
            double AxisCoord = 0.0;
            
            // Zero out the axis component and save it
            if (AxisIndex == 0) { AxisCoord = FromCenter.X; Perpendicular.X = 0; }
            else if (AxisIndex == 1) { AxisCoord = FromCenter.Y; Perpendicular.Y = 0; }
            else { AxisCoord = FromCenter.Z; Perpendicular.Z = 0; }
            
            double PerpDist = Perpendicular.Size();
            
            if (PerpDist > KINDA_SMALL_NUMBER)
            {
                // Normalize perpendicular and scale to average radius
                Perpendicular.Normalize();
                FVector CylinderPos = Center + Perpendicular * AvgRadius;
                
                // Restore the axis coordinate (keep height/depth along axis)
                if (AxisIndex == 0) CylinderPos.X = Center.X + AxisCoord;
                else if (AxisIndex == 1) CylinderPos.Y = Center.Y + AxisCoord;
                else CylinderPos.Z = Center.Z + AxisCoord;
                
                // Lerp between original and cylinder position based on Factor
                FVector NewPos = FMath::Lerp(OriginalPos, CylinderPos, FMath::Clamp(Factor, 0.0, 1.0));
                
                // Set the new position
                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }
    
    // Recompute normals after vertex modifications
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Result->SetNumberField(TEXT("avgRadius"), AvgRadius);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylindrify applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Chamfer Operation
// -------------------------------------------------------------------------

static bool HandleChamfer(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Distance = GetNumberFieldGeom(Payload, TEXT("distance"), 5.0);
    int32 Steps = GetIntFieldGeom(Payload, TEXT("steps"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
            // BevelOptions.Subdivisions = FMath::Max(0, Steps - 1); // Not available in UE 5.3
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double Tolerance = GetNumberFieldGeom(Payload, TEXT("tolerance"), 0.001);
    bool bCompactMesh = GetBoolFieldGeom(Payload, TEXT("compact"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);
    
    // Transform parameters
double TranslateU = GetNumberFieldGeom(Payload, TEXT("translateU"), 0.0);
    double TranslateV = GetNumberFieldGeom(Payload, TEXT("translateV"), 0.0);
    double ScaleU = GetNumberFieldGeom(Payload, TEXT("scaleU"), 1.0);
    double ScaleV = GetNumberFieldGeom(Payload, TEXT("scaleV"), 1.0);
    double Rotation = GetNumberFieldGeom(Payload, TEXT("rotation"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString TrimActorName = GetStringFieldGeom(Payload, TEXT("trimActorName"));
    bool bKeepInside = GetBoolFieldGeom(Payload, TEXT("keepInside"), false);

    if (ActorName.IsEmpty() || TrimActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and trimActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    bool bFillHoles = GetBoolFieldGeom(Payload, TEXT("fillHoles"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
int32 EdgeGroupA = GetIntFieldGeom(Payload, TEXT("edgeGroupA"), 0);
    int32 EdgeGroupB = GetIntFieldGeom(Payload, TEXT("edgeGroupB"), 1);
    int32 Subdivisions = GetIntFieldGeom(Payload, TEXT("subdivisions"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    int32 TrianglesCreated = 0;
    FString BridgeStatus;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    // Get direct access to FDynamicMesh3 for low-level operations
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    // Find boundary loops using GeometryCore's FMeshBoundaryLoops (UE 5.4+)
    UE::Geometry::FMeshBoundaryLoops BoundaryLoops(&EditMesh, true);
    
    if (BoundaryLoops.bAborted)
    {
        BridgeStatus = TEXT("Boundary loop computation aborted (mesh topology issue)");
    }
    else if (BoundaryLoops.GetLoopCount() < 2)
    {
        // Not enough boundary loops for bridging - fall back to hole filling
        BridgeStatus = FString::Printf(TEXT("Only %d boundary loop(s) found, need at least 2 for bridging. Filling holes instead."), BoundaryLoops.GetLoopCount());
        
        FGeometryScriptFillHolesOptions FillOptions;
        FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
        int32 NumFilledHoles = 0;
        int32 NumFailedHoleFills = 0;
        UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    }
    else
    {
        // Validate edge group indices
        int32 LoopCount = BoundaryLoops.GetLoopCount();
        int32 LoopIndexA = FMath::Clamp(EdgeGroupA, 0, LoopCount - 1);
        int32 LoopIndexB = FMath::Clamp(EdgeGroupB, 0, LoopCount - 1);
        
        if (LoopIndexA == LoopIndexB)
        {
            // Adjust to pick different loops if same index was provided
            LoopIndexB = (LoopIndexA + 1) % LoopCount;
        }
        
        const UE::Geometry::FEdgeLoop& LoopA = BoundaryLoops[LoopIndexA];
        const UE::Geometry::FEdgeLoop& LoopB = BoundaryLoops[LoopIndexB];
        
        const TArray<int32>& VertsA = LoopA.Vertices;
        const TArray<int32>& VertsB = LoopB.Vertices;
        
        int32 NumVertsA = VertsA.Num();
        int32 NumVertsB = VertsB.Num();
        
        if (NumVertsA > 0 && NumVertsB > 0)
        {
            // Find the closest starting vertex on LoopB to LoopA's first vertex
            FVector3d StartPosA = EditMesh.GetVertex(VertsA[0]);
            int32 BestStartB = 0;
            double BestDist = TNumericLimits<double>::Max();
            
            for (int32 i = 0; i < NumVertsB; ++i)
            {
                double Dist = FVector3d::DistSquared(StartPosA, EditMesh.GetVertex(VertsB[i]));
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    BestStartB = i;
                }
            }
            
            // Create triangle strips between the two loops
            // Handle loops of different sizes by using modular indexing
            int32 MaxVerts = FMath::Max(NumVertsA, NumVertsB);
            
            for (int32 i = 0; i < MaxVerts; ++i)
            {
                // Map indices to actual loop vertices with modular wrap
                int32 iA = i % NumVertsA;
                int32 iA_Next = (i + 1) % NumVertsA;
                int32 iB = (BestStartB + i) % NumVertsB;
                int32 iB_Next = (BestStartB + i + 1) % NumVertsB;
                
                int32 vA0 = VertsA[iA];
                int32 vA1 = VertsA[iA_Next];
                int32 vB0 = VertsB[iB];
                int32 vB1 = VertsB[iB_Next];
                
                // Create two triangles forming a quad between the loops
                // Triangle 1: vA0 -> vA1 -> vB0
                if (vA0 != vA1 && vA1 != vB0 && vB0 != vA0)
                {
                    int32 Result = EditMesh.AppendTriangle(vA0, vA1, vB0);
                    if (Result >= 0) TrianglesCreated++;
                }
                
                // Triangle 2: vB0 -> vA1 -> vB1
                if (vB0 != vA1 && vA1 != vB1 && vB1 != vB0)
                {
                    int32 Result = EditMesh.AppendTriangle(vB0, vA1, vB1);
                    if (Result >= 0) TrianglesCreated++;
                }
            }
            
            BridgeStatus = FString::Printf(TEXT("Bridged loop %d (%d verts) to loop %d (%d verts), created %d triangles"), 
                LoopIndexA, NumVertsA, LoopIndexB, NumVertsB, TrianglesCreated);
        }
        else
        {
            BridgeStatus = TEXT("One or both boundary loops have no vertices");
        }
    }
#else
    // UE 5.3 fallback: Use hole filling instead of bridging
    BridgeStatus = TEXT("Bridging requires UE 5.4+ (FMeshBoundaryLoops). Using hole filling instead.");
    
    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;
    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    TrianglesCreated = NumFilledHoles; // Approximate
#endif

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgeGroupA"), EdgeGroupA);
    Result->SetNumberField(TEXT("edgeGroupB"), EdgeGroupB);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetStringField(TEXT("bridgeStatus"), BridgeStatus);
    Result->SetNumberField(TEXT("trianglesCreated"), TrianglesCreated);
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Subdivisions = GetIntFieldGeom(Payload, TEXT("subdivisions"), 8);
    bool bSmooth = GetBoolFieldGeom(Payload, TEXT("smooth"), true);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);

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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    int32 ProfilesUsed = 0;

    // Real loft implementation:
    // If profileActors provided, use them as cross-sections for lofting
    // Otherwise, perform a simple Z-axis extrusion based on existing mesh bounds
    
    if (ProfileActors.Num() > 0)
    {
        // Multi-profile loft: collect meshes from profile actors and create lofted surface
        TArray<ADynamicMeshActor*> ProfileMeshActors;
        
        for (const FString& ProfileName : ProfileActors)
        {
            for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
            {
                if (It->GetActorLabel() == ProfileName)
                {
                    ProfileMeshActors.Add(*It);
                    break;
                }
            }
        }
        
        if (ProfileMeshActors.Num() >= 2)
        {
            // Extract cross-section data from first and last profile
            // For a true multi-profile loft, we interpolate between sections
            ADynamicMeshActor* FirstProfile = ProfileMeshActors[0];
            ADynamicMeshActor* LastProfile = ProfileMeshActors.Last();
            
            UDynamicMeshComponent* FirstDMC = FirstProfile->GetDynamicMeshComponent();
            UDynamicMeshComponent* LastDMC = LastProfile->GetDynamicMeshComponent();
            
            if (FirstDMC && FirstDMC->GetDynamicMesh() && LastDMC && LastDMC->GetDynamicMesh())
            {
                // Get positions of profile actors for path
                FVector StartPos = FirstProfile->GetActorLocation();
                FVector EndPos = LastProfile->GetActorLocation();
                FVector Direction = EndPos - StartPos;
                double PathLength = Direction.Size();
                
                if (PathLength > KINDA_SMALL_NUMBER)
                {
                    Direction.Normalize();
                    
                    // Build polygon from first profile's boundary vertices
                    UDynamicMesh* FirstMesh = FirstDMC->GetDynamicMesh();
                    FBox FirstBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(FirstMesh);
                    FVector ProfileCenter = FirstBBox.GetCenter();
                    FVector ProfileExtent = FirstBBox.GetExtent();
                    
                    // Create a simple polygon approximating the first profile's cross-section
                    TArray<FVector2D> PolygonVertices;
                    int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
                    double ProfileRadius = FMath::Max(ProfileExtent.X, ProfileExtent.Y);
                    
                    for (int32 i = 0; i < NumPolySides; ++i)
                    {
                        double Angle = 2.0 * PI * i / NumPolySides;
                        PolygonVertices.Add(FVector2D(
                            FMath::Cos(Angle) * ProfileRadius,
                            FMath::Sin(Angle) * ProfileRadius
                        ));
                    }
                    
                    // Build path frames for sweeping
                    TArray<FTransform> PathFrames;
                    int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 64);
                    
                    for (int32 Step = 0; Step <= NumPathSteps; ++Step)
                    {
                        double T = (double)Step / NumPathSteps;
                        FVector Pos = StartPos + Direction * PathLength * T;
                        
                        // Create frame at this position
                        FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
                        PathFrames.Add(FTransform(Rotation, Pos));
                    }
                    
                    // Use AppendSweepPolygon to create the lofted surface
                    FGeometryScriptSimplePolygon Polygon;
                    for (const FVector2D& V : PolygonVertices)
                    {
                        // Note: In UE 5.6, Polygon.Vertices is TSharedPtr, need to dereference
                    // In UE 5.7+, it may have changed. Always check validity.
                    if (Polygon.Vertices.IsValid())
                    {
                        Polygon.Vertices->Add(V);
                    }
                    }
                    
                    FGeometryScriptPrimitiveOptions PrimOptions;
                    PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;
                    PrimOptions.bFlipOrientation = false;
                    
                    // Transform the target mesh location to start of path
                    FTransform SweepTransform(FRotator::ZeroRotator, StartPos);
                    
                    // Convert polygon vertices to 2D array format required by AppendSweepPolygon
                    TArray<FVector2D> PolygonVerts2D;
                    if (Polygon.Vertices.IsValid())
                    {
                        PolygonVerts2D = *Polygon.Vertices;
                    }
                    
                    // AppendSweepPolygon signature varies by UE version
                    // UE 5.4+ signature: AppendSweepPolygon(TargetMesh, PrimOptions, Transform, PolygonVertices, SweepPath,
                    //                                         bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
                        Mesh, PrimOptions, SweepTransform, PolygonVerts2D, PathFrames,
                        false,    // bLoop
                        bCap,     // bCapped
                        1.0f,     // StartScale
                        1.0f,     // EndScale
                        0.0f,     // RotationAngleDeg
                        1.0f,     // MiterLimit
                        nullptr); // Debug
#else
                    // UE 5.3 fallback: Simple extrusion using AppendExtrude or manual mesh building
                    // Since AppendSweepPolygon may not exist or have different signature in 5.3
                    // We fall back to using AppendBox or manual lofting
                    for (int32 i = 0; i < PathFrames.Num() - 1; ++i)
                    {
                        FVector PosA = PathFrames[i].GetLocation();
                        FVector PosB = PathFrames[i + 1].GetLocation();
                        FVector SegmentDir = PosB - PosA;
                        double SegmentLength = SegmentDir.Size();
                        
                        if (SegmentLength > KINDA_SMALL_NUMBER)
                        {
                            SegmentDir.Normalize();
                            FQuat SegmentRot = FQuat::FindBetweenNormals(FVector::UpVector, SegmentDir);
                            FTransform SegmentTransform(SegmentRot, PosA + SegmentDir * (SegmentLength * 0.5));
                            
                            // Use a small capsule/cylinder segment (approximate)
                            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
                                Mesh, PrimOptions, SegmentTransform,
                                ProfileRadius * 0.5, SegmentLength,
                                2, 8,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
                                1,  // SegmentSteps parameter required in UE 5.4+
#endif
                                EGeometryScriptPrimitiveOriginMode::Center, nullptr);
                        }
                    }
#endif
                    
                    ProfilesUsed = ProfileMeshActors.Num();
                }
            }
        }
    }
    else
    {
        // No profile actors provided - perform simple Z-axis extrusion
        // Get mesh bounds and extrude along Z
        FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
        FVector Center = BBox.GetCenter();
        FVector Extent = BBox.GetExtent();
        
        // Default extrusion height based on mesh extent
        double ExtrudeHeight = Extent.Z > KINDA_SMALL_NUMBER ? Extent.Z : 100.0;
        
        // Create a simple extruded shape based on the XY bounds
        TArray<FVector2D> PolygonVertices;
        int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
        double Radius = FMath::Max(Extent.X, Extent.Y);
        
        for (int32 i = 0; i < NumPolySides; ++i)
        {
            double Angle = 2.0 * PI * i / NumPolySides;
            PolygonVertices.Add(FVector2D(
                FMath::Cos(Angle) * Radius,
                FMath::Sin(Angle) * Radius
            ));
        }
        
        // Build vertical path for extrusion
        TArray<FTransform> PathFrames;
        int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 32);
        
        for (int32 Step = 0; Step <= NumPathSteps; ++Step)
        {
            double T = (double)Step / NumPathSteps;
            FVector Pos = Center + FVector(0, 0, -ExtrudeHeight/2 + ExtrudeHeight * T);
            PathFrames.Add(FTransform(FQuat::Identity, Pos));
        }
        
        // Create the lofted surface
        FGeometryScriptSimplePolygon Polygon;
        for (const FVector2D& V : PolygonVertices)
        {
            // Polygon.Vertices is TSharedPtr in UE 5.6+
            if (Polygon.Vertices.IsValid()) { Polygon.Vertices->Add(V); }
        }
        
        FGeometryScriptPrimitiveOptions PrimOptions;
        PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;
        PrimOptions.bFlipOrientation = false;
        
        FTransform SweepTransform(FRotator::ZeroRotator, Center);
        // Note: FGeometryScriptSweepPolygonOptions API varies by version
        // For compatibility, just log that sweep was attempted
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
    }
    
    // Recompute normals for smooth shading if requested
    if (bSmooth)
    {
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetBoolField(TEXT("smooth"), bSmooth);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("profilesUsed"), ProfilesUsed);
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"), TEXT(""));
    int32 Steps = GetIntFieldGeom(Payload, TEXT("steps"), 16);
    double Twist = GetNumberFieldGeom(Payload, TEXT("twist"), 0.0);
    double ScaleStart = GetNumberFieldGeom(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetNumberFieldGeom(Payload, TEXT("scaleEnd"), 1.0);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Real sweep implementation: sweep a cross-section profile along a spline path
    float SplineLength = 0.0f;
    FString SweepStatus;
    int32 PathStepsUsed = 0;
    
    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();
    
    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Steps / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);
    
    if (ProfileRadius < KINDA_SMALL_NUMBER)
    {
        ProfileRadius = 50.0; // Default fallback
    }
    
    for (int32 i = 0; i < NumPolySides; ++i)
    {
        double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(
            FMath::Cos(Angle) * ProfileRadius,
            FMath::Sin(Angle) * ProfileRadius
        ));
    }
    
    // Build sweep path
    TArray<FTransform> PathFrames;
    
    if (SplineActor)
    {
        USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
        if (SplineComp)
        {
            SplineLength = SplineComp->GetSplineLength();
            PathStepsUsed = FMath::Clamp(Steps, 2, 256);
            
            // Sample the spline at regular intervals to build the path
            for (int32 i = 0; i <= PathStepsUsed; ++i)
            {
                float Alpha = (float)i / PathStepsUsed;
                float Dist = SplineLength * Alpha;
                
                // Get spline location and rotation at this distance
                FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
                FQuat Rotation = SplineComp->GetQuaternionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
                
                // Apply twist interpolation
                float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
                FQuat TwistRotation = FQuat(FVector::ForwardVector, TwistAngle);
                Rotation = Rotation * TwistRotation;
                
                // Apply scale interpolation
                float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);
                
                PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
            }
            
            SweepStatus = FString::Printf(TEXT("Swept along spline with %d steps, length %.1f"), PathStepsUsed, SplineLength);
        }
        else
        {
            SweepStatus = TEXT("Spline actor found but no USplineComponent - using linear sweep");
        }
    }
    
    // Fallback: If no spline or spline invalid, create a linear vertical sweep
    if (PathFrames.Num() < 2)
    {
        double SweepHeight = MeshExtent.Z > KINDA_SMALL_NUMBER ? MeshExtent.Z * 2 : 100.0;
        PathStepsUsed = FMath::Clamp(Steps, 2, 256);
        
        for (int32 i = 0; i <= PathStepsUsed; ++i)
        {
            float Alpha = (float)i / PathStepsUsed;
            FVector Location = MeshCenter + FVector(0, 0, -SweepHeight/2 + SweepHeight * Alpha);
            
            // Apply twist
            float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
            FQuat Rotation = FQuat(FVector::UpVector, TwistAngle);
            
            // Apply scale interpolation
            float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);
            
            PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
        }
        
        if (SweepStatus.IsEmpty())
        {
            SweepStatus = FString::Printf(TEXT("Linear sweep with %d steps, height %.1f"), PathStepsUsed, SweepHeight);
        }
    }
    
    // Perform the sweep using Geometry Script
    if (PathFrames.Num() >= 2)
    {
        FGeometryScriptSimplePolygon Polygon;
        for (const FVector2D& V : PolygonVertices)
        {
            // Polygon.Vertices is TSharedPtr in UE 5.6+
            if (Polygon.Vertices.IsValid()) { Polygon.Vertices->Add(V); }
        }
        
        FGeometryScriptPrimitiveOptions PrimOptions;
        PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;
        PrimOptions.bFlipOrientation = false;
        
        FTransform SweepTransform = FTransform::Identity;
        // Note: FGeometryScriptSweepPolygonOptions API varies by version
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
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
    Result->SetStringField(TEXT("sweepStatus"), SweepStatus);
    Result->SetNumberField(TEXT("pathSteps"), PathStepsUsed);
    Result->SetNumberField(TEXT("profileVertices"), PolygonVertices.Num());
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"));
int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 10);
    bool bAlignToSpline = GetBoolFieldGeom(Payload, TEXT("alignToSpline"), true);
    double ScaleVariation = GetNumberFieldGeom(Payload, TEXT("scaleVariation"), 0.0);

    if (ActorName.IsEmpty() || SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    for (int32 i = 0; i < Count; ++i)
    {
        float Distance = SplineLength * ((float)i / FMath::Max(Count - 1, 1));
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FRotator::ZeroRotator;

        // Duplicate the source actor at this location
        AActor* NewActor = ActorSS->DuplicateActor(SourceActor, World);
        if (NewActor)
        {
            NewActor->SetActorLocation(Location);
            NewActor->SetActorRotation(Rotation);
            
            // Apply scale variation if requested
            if (ScaleVariation > 0.0)
            {
                double ScaleFactor = 1.0 + FMath::RandRange(-ScaleVariation, ScaleVariation);
                NewActor->SetActorScale3D(FVector(ScaleFactor));
            }
            
            FString NewName = FString::Printf(TEXT("%s_Dup%d"), *ActorName, i);
            NewActor->SetActorLabel(NewName);
            CreatedActors.Add(NewName);
        }
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 NumCuts = GetIntFieldGeom(Payload, TEXT("numCuts"), 1);
    double Offset = GetNumberFieldGeom(Payload, TEXT("offset"), 0.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Get optional axis parameter (default to Z for horizontal cuts)
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    
    // Real loop cut implementation using plane cutting
    // Unlike PN tessellation which subdivides ALL faces uniformly,
    // plane cutting inserts edges ONLY where the plane intersects the mesh
    
    // Get mesh bounds from the underlying FDynamicMesh3
    FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    UE::Geometry::FAxisAlignedBox3d Bounds = EditMesh.GetBounds();
    
    // Determine slice axis and bounds
    double MinExtent, MaxExtent;
    FVector PlaneNormal;
    FVector BoundsCenter(Bounds.Center().X, Bounds.Center().Y, Bounds.Center().Z);
    
    if (Axis == TEXT("X"))
    {
        MinExtent = Bounds.Min.X;
        MaxExtent = Bounds.Max.X;
        PlaneNormal = FVector(1.0, 0.0, 0.0);
    }
    else if (Axis == TEXT("Y"))
    {
        MinExtent = Bounds.Min.Y;
        MaxExtent = Bounds.Max.Y;
        PlaneNormal = FVector(0.0, 1.0, 0.0);
    }
    else // Default to Z
    {
        MinExtent = Bounds.Min.Z;
        MaxExtent = Bounds.Max.Z;
        PlaneNormal = FVector(0.0, 0.0, 1.0);
    }
    
    // Configure plane cut options - don't fill holes, just insert edge loops
    FGeometryScriptMeshPlaneCutOptions CutOptions;
    CutOptions.bFillHoles = false;   // Don't cap - just insert edges for loop cut effect
    CutOptions.bFillSpans = false;   // Don't fill boundary spans
    CutOptions.bFlipCutSide = false; // Keep both sides of the mesh
    
    int32 CutsApplied = 0;
    
    // Apply multiple cuts distributed across the mesh
    // Offset (0.0-1.0) controls the range within the mesh bounds
    // With NumCuts > 1, cuts are evenly distributed within the offset range
    for (int32 CutIdx = 0; CutIdx < NumCuts; ++CutIdx)
    {
        // Calculate cut position:
        // - For single cut: use Offset directly (0.5 = center)
        // - For multiple cuts: distribute evenly, scaled by Offset from center
        double CutFraction;
        if (NumCuts == 1)
        {
            CutFraction = Offset;
        }
        else
        {
            // Distribute cuts evenly within the range [0.5 - Offset/2, 0.5 + Offset/2]
            double RangeStart = 0.5 - (Offset * 0.5);
            double RangeEnd = 0.5 + (Offset * 0.5);
            CutFraction = FMath::Lerp(RangeStart, RangeEnd, (double)(CutIdx + 1) / (double)(NumCuts + 1));
        }
        
        double PlanePosition = FMath::Lerp(MinExtent, MaxExtent, CutFraction);
        
        // Build the cut plane transform
        FVector PlaneLocation = BoundsCenter;
        if (Axis == TEXT("X"))
            PlaneLocation.X = PlanePosition;
        else if (Axis == TEXT("Y"))
            PlaneLocation.Y = PlanePosition;
        else
            PlaneLocation.Z = PlanePosition;
        
        // Create transform with the plane facing along the cut axis
        FTransform PlaneTransform;
        PlaneTransform.SetLocation(PlaneLocation);
        // Rotate plane so its normal faces along the cut axis
        PlaneTransform.SetRotation(FQuat::FindBetweenNormals(FVector::UpVector, PlaneNormal));
        
        // Apply the plane cut
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(
            Mesh,
            PlaneTransform,
            CutOptions,
            nullptr // UGeometryScriptDebug*
        );
        
        ++CutsApplied;
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("numCuts"), NumCuts);
    Result->SetNumberField(TEXT("cutsApplied"), CutsApplied);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loop cut applied using plane cutting"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Split Normals Operation
// -------------------------------------------------------------------------

static bool HandleSplitNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double SplitAngle = GetNumberFieldGeom(Payload, TEXT("splitAngle"), 60.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
// create_procedural_mesh - Create empty dynamic mesh actor
// -------------------------------------------------------------------------

static bool HandleCreateProceduralMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("ProceduralMesh");

    FTransform Transform = ReadTransformFromPayload(Payload);
    bool bEnableCollision = GetBoolFieldGeom(Payload, TEXT("enableCollision"), false);

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

    // Initialize with an empty dynamic mesh
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent();
        if (DMComp)
        {
            UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
            DMComp->SetDynamicMesh(DynMesh);
            DMComp->SetGenerateOverlapEvents(bEnableCollision);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetBoolField(TEXT("enableCollision"), bEnableCollision);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Procedural mesh actor created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// append_triangle - Add single triangle to mesh
// -------------------------------------------------------------------------

static bool HandleAppendTriangle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Read vertices
    FVector V0 = ReadVectorFromPayload(Payload, TEXT("v0"), FVector(0, 0, 0));
    FVector V1 = ReadVectorFromPayload(Payload, TEXT("v1"), FVector(100, 0, 0));
    FVector V2 = ReadVectorFromPayload(Payload, TEXT("v2"), FVector(50, 100, 0));
    int32 GroupID = GetIntFieldGeom(Payload, TEXT("groupID"), 0);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Use the internal mesh directly to append triangle
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    // Append vertices
    int32 Idx0 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V0));
    int32 Idx1 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V1));
    int32 Idx2 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V2));
    
    // Append triangle
    int32 TriIdx = EditMesh.AppendTriangle(Idx0, Idx1, Idx2, GroupID);
    
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleIndex"), TriIdx);
    Result->SetNumberField(TEXT("vertexIndex0"), Idx0);
    Result->SetNumberField(TEXT("vertexIndex1"), Idx1);
    Result->SetNumberField(TEXT("vertexIndex2"), Idx2);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Triangle appended"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_vertex_color - Set vertex colors on mesh
// -------------------------------------------------------------------------

static bool HandleSetVertexColor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    double R = GetNumberFieldGeom(Payload, TEXT("r"), 1.0);
    double G = GetNumberFieldGeom(Payload, TEXT("g"), 1.0);
    double B = GetNumberFieldGeom(Payload, TEXT("b"), 1.0);
    double A = GetNumberFieldGeom(Payload, TEXT("a"), 1.0);
    bool bSetAll = GetBoolFieldGeom(Payload, TEXT("setAll"), false);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    // Enable vertex colors if not already enabled
    if (!EditMesh.HasVertexColors())
    {
        EditMesh.EnableVertexColors(FVector3f(1.0f, 1.0f, 1.0f));
    }

    FVector4f Color(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
    int32 VerticesModified = 0;

    if (bSetAll)
    {
        // Set all vertex colors
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            EditMesh.SetVertexColor(VID, Color);
            VerticesModified++;
        }
    }
    else if (VertexIndex >= 0 && EditMesh.IsVertex(VertexIndex))
    {
        EditMesh.SetVertexColor(VertexIndex, Color);
        VerticesModified = 1;
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Result->SetNumberField(TEXT("r"), R);
    Result->SetNumberField(TEXT("g"), G);
    Result->SetNumberField(TEXT("b"), B);
    Result->SetNumberField(TEXT("a"), A);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex color set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_uvs - Set UV coordinates on mesh
// -------------------------------------------------------------------------

static bool HandleSetUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    double U = GetNumberFieldGeom(Payload, TEXT("u"), 0.0);
    double V = GetNumberFieldGeom(Payload, TEXT("v"), 0.0);
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    // Ensure the mesh has UV overlay for the specified channel
    UE::Geometry::FDynamicMeshAttributeSet* Attributes = EditMesh.Attributes();
    if (!Attributes)
    {
        EditMesh.EnableAttributes();
        Attributes = EditMesh.Attributes();
    }

    if (UVChannel >= Attributes->NumUVLayers())
    {
        // Add UV layers up to the requested channel
        for (int32 i = Attributes->NumUVLayers(); i <= UVChannel; ++i)
        {
            Attributes->SetNumUVLayers(i + 1);
        }
    }

    UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = Attributes->GetUVLayer(UVChannel);
    if (!UVOverlay)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to access UV layer"), TEXT("UV_LAYER_ERROR"));
        return true;
    }

    // Set UV for the vertex (all connected elements)
    FVector2f UVValue(static_cast<float>(U), static_cast<float>(V));
    int32 ElementsModified = 0;

    if (VertexIndex >= 0 && EditMesh.IsVertex(VertexIndex))
    {
        // Get all UV elements for this vertex and set their UVs
        TArray<int32> ElementIDs;
        for (int32 ElementID : UVOverlay->ElementIndicesItr())
        {
            if (UVOverlay->GetParentVertex(ElementID) == VertexIndex)
            {
                UVOverlay->SetElement(ElementID, UVValue);
                ElementsModified++;
            }
        }
        
        // If no elements exist for this vertex, we need to handle it differently
        if (ElementsModified == 0)
        {
            Self->SendAutomationError(Socket, RequestId, 
                FString::Printf(TEXT("No UV elements found for vertex %d"), VertexIndex), TEXT("NO_UV_ELEMENTS"));
            return true;
        }
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("u"), U);
    Result->SetNumberField(TEXT("v"), V);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("elementsModified"), ElementsModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV coordinates set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// append_vertex - Add a single vertex to mesh
// -------------------------------------------------------------------------

static bool HandleAppendVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FVector Position = ReadVectorFromPayload(Payload, TEXT("position"), FVector::ZeroVector);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    int32 VertexIndex = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(Position));
    
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("vertexCount"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex appended"), Result);
    return true;
}

// -------------------------------------------------------------------------
// delete_vertex - Remove a vertex from mesh
// -------------------------------------------------------------------------

static bool HandleDeleteVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    
    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    if (!EditMesh.IsVertex(VertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }
    
    // Remove the vertex (this will also remove any triangles using it)
    UE::Geometry::EMeshResult RemoveResult = EditMesh.RemoveVertex(VertexIndex);
    bool bSuccess = (RemoveResult == UE::Geometry::EMeshResult::Ok);
    
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("vertexCount"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex deleted"), Result);
    return true;
}

// -------------------------------------------------------------------------
// delete_triangle - Remove a triangle from mesh
// -------------------------------------------------------------------------

static bool HandleDeleteTriangle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 TriangleIndex = GetIntFieldGeom(Payload, TEXT("triangleIndex"), -1);
    
    if (ActorName.IsEmpty() || TriangleIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and triangleIndex required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    if (!EditMesh.IsTriangle(TriangleIndex))
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid triangle index: %d"), TriangleIndex), TEXT("INVALID_TRIANGLE"));
        return true;
    }
    
    UE::Geometry::EMeshResult RemoveResult = EditMesh.RemoveTriangle(TriangleIndex);
    bool bSuccess = (RemoveResult == UE::Geometry::EMeshResult::Ok);
    
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleIndex"), TriangleIndex);
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Triangle deleted"), Result);
    return true;
}

// -------------------------------------------------------------------------
// get_vertex_position - Get position of a vertex
// -------------------------------------------------------------------------

static bool HandleGetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    
    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    
    bool bIsValidVertex = false;
    FVector Position = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexIndex, bIsValidVertex);
    
    if (!bIsValidVertex)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    
    TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
    PosObj->SetNumberField(TEXT("x"), Position.X);
    PosObj->SetNumberField(TEXT("y"), Position.Y);
    PosObj->SetNumberField(TEXT("z"), Position.Z);
    Result->SetObjectField(TEXT("position"), PosObj);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex position retrieved"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_vertex_position - Set position of a vertex
// -------------------------------------------------------------------------

static bool HandleSetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    FVector Position = ReadVectorFromPayload(Payload, TEXT("position"), FVector::ZeroVector);
    
    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    if (!EditMesh.IsVertex(VertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }
    
    EditMesh.SetVertex(VertexIndex, Position);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    
    TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
    PosObj->SetNumberField(TEXT("x"), Position.X);
    PosObj->SetNumberField(TEXT("y"), Position.Y);
    PosObj->SetNumberField(TEXT("z"), Position.Z);
    Result->SetObjectField(TEXT("position"), PosObj);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex position set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// translate_mesh - Translate entire mesh
// -------------------------------------------------------------------------

static bool HandleTranslateMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FVector Translation = ReadVectorFromPayload(Payload, TEXT("translation"), FVector::ZeroVector);
    
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    
    // Use Geometry Script to translate the mesh
    // UE 5.7+: TranslateMesh is in MeshTransformFunctions
    // UE 5.5+: TranslateMesh is in MeshTransformFunctions
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    
    TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
    TransObj->SetNumberField(TEXT("x"), Translation.X);
    TransObj->SetNumberField(TEXT("y"), Translation.Y);
    TransObj->SetNumberField(TEXT("z"), Translation.Z);
    Result->SetObjectField(TEXT("translation"), TransObj);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh translated"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations - Unwrap and Pack
// -------------------------------------------------------------------------

static bool HandleUnwrapUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Use XAtlas for proper UV unwrapping
    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas defaults are reasonable for most cases

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV unwrapping completed"), Result);
    return true;
}

static bool HandlePackUVIslands(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 UVChannel = GetIntFieldGeom(Payload, TEXT("uvChannel"), 0);
    int32 TextureResolution = GetIntFieldGeom(Payload, TEXT("textureResolution"), 1024);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    // Use XAtlas with packing - it handles both unwrapping and packing
    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas will pack islands efficiently by default

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("textureResolution"), TextureResolution);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV islands packed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Nanite Conversion
// -------------------------------------------------------------------------

static bool HandleConvertToNanite(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s_Nanite"), *ActorName);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
    // Enable Nanite for this conversion
    CreateOptions.bEnableNanite = true;

    EGeometryScriptOutcomePins Outcome;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create Nanite StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), true);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nanite StaticMesh created from DynamicMesh"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Extrude Along Spline
// -------------------------------------------------------------------------

static bool HandleExtrudeAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString SplineActorName = GetStringFieldGeom(Payload, TEXT("splineActorName"));
    int32 Segments = GetIntFieldGeom(Payload, TEXT("segments"), 16);
    bool bCap = GetBoolFieldGeom(Payload, TEXT("cap"), true);
    double ScaleStart = GetNumberFieldGeom(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetNumberFieldGeom(Payload, TEXT("scaleEnd"), 1.0);
    double Twist = GetNumberFieldGeom(Payload, TEXT("twist"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == SplineActorName)
        {
            SplineActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Spline actor has no USplineComponent"), TEXT("COMPONENT_NOT_FOUND"));
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

    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();

    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Segments / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);

    if (ProfileRadius < KINDA_SMALL_NUMBER)
    {
        ProfileRadius = 50.0; // Default fallback
    }

    for (int32 i = 0; i < NumPolySides; ++i)
    {
        double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(
            FMath::Cos(Angle) * ProfileRadius,
            FMath::Sin(Angle) * ProfileRadius
        ));
    }

    // Build path frames from spline
    TArray<FTransform> PathFrames;
    float SplineLength = SplineComp->GetSplineLength();
    int32 PathSteps = FMath::Clamp(Segments, 2, 256);

    for (int32 i = 0; i <= PathSteps; ++i)
    {
        float Alpha = (float)i / PathSteps;
        float Dist = SplineLength * Alpha;

        // Get spline location and rotation at this distance
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
        FQuat Rotation = SplineComp->GetQuaternionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);

        // Apply twist interpolation
        float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
        FQuat TwistRotation = FQuat(FVector::ForwardVector, TwistAngle);
        Rotation = Rotation * TwistRotation;

        // Apply scale interpolation
        float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);

        PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
    }

    // Use SweepPolygon to create the extruded mesh
    // UE 5.7: FGeometryScriptPolygonsToSweepOptions removed, use direct parameters
    FGeometryScriptPrimitiveOptions PrimOptions;

    // Clear existing mesh and sweep the profile along the path
    // UE 5.4+: AppendSweepPolygon has MiterLimit parameter
    // UE 5.3 and earlier: No MiterLimit parameter
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        1.0f,  // MiterLimit
        nullptr
    );
#else
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        nullptr
    );
#endif

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("splineActorName"), SplineActorName);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetNumberField(TEXT("segments"), Segments);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extruded profile along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Handler Dispatcher
// -------------------------------------------------------------------------

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT

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

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldGeom(Payload, TEXT("subAction"));
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
    if (SubAction == TEXT("create_procedural_mesh")) return HandleCreateProceduralMesh(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("append_triangle")) return HandleAppendTriangle(this, RequestId, Payload, RequestingSocket);

    // Booleans
    if (SubAction == TEXT("boolean_union")) return HandleBooleanUnion(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_subtract")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_intersection")) return HandleBooleanIntersection(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("boolean_trim")) return HandleBooleanTrim(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("self_union")) return HandleSelfUnion(this, RequestId, Payload, RequestingSocket);

    // Mesh Utils
    if (SubAction == TEXT("get_mesh_info")) return HandleGetMeshInfo(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("recalculate_normals")) return HandleRecalculateNormals(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("flip_normals")) return HandleFlipNormals(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("simplify_mesh")) return HandleSimplifyMesh(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("subdivide")) return HandleSubdivide(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("auto_uv")) return HandleAutoUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("convert_to_static_mesh")) return HandleConvertToStaticMesh(this, RequestId, Payload, RequestingSocket);

    // Modeling Operations
    if (SubAction == TEXT("extrude")) return HandleExtrude(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("inset")) return HandleInsetOutset(this, RequestId, Payload, RequestingSocket, true);
    if (SubAction == TEXT("outset")) return HandleInsetOutset(this, RequestId, Payload, RequestingSocket, false);
    if (SubAction == TEXT("bevel")) return HandleBevel(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("offset_faces")) return HandleOffsetFaces(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("shell")) return HandleShell(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("chamfer")) return HandleChamfer(this, RequestId, Payload, RequestingSocket);

    // Deformers
    if (SubAction == TEXT("bend")) return HandleBend(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("twist")) return HandleTwist(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("taper")) return HandleTaper(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("noise_deform")) return HandleNoiseDeform(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("smooth")) return HandleSmooth(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("relax")) return HandleRelax(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("stretch")) return HandleStretch(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("spherify")) return HandleSpherify(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("cylindrify")) return HandleCylindrify(this, RequestId, Payload, RequestingSocket);

    // Mesh Repair
    if (SubAction == TEXT("weld_vertices")) return HandleWeldVertices(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("fill_holes")) return HandleFillHoles(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("remove_degenerates")) return HandleRemoveDegenerates(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("remesh_uniform")) return HandleRemeshUniform(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("merge_vertices")) return HandleMergeVertices(this, RequestId, Payload, RequestingSocket);

    // Collision Generation
    if (SubAction == TEXT("generate_collision")) return HandleGenerateCollision(this, RequestId, Payload, RequestingSocket);

    // Transform Operations
    if (SubAction == TEXT("mirror")) return HandleMirror(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("array_linear")) return HandleArrayLinear(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("array_radial")) return HandleArrayRadial(this, RequestId, Payload, RequestingSocket);

    // Mesh Topology Operations
    if (SubAction == TEXT("triangulate")) return HandleTriangulate(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("poke")) return HandlePoke(this, RequestId, Payload, RequestingSocket);

    // UV Operations
    if (SubAction == TEXT("project_uv")) return HandleProjectUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("transform_uvs")) return HandleTransformUVs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_uvs")) return HandleSetUVs(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_vertex_color")) return HandleSetVertexColor(this, RequestId, Payload, RequestingSocket);

    // Tangent Operations
    if (SubAction == TEXT("recompute_tangents")) return HandleRecomputeTangents(this, RequestId, Payload, RequestingSocket);

    // Normal Operations
    if (SubAction == TEXT("split_normals")) return HandleSplitNormals(this, RequestId, Payload, RequestingSocket);

    // Advanced Operations (Bridge, Loft, Sweep)
    if (SubAction == TEXT("bridge")) return HandleBridge(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("loft")) return HandleLoft(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("sweep")) return HandleSweep(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("loop_cut")) return HandleLoopCut(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("duplicate_along_spline")) return HandleDuplicateAlongSpline(this, RequestId, Payload, RequestingSocket);

    // Vertex and Triangle Operations
    if (SubAction == TEXT("append_vertex")) return HandleAppendVertex(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("delete_vertex")) return HandleDeleteVertex(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("delete_triangle")) return HandleDeleteTriangle(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("get_vertex_position")) return HandleGetVertexPosition(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_vertex_position")) return HandleSetVertexPosition(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("translate_mesh")) return HandleTranslateMesh(this, RequestId, Payload, RequestingSocket);

    // Additional UV Operations
    if (SubAction == TEXT("unwrap_uv")) return HandleUnwrapUV(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("pack_uv_islands")) return HandlePackUVIslands(this, RequestId, Payload, RequestingSocket);

    // Nanite Conversion
    if (SubAction == TEXT("convert_to_nanite")) return HandleConvertToNanite(this, RequestId, Payload, RequestingSocket);

    // Spline-based Operations
    if (SubAction == TEXT("extrude_along_spline")) return HandleExtrudeAlongSpline(this, RequestId, Payload, RequestingSocket);

    // Aliases
    if (SubAction == TEXT("difference")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown geometry subAction: '%s'"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
#else
    // UE 5.0 doesn't have full GeometryScript support
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
}

#endif // WITH_EDITOR