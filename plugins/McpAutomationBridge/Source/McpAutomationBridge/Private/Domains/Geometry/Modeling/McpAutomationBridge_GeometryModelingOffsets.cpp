#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleOffsetFaces(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions Options;
    Options.Distance = Distance;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    FString SelectionError;
    if (!McpBuildTriangleSelection(Mesh, Payload, Selection, bHasSelection, SelectionError))
    {
        Self->SendAutomationError(Socket, RequestId, SelectionError, TEXT("INVALID_SELECTION"));
        return true;
    }

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Offset faces applied"), Result);
    return true;
}

bool HandleShell(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Thickness = GetJsonNumberField(Payload, TEXT("thickness"), 5.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptMeshOffsetOptions Options;
    Options.OffsetDistance = -Thickness;  // Negative to go inward for shell

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("thickness"), Thickness);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Shell/solidify applied"), Result);
    return true;
}

bool HandleChamfer(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);
    int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 1);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // Chamfer is similar to bevel but with flat (1-step) result
    // Use bevel with steps=1 for chamfer effect
    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = Distance;
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
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("steps"), Steps);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Chamfer applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
