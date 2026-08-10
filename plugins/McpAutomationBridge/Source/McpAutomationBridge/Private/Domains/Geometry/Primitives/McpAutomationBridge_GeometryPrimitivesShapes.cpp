#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateArch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedArch");

    FTransform Transform = ReadTransformFromPayload(Payload);
double MajorRadius = GetJsonNumberField(Payload, TEXT("majorRadius"), 100.0);
    double MinorRadius = GetJsonNumberField(Payload, TEXT("minorRadius"), 25.0);
    double ArchAngle = GetJsonNumberField(Payload, TEXT("angle"), 180.0);
    int32 MajorSteps = GetJsonIntField(Payload, TEXT("majorSteps"), 16);
    int32 MinorSteps = GetJsonIntField(Payload, TEXT("minorSteps"), 8);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create partial torus (arch) using revolve options
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = ArchAngle;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh, Options, Transform, RevolveOptions, MajorRadius, MinorRadius, MajorSteps, MinorSteps,
        EGeometryScriptPrimitiveOriginMode::Center, nullptr);

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
    Result->SetNumberField(TEXT("majorRadius"), MajorRadius);
    Result->SetNumberField(TEXT("angle"), ArchAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Arch created"), Result);
    return true;
}

bool HandleCreatePipe(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPipe");

    FTransform Transform = ReadTransformFromPayload(Payload);
double OuterRadius = GetJsonNumberField(Payload, TEXT("outerRadius"), 50.0);
    double InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), 40.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 RadialSteps = GetJsonIntField(Payload, TEXT("radialSteps"), 24);
    int32 HeightSteps = GetJsonIntField(Payload, TEXT("heightSteps"), 1);

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
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("length"), Length);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ramp created"), Result);
    return true;
}

bool HandleRevolve(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedRevolve");

    FTransform Transform = ReadTransformFromPayload(Payload);
double Angle = GetJsonNumberField(Payload, TEXT("angle"), 360.0);
    int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 16);
    bool bCapped = GetJsonBoolField(Payload, TEXT("capped"), true);

    TArray<FVector2D> ProfilePoints;
    if (Payload->HasField(TEXT("profile")))
    {
        const TArray<TSharedPtr<FJsonValue>>& PointsArray = Payload->GetArrayField(TEXT("profile"));
        for (const TSharedPtr<FJsonValue>& PointValue : PointsArray)
        {
            const TSharedPtr<FJsonObject>& PointObj = PointValue->AsObject();
            if (PointObj.IsValid())
            {
double X = GetJsonNumberField(PointObj, TEXT("x"), 0.0);
                double Y = GetJsonNumberField(PointObj, TEXT("y"), 0.0);
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
    Result->SetNumberField(TEXT("angle"), Angle);
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("profilePoints"), ProfilePoints.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Revolve created"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
