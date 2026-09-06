#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("X")).ToUpper();
    bool bWeld = GetJsonBoolField(Payload, TEXT("weld"), true);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    UDynamicMesh* MirroredMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    MirroredMesh->SetMesh(Mesh->GetMeshRef());

    // Mirror by scaling with negative value on the axis
    FVector MirrorScale = FVector::OneVector;
    if (Axis == TEXT("X")) MirrorScale.X = -1.0;
    else if (Axis == TEXT("Y")) MirrorScale.Y = -1.0;
    else if (Axis == TEXT("Z")) MirrorScale.Z = -1.0;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MirroredMesh, MirrorScale, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = MirroredMesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= MirrorScale.X;
            Pos.Y *= MirrorScale.Y;
            Pos.Z *= MirrorScale.Z;
            EditMesh.SetVertex(VID, Pos);
        }
    }
#endif

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, MirroredMesh, FTransform::Identity, false, AppendOptions, nullptr);

    if (bWeld)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = 0.001;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mirror applied"), Result);
    return true;
}

bool HandleTranslateMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FVector Translation = ReadVectorFromPayload(Payload, TEXT("translation"), FVector::ZeroVector);

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    // UE 5.7+: TranslateMesh is in MeshTransformFunctions
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    TSharedPtr<FJsonObject> TransObj = McpHandlerUtils::CreateResultObject();
    TransObj->SetNumberField(TEXT("x"), Translation.X);
    TransObj->SetNumberField(TEXT("y"), Translation.Y);
    TransObj->SetNumberField(TEXT("z"), Translation.Z);
    Result->SetObjectField(TEXT("translation"), TransObj);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh translated"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
