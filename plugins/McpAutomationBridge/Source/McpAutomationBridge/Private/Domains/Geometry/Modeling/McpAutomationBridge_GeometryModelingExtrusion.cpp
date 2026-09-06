#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleExtrude(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    // amount/offset are the documented spellings; distance stays as the legacy alias (dogfood #137).
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("offset"), 10.0)));
    FVector Direction = ReadVectorFromPayload(Payload, TEXT("direction"), FVector(0, 0, 1));

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
    ExtrudeOptions.Distance = Distance;
    ExtrudeOptions.Direction = Direction;
    ExtrudeOptions.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    FString SelectionError;
    if (!McpBuildTriangleSelection(Mesh, Payload, Selection, bHasSelection, SelectionError))
    {
        Self->SendAutomationError(Socket, RequestId, SelectionError, TEXT("INVALID_SELECTION"));
        return true;
    }

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
        Mesh, ExtrudeOptions, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extrude applied"), Result);
    return true;
}

bool HandleInsetOutset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              bool bIsInset)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("offset"), 5.0)));

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptMeshInsetOutsetFacesOptions Options;
    Options.Distance = bIsInset ? -Distance : Distance;  // Negative for inset
    Options.bReproject = true;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    FString SelectionError;
    if (!McpBuildTriangleSelection(Mesh, Payload, Selection, bHasSelection, SelectionError))
    {
        Self->SendAutomationError(Socket, RequestId, SelectionError, TEXT("INVALID_SELECTION"));
        return true;
    }

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), bIsInset ? TEXT("inset") : TEXT("outset"));
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, bIsInset ? TEXT("Inset applied") : TEXT("Outset applied"), Result);
    return true;
}

bool HandleBevel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double BevelDistance = GetJsonNumberField(Payload, TEXT("distance"), GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("offset"), 5.0)));
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("subdivisions"), 0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = BevelDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    BevelOptions.Subdivisions = Subdivisions;
#endif

    FGeometryScriptMeshSelection BevelSelection;
    bool bHasBevelSelection = false;
    FString BevelSelectionError;
    if (!McpBuildTriangleSelection(Mesh, Payload, BevelSelection, bHasBevelSelection, BevelSelectionError))
    {
        Self->SendAutomationError(Socket, RequestId, BevelSelectionError, TEXT("INVALID_SELECTION"));
        return true;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    if (bHasBevelSelection)
    {
        FGeometryScriptMeshBevelSelectionOptions SelectionOptions;
        SelectionOptions.BevelDistance = BevelOptions.BevelDistance;
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
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bevel applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
