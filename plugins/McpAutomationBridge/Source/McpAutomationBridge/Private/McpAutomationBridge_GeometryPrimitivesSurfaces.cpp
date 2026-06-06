#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateTorus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Torus mesh created"), Result);
    return true;
}

/**
 * Create a DynamicMesh plane from the current schema fields and legacy aliases.
 *
 * Reads `width`, `height`, `widthSegments`, and `heightSegments` from the payload,
 * while preserving `depth`, `widthSubdivisions`, and `depthSubdivisions` as
 * compatibility fallbacks. Returns the effective dimensions and segment counts
 * so clients can verify which values were applied.
 */

bool HandleCreatePlane(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPlane");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = ClampDimension(GetNumberFieldGeom(Payload, TEXT("width"), 100.0));
    double Height = ClampDimension(GetNumberFieldGeom(
        Payload, TEXT("height"), GetNumberFieldGeom(Payload, TEXT("depth"), 100.0)));
    int32 WidthSubdivisions = ClampSegments(GetIntFieldGeom(
        Payload, TEXT("widthSegments"), GetIntFieldGeom(Payload, TEXT("widthSubdivisions"), 1)));
    int32 HeightSubdivisions = ClampSegments(GetIntFieldGeom(
        Payload, TEXT("heightSegments"), GetIntFieldGeom(Payload, TEXT("depthSubdivisions"), 1)));

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(
        DynMesh,
        Options,
        Transform,
        Width, Height,
        WidthSubdivisions, HeightSubdivisions,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("widthSegments"), WidthSubdivisions);
    Result->SetNumberField(TEXT("heightSegments"), HeightSubdivisions);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Plane mesh created"), Result);
    return true;
}

bool HandleCreateDisc(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Disc mesh created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Booleans
// -------------------------------------------------------------------------

bool HandleCreateStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear stairs created"), Result);
    return true;
}

bool HandleCreateSpiralStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);
    Result->SetNumberField(TEXT("curveAngle"), CurveAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spiral stairs created"), Result);
    return true;
}

bool HandleCreateRing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ring created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Modeling Operations
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
