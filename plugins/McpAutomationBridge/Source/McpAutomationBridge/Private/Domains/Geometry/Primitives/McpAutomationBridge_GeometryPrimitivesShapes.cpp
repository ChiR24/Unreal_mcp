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
    // The profile runs bottom to top as {radius, height}; it used to be a fixed vase whatever was asked.
    TArray<FVector2D> ProfilePoints;
    const TArray<TSharedPtr<FJsonValue>>* Profile = nullptr;
    if (Payload->TryGetArrayField(TEXT("profile"), Profile))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Profile)
        {
            const TSharedPtr<FJsonObject>* Point = nullptr;
            double Radius = 0.0, Height = 0.0;
            if (!Value.IsValid() || !Value->TryGetObject(Point) || !(*Point)->TryGetNumberField(TEXT("radius"), Radius) ||
                !(*Point)->TryGetNumberField(TEXT("height"), Height) || Radius < 0.0)
            {
                Self->SendAutomationError(Socket, RequestId, TEXT("Each profile point must be {radius, height} with radius 0 or more."), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            ProfilePoints.Add(FVector2D(Radius, Height));
        }
        if (ProfilePoints.Num() < 2)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("A profile needs at least 2 points."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    const bool bDefaultProfile = ProfilePoints.Num() == 0;
    if (bDefaultProfile) ProfilePoints = {{10, 0}, {30, 0}, {50, 25}, {50, 75}, {30, 100}, {10, 100}};

    const int32 Steps = FMath::Clamp(GetJsonIntField(Payload, TEXT("steps"), 16), 3, 512);
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = FMath::Clamp(GetJsonNumberField(Payload, TEXT("angle"), 360.0), 1.0, 360.0);
    bool bCap = true;
    Payload->TryGetBoolField(TEXT("cap"), bCap);
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRevolve");

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePath(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, ProfilePoints, RevolveOptions, Steps, bCap, nullptr);

    TSharedPtr<FJsonObject> Result;
    if (!SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result)) return true;
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("profilePoints"), ProfilePoints.Num());
    Result->SetNumberField(TEXT("angle"), RevolveOptions.RevolveDegrees);
    Result->SetBoolField(TEXT("capped"), bCap);
    Result->SetBoolField(TEXT("usedDefaultProfile"), bDefaultProfile);
    Self->SendAutomationResponse(Socket, RequestId, true,
        bDefaultProfile ? TEXT("Revolve created from the built-in vase profile, since no profile was given") : TEXT("Revolve created"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
