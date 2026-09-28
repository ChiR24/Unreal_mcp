#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateBox(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedBox");

    FTransform Transform = ReadTransformFromPayload(Payload);

    double Width = GetJsonNumberField(Payload, TEXT("width"), 100.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    double Depth = GetJsonNumberField(Payload, TEXT("depth"), 100.0);

    const TSharedPtr<FJsonObject>* DimensionsObject = nullptr;
    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("dimensions"), DimensionsObject) && DimensionsObject && DimensionsObject->IsValid())
    {
        // The published schema documents `dimensions` as {x, y, z} — reading
        // only width/height/depth meant a contract-correct call was ACCEPTED and
        // then silently ignored, leaving the default 100-unit primitive. Nothing
        // reported the discard, and get_actor_bounds returned no bounds, so the
        // wrong size was undetectable through the API. Accept both spellings.
        (*DimensionsObject)->TryGetNumberField(TEXT("width"), Width);
        (*DimensionsObject)->TryGetNumberField(TEXT("height"), Height);
        (*DimensionsObject)->TryGetNumberField(TEXT("depth"), Depth);
        (*DimensionsObject)->TryGetNumberField(TEXT("x"), Width);
        (*DimensionsObject)->TryGetNumberField(TEXT("y"), Height);
        (*DimensionsObject)->TryGetNumberField(TEXT("z"), Depth);
    }

    const TArray<TSharedPtr<FJsonValue>>* Dimensions = nullptr;
    if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("dimensions"), Dimensions) && Dimensions && Dimensions->Num() >= 3)
    {
        Width = (*Dimensions)[0]->AsNumber();
        Height = (*Dimensions)[1]->AsNumber();
        Depth = (*Dimensions)[2]->AsNumber();
    }

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

    const int64 EstimatedTriangles = 2LL * (static_cast<int64>(WidthSegments) * HeightSegments +
                                            static_cast<int64>(WidthSegments) * DepthSegments +
                                            static_cast<int64>(HeightSegments) * DepthSegments);
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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("depth"), Depth);
    Result->SetNumberField(TEXT("estimatedTriangles"), static_cast<double>(EstimatedTriangles));
    Result->SetBoolField(TEXT("dimensionsClamped"), bDimensionsClamped);

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Box mesh created"), Result);
    return true;
}

bool HandleCreateSphere(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSphere");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    int32 Subdivisions = DeclaredSegments(Payload, {TEXT("numRings"), TEXT("radialSegments")}, 16);

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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("radius"), Radius);

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sphere mesh created"), Result);
    return true;
}

bool HandleCreateCylinder(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCylinder");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = DeclaredSegments(Payload, {TEXT("numSides"), TEXT("radialSegments")}, 16);

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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylinder mesh created"), Result);
    return true;
}

bool HandleCreateCone(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCone");

    FTransform Transform = ReadTransformFromPayload(Payload);
    // `radius` is the spelling every other round primitive here uses (and the
    // one callers reach for); reading only `baseRadius` meant a `radius` on a
    // cone was accepted and silently discarded, leaving the 50-unit default.
    const double DefaultBaseRadius =
        GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double BaseRadius =
        GetJsonNumberField(Payload, TEXT("baseRadius"), DefaultBaseRadius);
    double TopRadius = GetJsonNumberField(Payload, TEXT("topRadius"), 0.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = DeclaredSegments(Payload, {TEXT("numSides"), TEXT("radialSegments")}, 16);

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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cone mesh created"), Result);
    return true;
}

bool HandleCreateCapsule(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedCapsule");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double Length = GetJsonNumberField(Payload, TEXT("length"), GetJsonNumberField(Payload, TEXT("height"), 100.0));
    int32 HemisphereSteps = DeclaredSegments(Payload, {TEXT("numRings")}, 4);
    // radialSegments is the declared capsule name, so it wins over the numSides fallback.
    int32 Segments = DeclaredSegments(Payload, {TEXT("radialSegments"), TEXT("numSides")}, 16);

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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule mesh created"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
