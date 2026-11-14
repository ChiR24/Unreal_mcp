#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateBox(UEngineWireSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // dimensions {x, y, z}, key by key, over width, height and depth.
    const FVector Size = ExtractVectorField(Payload, TEXT("dimensions"),
                                            FVector(GetJsonNumberField(Payload, TEXT("width"), 100.0),
                                                    GetJsonNumberField(Payload, TEXT("height"), 100.0),
                                                    GetJsonNumberField(Payload, TEXT("depth"), 100.0)));
    double Width = Size.X;
    double Height = Size.Y;
    double Depth = Size.Z;

    if (Width <= 0.0 || Height <= 0.0 || Depth <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Box dimensions must be positive"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const bool bDimensionsClamped = Width > MAX_DIMENSION || Height > MAX_DIMENSION || Depth > MAX_DIMENSION;
    Width = ClampDimension(Width);
    Height = ClampDimension(Height);
    Depth = ClampDimension(Depth);

    int32 WidthSegments = ClampSegments(GetJsonIntField(Payload, TEXT("widthSegments"), 1));
    int32 HeightSegments = ClampSegments(GetJsonIntField(Payload, TEXT("heightSegments"), 1));
    int32 DepthSegments = ClampSegments(GetJsonIntField(Payload, TEXT("depthSegments"), 1));

    const int64 EstimatedTriangles = GridBoxTriangles(WidthSegments, HeightSegments, DepthSegments);
    if (!GuardMeshBudget(Self, RequestId, Socket, EstimatedTriangles, TEXT("Box creation"))) return true;

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());

    FGeometryScriptPrimitiveOptions Options;
    Options.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
        DynMesh,
        Options,
        // Build in LOCAL space: the actor transform below is the single source
        // of placement. Baking Transform here AND setting it on the actor
        // double-placed every primitive (location, rotation AND scale applied
        // twice) — e.g. a box asked at (3000,3000,100) effectively landed at
        // (6000,6000,200), which also broke boolean overlap tests downstream.
        FTransform::Identity,
        Width, Height, Depth,
        WidthSegments, HeightSegments, DepthSegments,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    // Spawn actor with dynamic mesh component. Use direct world spawning so
    // headless/NullRHI automation does not enter viewport hit-proxy placement.
    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedBox"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("depth"), Depth);
    Result->SetNumberField(TEXT("estimatedTriangles"), static_cast<double>(EstimatedTriangles));
    Result->SetBoolField(TEXT("dimensionsClamped"), bDimensionsClamped);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Box mesh created"), Result);
    return true;
}

bool HandleCreateSphere(UEngineWireSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    int32 Subdivisions = DeclaredSegments(Payload, {TEXT("numRings"), TEXT("radialSegments")}, 16);
    if (!GuardMeshBudget(Self, RequestId, Socket, GridBoxTriangles(Subdivisions, Subdivisions, Subdivisions), TEXT("Sphere creation"))) return true;

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
        DynMesh,
        Options,
        FTransform::Identity,
        Radius,
        Subdivisions, Subdivisions, Subdivisions,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedSphere"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("radius"), Radius);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sphere mesh created"), Result);
    return true;
}

bool HandleCreateCylinder(UEngineWireSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = DeclaredSegments(Payload, {TEXT("numSides")}, 16);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh,
        Options,
        FTransform::Identity,
        Radius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedCylinder"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylinder mesh created"), Result);
    return true;
}

bool HandleCreateCone(UEngineWireSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // `radius` is the spelling every other round primitive here uses (and the
    // one callers reach for); reading only `baseRadius` meant a `radius` on a
    // cone was accepted and silently discarded, leaving the 50-unit default.
    const double DefaultBaseRadius =
        GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double BaseRadius =
        GetJsonNumberField(Payload, TEXT("baseRadius"), DefaultBaseRadius);
    double TopRadius = GetJsonNumberField(Payload, TEXT("topRadius"), 0.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = DeclaredSegments(Payload, {TEXT("numSides")}, 16);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
        DynMesh,
        Options,
        FTransform::Identity,
        BaseRadius, TopRadius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedCone"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cone mesh created"), Result);
    return true;
}

bool HandleCreateCapsule(UEngineWireSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double Length = GetJsonNumberField(Payload, TEXT("length"), GetJsonNumberField(Payload, TEXT("height"), 100.0));
    int32 HemisphereSteps = DeclaredSegments(Payload, {TEXT("numRings")}, 4);
    int32 Segments = DeclaredSegments(Payload, {TEXT("radialSegments")}, 16);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
        DynMesh,
        Options,
        FTransform::Identity,
        Radius, Length,
        HemisphereSteps, Segments,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        DeclaredSegments(Payload, {TEXT("heightSegments")}, 1) - 1,  // SegmentSteps parameter added in UE 5.5
#endif
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedCapsule"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule mesh created"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
