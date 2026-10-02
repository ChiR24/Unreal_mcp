#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleExtrude(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
    // extrude declares amount, so it wins over the distance spelling the other face operators use.
    const double Distance = Offset.IsNearlyZero()
        ? GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("distance"), 10.0)) : Offset.Size();
    const FVector Direction = Offset.IsNearlyZero() ? FVector::UpVector : Offset.GetSafeNormal();

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
    ExtrudeOptions.Distance = Distance;
    ExtrudeOptions.Direction = Direction;
    ExtrudeOptions.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, Selection, bHasSelection);

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
        Mesh, ExtrudeOptions, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extrude applied"), Result);
    return true;
}

bool HandleInsetOutset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              bool bIsInset)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = FaceOpDistance(Payload, 5.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshInsetOutsetFacesOptions Options;
    Options.Distance = bIsInset ? -Distance : Distance;  // Negative for inset
    Options.bReproject = true;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrianglesSelected = SelectedTriangleCount(Mesh, Selection, bHasSelection);

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), bIsInset ? TEXT("inset") : TEXT("outset"));
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, bIsInset ? TEXT("Inset applied") : TEXT("Outset applied"), Result);
    return true;
}

bool HandleBevel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double BevelDistance = FaceOpDistance(Payload, 5.0);
    // No segments = a single flat chamfer (the chamfer action routes here).
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("segments"), 0);
#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
    if (Subdivisions > 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("segments (rounded bevel subdivisions) needs UE 5.4 or later; omit it for a flat bevel."), TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = BevelDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    BevelOptions.Subdivisions = Subdivisions;
#endif

    FGeometryScriptMeshSelection BevelSelection;
    bool bHasBevelSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, BevelSelection, bHasBevelSelection)) return true;
    // Before UE 5.2 the polygroup bevel takes no selection, so it always works the whole mesh.
    int32 TrianglesSelected = Mesh->GetTriangleCount();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    TrianglesSelected = SelectedTriangleCount(Mesh, BevelSelection, bHasBevelSelection);
    if (bHasBevelSelection)
    {
        FGeometryScriptMeshBevelSelectionOptions SelectionOptions;
        SelectionOptions.BevelDistance = BevelOptions.BevelDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        SelectionOptions.Subdivisions = Subdivisions;
#endif
        UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshBevelSelection(
            Mesh, BevelSelection, EGeometryScriptMeshBevelSelectionMode::TriangleArea, SelectionOptions, nullptr);
    }
    else
#endif
    {
        UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
            Mesh, BevelOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), BevelDistance);
    Result->SetNumberField(TEXT("segments"), Subdivisions);
    Result->SetNumberField(TEXT("trianglesSelected"), TrianglesSelected);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bevel applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
