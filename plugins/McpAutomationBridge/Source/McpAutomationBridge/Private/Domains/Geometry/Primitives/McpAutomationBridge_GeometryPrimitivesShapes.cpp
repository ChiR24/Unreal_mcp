#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreatePipe(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPipe");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double OuterRadius = GetJsonNumberField(Payload, TEXT("outerRadius"), GetJsonNumberField(Payload, TEXT("radius"), 50.0));
    double InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), 40.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 RadialSteps = DeclaredSegments(Payload, {TEXT("numSides"), TEXT("radialSegments")}, 24);
    int32 HeightSteps = DeclaredSegments(Payload, {TEXT("heightSegments")}, 1);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create outer cylinder (local space; the actor transform places it —
    // baking Transform here as well double-placed the mesh).
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh, Options, FTransform::Identity, OuterRadius, Height, RadialSteps, HeightSteps, false,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Create inner cylinder for boolean subtraction
    UDynamicMesh* InnerMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        InnerMesh, Options, FTransform::Identity, InnerRadius, Height + 1.0, RadialSteps, HeightSteps, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Boolean subtract to create hollow pipe
    FGeometryScriptMeshBooleanOptions BoolOptions;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        DynMesh, FTransform::Identity, InnerMesh, FTransform::Identity,
        EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Pipe created"), Result);
    return true;
}

bool HandleCreateRamp(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRamp");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = GetJsonNumberField(Payload, TEXT("width"), 100.0);
    double Length = GetJsonNumberField(Payload, TEXT("length"), 200.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 50.0);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create ramp by extruding a right triangle polygon
    TArray<FVector2D> RampPolygon;
    RampPolygon.Add(FVector2D(0, 0));           // Bottom front
    RampPolygon.Add(FVector2D(Length, 0));      // Bottom back
    RampPolygon.Add(FVector2D(Length, Height)); // Top back

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
        DynMesh, Options, FTransform::Identity, RampPolygon, Width, 0, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("length"), Length);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ramp created"), Result);
    return true;
}

bool HandleRevolve(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // model_mesh declares only steps for revolve: a fixed vase profile turned a full circle.
    const int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 16);
    const TArray<FVector2D> ProfilePoints = {{10, 0}, {30, 0}, {50, 25}, {50, 75}, {30, 100}, {10, 100}};

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePath(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, ProfilePoints, FGeometryScriptRevolveOptions(), Steps, true, nullptr);

    TSharedPtr<FJsonObject> Result;
    if (!SpawnPrimitiveOrReply(Self, RequestId, Socket, FTransform::Identity, TEXT("GeneratedRevolve"), DynMesh, Result)) return true;
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("profilePoints"), ProfilePoints.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Revolve created"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
