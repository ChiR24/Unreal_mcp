#include "Domains/Environment/McpEnvShared.h"
#include "Domains/Environment/Inspection/MeshCheck/McpMeshCheck.h"

// check_mesh: whether a static or skeletal mesh's geometry is sound, read from its source triangles (the mesh as
// authored or imported, not a Nanite fallback; a skeletal mesh in its reference pose). A model cannot see a mesh: an
// imported or generated one with a hole, an edge shared by three faces, faces turned inside out or a whole piece wound
// inside out passed every other check. McpMeshCheck reads it, as it does for the import and reimport warnings.
namespace McpEnvironmentHandlers {
namespace {
constexpr int32 McpMaxListedParts = 8;

TArray<TSharedPtr<FJsonValue>> McpRoundedVec(const FVector &V)
{
    return McpHandlerUtils::VectorToJsonArray(FVector(FMath::RoundToDouble(V.X * 10.0) / 10.0,
        FMath::RoundToDouble(V.Y * 10.0) / 10.0, FMath::RoundToDouble(V.Z * 10.0) / 10.0));
}
} // namespace

bool HandleInspectMeshCheckAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    UObject *Mesh = MeshPath.IsEmpty() ? nullptr : McpHandlerUtils::ResolveObjectFromPath(MeshPath);
    TArray<FVector3f> Positions;
    TArray<int32> Corners;
    if (!McpMeshCheck::ReadSourceTriangles(Mesh, Positions, Corners))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("meshPath '%s' is not a static or skeletal mesh with source geometry."), *MeshPath), TEXT("NOT_FOUND"));
        return true;
    }
    const McpMeshCheck::FReport Report = McpMeshCheck::Analyze(Positions, Corners);
    TArray<TSharedPtr<FJsonValue>> Listed;
    for (int32 Index = 0; Index < FMath::Min(Report.Parts.Num(), McpMaxListedParts); ++Index)
    {
        const McpMeshCheck::FPart &Part = Report.Parts[Index];
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("triangles"), Part.Triangles);
        Entry->SetArrayField(TEXT("size"), McpRoundedVec(Part.Box.GetSize()));
        Entry->SetArrayField(TEXT("center"), McpRoundedVec(Part.Box.GetCenter()));
        if (Part.bInsideOut)
        {
            Entry->SetBoolField(TEXT("insideOut"), true);
        }
        Listed.Add(MakeShared<FJsonValueObject>(Entry));
    }

    const bool bClosed = Report.IsClosed();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
    Result->SetNumberField(TEXT("triangles"), Report.Triangles);
    Result->SetNumberField(TEXT("vertices"), Report.Vertices);
    Result->SetArrayField(TEXT("size"), McpRoundedVec(Report.Bounds.IsValid ? Report.Bounds.GetSize() : FVector::ZeroVector));
    Result->SetNumberField(TEXT("surfaceArea"), FMath::RoundToDouble(Report.SurfaceArea));
    Result->SetBoolField(TEXT("closed"), bClosed);
    if (bClosed)
    {
        // Only a closed surface encloses a volume: each part's size, whichever way it is wound.
        Result->SetNumberField(TEXT("volume"), FMath::RoundToDouble(Report.Volume));
    }
    Result->SetNumberField(TEXT("openEdges"), Report.OpenEdges);
    Result->SetNumberField(TEXT("holes"), Report.Holes);
    Result->SetNumberField(TEXT("seamEdges"), Report.SeamEdges);
    Result->SetNumberField(TEXT("nonManifoldEdges"), Report.NonManifoldEdges);
    Result->SetNumberField(TEXT("flippedEdges"), Report.FlippedEdges);
    Result->SetNumberField(TEXT("degenerateTriangles"), Report.DegenerateTriangles);
    Result->SetNumberField(TEXT("insideOutParts"), Report.InsideOutParts);
    Result->SetNumberField(TEXT("parts"), Report.Parts.Num());
    Result->SetArrayField(TEXT("largestParts"), Listed);
    McpHandlerUtils::MarkNoAssetsChanged(Result);

    FString Message = FString::Printf(TEXT("%s: %d triangles in %d part(s), %s"), *Mesh->GetName(), Report.Triangles,
        Report.Parts.Num(), bClosed ? TEXT("closed") : *FString::Printf(TEXT("open (%d open edges in %d hole(s))"),
        Report.OpenEdges, Report.Holes));
    if (Report.InsideOutParts > 0)
    {
        Message += FString::Printf(TEXT("; %d part(s) are inside out: their faces point inward, so the renderer culls the side "
            "you look at and shows the far inner wall; flip their normals in the source and import again"), Report.InsideOutParts);
    }
    if (Report.NonManifoldEdges + Report.FlippedEdges + Report.DegenerateTriangles > 0)
    {
        Message += FString::Printf(TEXT("; %d non-manifold edges, %d flipped edges, %d degenerate triangles"),
            Report.NonManifoldEdges, Report.FlippedEdges, Report.DegenerateTriangles);
    }
    if (Report.SeamEdges > 0)
    {
        Message += FString::Printf(TEXT("; its faces are split along %d seam edges (corners at one place are separate "
            "vertices): it renders whole, but booleans, remeshing and collision built from it need it welded"), Report.SeamEdges);
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message + TEXT("."), Result);
    return true;
}
} // namespace McpEnvironmentHandlers
