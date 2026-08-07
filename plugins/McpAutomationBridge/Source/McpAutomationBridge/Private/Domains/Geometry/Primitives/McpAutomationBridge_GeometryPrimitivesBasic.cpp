#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

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
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Box segment counts would create too many triangles: %lld"), EstimatedTriangles),
            TEXT("TOO_MANY_TRIANGLES"));
        return true;
    }

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high for geometry creation: %.1f%%"), GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

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

    // Spawn actor with dynamic mesh component. Use direct world spawning so
    // headless/NullRHI automation does not enter viewport hit-proxy placement.
    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("depth"), Depth);
    Result->SetNumberField(TEXT("estimatedTriangles"), static_cast<double>(EstimatedTriangles));
    Result->SetBoolField(TEXT("dimensionsClamped"), bDimensionsClamped);

    // Add verification data
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
    int32 Subdivisions = ClampSegments(GetJsonIntField(Payload, TEXT("subdivisions"), 16), 16);

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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("radius"), Radius);

    // Add verification data
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
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for cylinder") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));

    // Add verification data
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
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

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

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);

    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for cone") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), Name);

    // Add verification data
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
double Length = GetJsonNumberField(Payload, TEXT("length"), 100.0);
    int32 HemisphereSteps = GetJsonIntField(Payload, TEXT("hemisphereSteps"), 4);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
        DynMesh,
        Options,
        Transform,
        Radius, Length,
        HemisphereSteps, Segments,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        0,  // SegmentSteps parameter added in UE 5.5
#endif
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);

    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for capsule") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), Name);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule mesh created"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
