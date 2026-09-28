#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateTorus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bArch)
{
    // An arch is a torus revolved part of the way round.
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = bArch ? TEXT("GeneratedArch") : TEXT("GeneratedTorus");

    FTransform Transform = ReadTransformFromPayload(Payload);
    const double MajorRadius = GetJsonNumberField(Payload, TEXT("radius"), bArch ? 100.0 : 50.0);
    const double MinorRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), bArch ? 25.0 : 20.0);
    const int32 MajorSegments = DeclaredSegments(Payload, {TEXT("numRings"), TEXT("radialSegments")}, 16);
    const int32 MinorSegments = DeclaredSegments(Payload, {TEXT("numSides")}, 8);
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = GetJsonNumberField(Payload, TEXT("angle"), bArch ? 180.0 : 360.0);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, RevolveOptions,
        MajorRadius, MinorRadius, MajorSegments, MinorSegments,
        EGeometryScriptPrimitiveOriginMode::Center, nullptr);

    TSharedPtr<FJsonObject> Result;
    if (!SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result)) return true;
    Result->SetNumberField(TEXT("majorRadius"), MajorRadius);
    Result->SetNumberField(TEXT("angle"), RevolveOptions.RevolveDegrees);
    Self->SendAutomationResponse(Socket, RequestId, true, bArch ? TEXT("Arch created") : TEXT("Torus mesh created"), Result);
    return true;
}

bool HandleCreatePlane(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedPlane");

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = ClampDimension(GetJsonNumberField(Payload, TEXT("width"), 100.0));
    double Height = ClampDimension(GetJsonNumberField(Payload, TEXT("depth"), GetJsonNumberField(Payload, TEXT("height"), 100.0)));
    int32 WidthSubdivisions = ClampSegments(GetJsonIntField(Payload, TEXT("widthSegments"), 1));
    int32 HeightSubdivisions = ClampSegments(GetJsonIntField(Payload, TEXT("heightSegments"), 1));

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(
        DynMesh,
        Options,
        FTransform::Identity,
        Width, Height,
        WidthSubdivisions, HeightSubdivisions,
        nullptr
    );

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("widthSegments"), WidthSubdivisions);
    Result->SetNumberField(TEXT("heightSegments"), HeightSubdivisions);

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Plane mesh created"), Result);
    return true;
}

bool HandleCreateDisc(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bRing)
{
    // A ring is a disc with a hole.
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = bRing ? TEXT("GeneratedRing") : TEXT("GeneratedDisc");

    FTransform Transform = ReadTransformFromPayload(Payload);
    // The declared name wins: outerRadius for a ring, radius for a disc.
    const TCHAR* const RadiusField = bRing ? TEXT("outerRadius") : TEXT("radius");
    const double Radius = GetJsonNumberField(Payload, RadiusField, GetJsonNumberField(Payload, bRing ? TEXT("radius") : TEXT("outerRadius"), 50.0));
    const double HoleRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), bRing ? 25.0 : 0.0);
    const int32 Segments = DeclaredSegments(Payload, {TEXT("numSides"), TEXT("radialSegments")}, bRing ? 32 : 16);

    // Local space: the actor transform is the single source of placement.
    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, Radius, Segments, 1, 0.0f, 360.0f, HoleRadius, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor) return true;
    Result->SetNumberField(TEXT("outerRadius"), Radius);
    Result->SetNumberField(TEXT("innerRadius"), HoleRadius);
    McpHandlerUtils::AddVerification(Result, NewActor);
    Self->SendAutomationResponse(Socket, RequestId, true, bRing ? TEXT("Ring created") : TEXT("Disc mesh created"), Result);
    return true;
}

bool HandleCreateStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
    float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float StepDepth = GetJsonNumberField(Payload, TEXT("stepDepth"), 30.0f);
    int32 NumSteps = GetJsonIntField(Payload, TEXT("numSteps"), GetJsonIntField(Payload, TEXT("steps"), 8));
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(
        DynMesh, Options, FTransform::Identity, StepWidth, StepHeight, StepDepth, NumSteps, bFloating, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("numSteps"), NumSteps);

    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear stairs created"), Result);
    return true;
}

bool HandleCreateSpiralStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSpiralStairs");

    FTransform Transform = ReadTransformFromPayload(Payload);
    float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), GetJsonNumberField(Payload, TEXT("radius"), 150.0));
    // numTurns is the declared spelling; no turns = a quarter turn.
    const float NumTurns = GetJsonNumberField(Payload, TEXT("numTurns"), 0.0f);
    const float CurveAngle = NumTurns > 0.0f ? NumTurns * 360.0f : 90.0f;
    int32 NumSteps = GetJsonIntField(Payload, TEXT("numSteps"), GetJsonIntField(Payload, TEXT("steps"), 8));
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCurvedStairs(
        DynMesh, Options, FTransform::Identity, StepWidth, StepHeight, InnerRadius, CurveAngle, NumSteps, bFloating, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Transform, Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("numSteps"), NumSteps);
    Result->SetNumberField(TEXT("curveAngle"), CurveAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spiral stairs created"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
