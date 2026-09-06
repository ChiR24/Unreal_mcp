#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSpherify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Factor = GetJsonNumberField(Payload, TEXT("factor"), 1.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    double TargetRadius = BBox.GetExtent().GetMax();

    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);

    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;

    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);

        if (bIsValid)
        {
            FVector Direction = OriginalPos - Center;
            double CurrentDistance = Direction.Size();

            if (CurrentDistance > KINDA_SMALL_NUMBER)
            {
                Direction.Normalize();

                FVector SpherePos = Center + Direction * TargetRadius;

                FVector NewPos = FMath::Lerp(OriginalPos, SpherePos, FMath::Clamp(Factor, 0.0, 1.0));

                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }

    // Recompute normals after vertex modifications
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spherify applied"), Result);
    return true;
}

bool HandleCylindrify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetJsonNumberField(Payload, TEXT("factor"), 1.0);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // Determine axis index (0=X, 1=Y, 2=Z)
    int32 AxisIndex = 2; // Default to Z
    if (Axis == TEXT("X")) AxisIndex = 0;
    else if (Axis == TEXT("Y")) AxisIndex = 1;

    // Calculate bounding box center and average perpendicular radius from axis
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();

    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);

    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;

    // First pass: compute average radius perpendicular to the cylinder axis
    double TotalRadius = 0.0;
    int32 ValidVertexCount = 0;

    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector Pos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);

        if (bIsValid)
        {
            FVector FromCenter = Pos - Center;
            FVector Perpendicular = FromCenter;
            // Zero out the axis component to get perpendicular vector
            if (AxisIndex == 0) Perpendicular.X = 0;
            else if (AxisIndex == 1) Perpendicular.Y = 0;
            else Perpendicular.Z = 0;

            double PerpDist = Perpendicular.Size();
            TotalRadius += PerpDist;
            ValidVertexCount++;
        }
    }

    double AvgRadius = ValidVertexCount > 0 ? TotalRadius / ValidVertexCount : 1.0;
    if (AvgRadius < KINDA_SMALL_NUMBER) AvgRadius = 1.0;

    // Second pass: project each vertex to cylinder surface
    int32 VerticesModified = 0;

    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);

        if (bIsValid)
        {
            FVector FromCenter = OriginalPos - Center;
            FVector Perpendicular = FromCenter;
            double AxisCoord = 0.0;

            if (AxisIndex == 0) { AxisCoord = FromCenter.X; Perpendicular.X = 0; }
            else if (AxisIndex == 1) { AxisCoord = FromCenter.Y; Perpendicular.Y = 0; }
            else { AxisCoord = FromCenter.Z; Perpendicular.Z = 0; }

            double PerpDist = Perpendicular.Size();

            if (PerpDist > KINDA_SMALL_NUMBER)
            {
                Perpendicular.Normalize();
                FVector CylinderPos = Center + Perpendicular * AvgRadius;

                // Restore the axis coordinate (keep height/depth along axis)
                if (AxisIndex == 0) CylinderPos.X = Center.X + AxisCoord;
                else if (AxisIndex == 1) CylinderPos.Y = Center.Y + AxisCoord;
                else CylinderPos.Z = Center.Z + AxisCoord;

                FVector NewPos = FMath::Lerp(OriginalPos, CylinderPos, FMath::Clamp(Factor, 0.0, 1.0));

                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }

    // Recompute normals after vertex modifications
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Result->SetNumberField(TEXT("avgRadius"), AvgRadius);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylindrify applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
