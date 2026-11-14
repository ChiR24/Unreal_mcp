#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateTorus(UEngineWireSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bArch)
{
    // An arch is a torus revolved part of the way round.
    const double MajorRadius = GetJsonNumberField(Payload, TEXT("radius"), bArch ? 100.0 : 50.0);
    const double MinorRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), bArch ? 25.0 : 20.0);
    const int32 MajorSegments = DeclaredSegments(Payload, {TEXT("numRings"), TEXT("radialSegments")}, 16);
    const int32 MinorSegments = DeclaredSegments(Payload, {TEXT("numSides")}, 8);
    FGeometryScriptRevolveOptions RevolveOptions;
    // 1 to 360 degrees, as create_revolve takes it: 0 made a degenerate mesh, 720 wound the ring twice.
    RevolveOptions.RevolveDegrees = FMath::Clamp(GetJsonNumberField(Payload, TEXT("angle"), bArch ? 180.0 : 360.0), 1.0, 360.0);

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, RevolveOptions,
        MajorRadius, MinorRadius, MajorSegments, MinorSegments,
        EGeometryScriptPrimitiveOriginMode::Center, nullptr);

    TSharedPtr<FJsonObject> Result;
    if (!SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, bArch ? TEXT("GeneratedArch") : TEXT("GeneratedTorus"), DynMesh, Result)) return true;
    Result->SetNumberField(TEXT("majorRadius"), MajorRadius);
    Result->SetNumberField(TEXT("angle"), RevolveOptions.RevolveDegrees);
    Self->SendAutomationResponse(Socket, RequestId, true, bArch ? TEXT("Arch created") : TEXT("Torus mesh created"), Result);
    return true;
}

bool HandleCreatePlane(UEngineWireSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    double Width = ClampDimension(GetJsonNumberField(Payload, TEXT("width"), 100.0));
    double Height = ClampDimension(GetJsonNumberField(Payload, TEXT("depth"), 100.0));
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
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedPlane"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("depth"), Height);
    Result->SetNumberField(TEXT("widthSegments"), WidthSubdivisions);
    Result->SetNumberField(TEXT("heightSegments"), HeightSubdivisions);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Plane mesh created"), Result);
    return true;
}

bool HandleCreateDisc(UEngineWireSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bRing)
{
    // A ring is a disc with a hole.
    // Each reads its declared name: outerRadius for a ring, radius for a disc.
    const double Radius = GetJsonNumberField(Payload, bRing ? TEXT("outerRadius") : TEXT("radius"), 50.0);
    const double HoleRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), bRing ? 25.0 : 0.0);
    const int32 Segments = DeclaredSegments(Payload, {TEXT("numSides")}, bRing ? 32 : 16);

    // Local space: the actor transform is the single source of placement.
    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh, FGeometryScriptPrimitiveOptions(), FTransform::Identity, Radius, Segments, 1, 0.0f, 360.0f, HoleRadius, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, bRing ? TEXT("GeneratedRing") : TEXT("GeneratedDisc"), DynMesh, Result);
    if (!NewActor) return true;
    Result->SetNumberField(TEXT("outerRadius"), Radius);
    Result->SetNumberField(TEXT("innerRadius"), HoleRadius);
    Self->SendAutomationResponse(Socket, RequestId, true, bRing ? TEXT("Ring created") : TEXT("Disc mesh created"), Result);
    return true;
}

// What the engine's stair generator makes for a step count: a solid staircase fills its two sides step by step
// (2N*N + 10N triangles), a floating one is 16N - 4. numSteps has no bound of its own, and 10000 solid steps are 200
// million triangles, so the count is checked against the mesh budget before the append.
static bool GuardStairBudget(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, int64 Steps, bool bFloating)
{
    return GuardMeshBudget(Self, RequestId, Socket, bFloating ? 16 * Steps - 4 : 2 * Steps * Steps + 10 * Steps, TEXT("Stairs"));
}

bool HandleCreateStairs(UEngineWireSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float StepDepth = GetJsonNumberField(Payload, TEXT("stepDepth"), 30.0f);
    // The engine builds at least one step: the count it uses is the one checked and answered.
    const int32 NumSteps = FMath::Max(1, GetJsonIntField(Payload, TEXT("numSteps"), GetJsonIntField(Payload, TEXT("steps"), 8)));
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);
    if (!GuardStairBudget(Self, RequestId, Socket, NumSteps, bFloating)) return true;

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(
        DynMesh, Options, FTransform::Identity, StepWidth, StepHeight, StepDepth, NumSteps, bFloating, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedStairs"), DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("numSteps"), NumSteps);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear stairs created"), Result);
    return true;
}

bool HandleCreateSpiralStairs(UEngineWireSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), GetJsonNumberField(Payload, TEXT("radius"), 150.0));
    // numTurns is the declared spelling; no turns = a quarter turn.
    const float NumTurns = GetJsonNumberField(Payload, TEXT("numTurns"), 0.0f);
    const float CurveAngle = NumTurns > 0.0f ? NumTurns * 360.0f : 90.0f;
    // The engine builds at least one step: the count it uses is the one checked and answered.
    const int32 NumSteps = FMath::Max(1, GetJsonIntField(Payload, TEXT("numSteps"), GetJsonIntField(Payload, TEXT("steps"), 8)));
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);
    if (!GuardStairBudget(Self, RequestId, Socket, NumSteps, bFloating)) return true;

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCurvedStairs(
        DynMesh, Options, FTransform::Identity, StepWidth, StepHeight, InnerRadius, CurveAngle, NumSteps, bFloating, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, Payload, TEXT("GeneratedSpiralStairs"), DynMesh, Result);
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
