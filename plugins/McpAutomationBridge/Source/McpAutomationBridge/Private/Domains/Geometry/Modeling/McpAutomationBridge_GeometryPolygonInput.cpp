// McpAutomationBridge_GeometryPolygonInput.cpp — the validated input of append_polygons.
//
// `vertices` are new points and `faces` are lists of indices into them, three or more per face, wound so that
// (v1 - v0) x (v2 - v0) points outward. A face with more than three corners is triangulated by ear clipping, and
// every triangle of a face carries the face's polygroup, so a quad cage reads back as quads to subdivide with
// catmull_clark. Everything is checked before the mesh is touched, and a refusal names the face at fault.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "CompGeom/PolygonTriangulation.h"

namespace McpGeometryHandlers
{
static constexpr int32 MaxAppendVertices = 20000;
static constexpr int32 MaxAppendFaces = 20000;
static constexpr int32 MaxAppendFaceCorners = 256;
// FGroupTopology sizes arrays by the largest group id, so an id has to stay modest.
static constexpr int32 MaxAppendGroupId = 1000000;

static bool ParseAppendVertices(const TSharedPtr<FJsonObject>& Payload, TArray<FVector3d>& OutVertices, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(TEXT("vertices"), Values) || Values->Num() == 0)
    {
        OutError = TEXT("vertices required: a list of {x, y, z} points that faces index into");
        return false;
    }
    if (Values->Num() > MaxAppendVertices)
    {
        OutError = FString::Printf(TEXT("vertices has %d points; at most %d can be appended in one call"), Values->Num(), MaxAppendVertices);
        return false;
    }
    OutVertices.Reserve(Values->Num());
    for (int32 Index = 0; Index < Values->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(Point) || !Point->IsValid()
            || !(*Point)->TryGetNumberField(TEXT("x"), X) || !(*Point)->TryGetNumberField(TEXT("y"), Y)
            || !(*Point)->TryGetNumberField(TEXT("z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
        {
            OutError = FString::Printf(TEXT("vertices[%d] must be an {x, y, z} object of finite numbers"), Index);
            return false;
        }
        OutVertices.Add(FVector3d(X, Y, Z));
    }
    return true;
}

static bool ParseAppendFaces(const TSharedPtr<FJsonObject>& Payload, int32 VertexCount, TArray<TArray<int32>>& OutFaces, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(TEXT("faces"), Values) || Values->Num() == 0)
    {
        OutError = TEXT("faces required: a list of faces, each a list of three or more indices into vertices");
        return false;
    }
    if (Values->Num() > MaxAppendFaces)
    {
        OutError = FString::Printf(TEXT("faces has %d entries; at most %d can be appended in one call"), Values->Num(), MaxAppendFaces);
        return false;
    }
    OutFaces.Reserve(Values->Num());
    int64 Triangles = 0;
    for (int32 FaceIndex = 0; FaceIndex < Values->Num(); ++FaceIndex)
    {
        const TArray<TSharedPtr<FJsonValue>>* Corners = nullptr;
        if (!(*Values)[FaceIndex].IsValid() || !(*Values)[FaceIndex]->TryGetArray(Corners))
        {
            OutError = FString::Printf(TEXT("faces[%d] must be a list of vertex indices"), FaceIndex);
            return false;
        }
        if (Corners->Num() < 3 || Corners->Num() > MaxAppendFaceCorners)
        {
            OutError = FString::Printf(TEXT("faces[%d] has %d corners; a face needs 3 to %d"), FaceIndex, Corners->Num(), MaxAppendFaceCorners);
            return false;
        }
        Triangles += Corners->Num() - 2;
        if (Triangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
        {
            OutError = FString::Printf(TEXT("faces come to more than %d triangles; append them in several calls"), MAX_TRIANGLES_PER_DYNAMIC_MESH);
            return false;
        }
        TArray<int32>& Face = OutFaces.AddDefaulted_GetRef();
        for (const TSharedPtr<FJsonValue>& Corner : *Corners)
        {
            double Number = 0.0;
            if (!Corner.IsValid() || !Corner->TryGetNumber(Number) || !FMath::IsFinite(Number) || Number != FMath::FloorToDouble(Number))
            {
                OutError = FString::Printf(TEXT("faces[%d] holds a value that is not a whole-number vertex index"), FaceIndex);
                return false;
            }
            if (Number < 0.0 || Number >= VertexCount)
            {
                OutError = FString::Printf(TEXT("faces[%d] uses vertex index %.0f, but vertices has %d points (valid indices 0 to %d)"),
                                           FaceIndex, Number, VertexCount, VertexCount - 1);
                return false;
            }
            if (Face.Contains(static_cast<int32>(Number)))
            {
                OutError = FString::Printf(TEXT("faces[%d] uses vertex %.0f twice; a face cannot repeat a corner"), FaceIndex, Number);
                return false;
            }
            Face.Add(static_cast<int32>(Number));
        }
    }
    return true;
}

// An optional one-integer-per-face list (faceGroups, faceMaterials); OutValues stays empty when the field is absent.
static bool ParseAppendFaceIntegers(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, int32 FaceCount, int32 MaxValue,
                                    TArray<int32>& OutValues, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(Field, Values) || Values->Num() == 0)
    {
        return true;
    }
    if (Values->Num() != FaceCount)
    {
        OutError = FString::Printf(TEXT("%s has %d entries but faces has %d; give one value per face"), Field, Values->Num(), FaceCount);
        return false;
    }
    for (int32 FaceIndex = 0; FaceIndex < Values->Num(); ++FaceIndex)
    {
        double Number = 0.0;
        if (!(*Values)[FaceIndex].IsValid() || !(*Values)[FaceIndex]->TryGetNumber(Number) || !FMath::IsFinite(Number)
            || Number != FMath::FloorToDouble(Number) || Number < 0.0 || Number > MaxValue)
        {
            OutError = FString::Printf(TEXT("%s[%d] must be a whole number from 0 to %d"), Field, FaceIndex, MaxValue);
            return false;
        }
        OutValues.Add(static_cast<int32>(Number));
    }
    return true;
}

// Triangles of every face, keeping each face's winding; TriangleFace says which face a triangle came from.
static bool TriangulateAppendFaces(FAppendPolygonsInput& Input, FString& OutError)
{
    for (int32 FaceIndex = 0; FaceIndex < Input.Faces.Num(); ++FaceIndex)
    {
        const TArray<int32>& Face = Input.Faces[FaceIndex];
        if (Face.Num() == 3)
        {
            Input.Triangles.Add(UE::Geometry::FIndex3i(Face[0], Face[1], Face[2]));
            Input.TriangleFace.Add(FaceIndex);
            continue;
        }
        TArray<FVector3d> Polygon;
        Polygon.Reserve(Face.Num());
        for (const int32 Corner : Face)
        {
            Polygon.Add(Input.Vertices[Corner]);
        }
        TArray<UE::Geometry::FIndex3i> Local;
        PolygonTriangulation::TriangulateSimplePolygon(Polygon, Local, /*bOrientAsHoleFill=*/false);
        if (Local.Num() != Face.Num() - 2)
        {
            OutError = FString::Printf(TEXT("faces[%d] cannot be triangulated: its corners are collinear, or the face crosses itself"), FaceIndex);
            return false;
        }
        for (const UE::Geometry::FIndex3i& Triangle : Local)
        {
            Input.Triangles.Add(UE::Geometry::FIndex3i(Face[Triangle.A], Face[Triangle.B], Face[Triangle.C]));
            Input.TriangleFace.Add(FaceIndex);
        }
    }
    return true;
}

// The cage has to be a manifold with consistent winding: an edge belongs to at most two faces, and two faces that
// share one run along it in opposite directions. The mesh itself would accept the second, but subdivision cannot.
static bool CheckAppendEdges(const FAppendPolygonsInput& Input, FString& OutError)
{
    TMap<uint64, int32> DirectedFace;
    TMap<uint64, int32> Uses;
    for (int32 TriangleIndex = 0; TriangleIndex < Input.Triangles.Num(); ++TriangleIndex)
    {
        const UE::Geometry::FIndex3i& Triangle = Input.Triangles[TriangleIndex];
        const int32 Face = Input.TriangleFace[TriangleIndex];
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const uint32 From = static_cast<uint32>(Triangle[Corner]);
            const uint32 To = static_cast<uint32>(Triangle[(Corner + 1) % 3]);
            const uint64 Directed = (static_cast<uint64>(From) << 32) | To;
            if (const int32* Other = DirectedFace.Find(Directed))
            {
                OutError = FString::Printf(
                    TEXT("faces[%d] and faces[%d] both run from vertex %u to vertex %u, so they are wound opposite ways: reverse the vertex order of one "
                         "so shared edges run in opposite directions, with (v1 - v0) x (v2 - v0) pointing outward"), *Other, Face, From, To);
                return false;
            }
            DirectedFace.Add(Directed, Face);
            int32& Count = Uses.FindOrAdd((static_cast<uint64>(FMath::Min(From, To)) << 32) | FMath::Max(From, To));
            if (++Count > 2)
            {
                OutError = FString::Printf(TEXT("the edge between vertex %u and vertex %u is shared by more than two faces (faces[%d] is one of them); "
                                                "a cage is a manifold surface, so an edge borders at most two faces"), From, To, Face);
                return false;
            }
        }
    }
    return true;
}

bool ParseAppendPolygonsInput(const TSharedPtr<FJsonObject>& Payload, FAppendPolygonsInput& OutInput, FString& OutError)
{
    return ParseAppendVertices(Payload, OutInput.Vertices, OutError)
        && ParseAppendFaces(Payload, OutInput.Vertices.Num(), OutInput.Faces, OutError)
        && ParseAppendFaceIntegers(Payload, TEXT("faceGroups"), OutInput.Faces.Num(), MaxAppendGroupId, OutInput.FaceGroups, OutError)
        && ParseAppendFaceIntegers(Payload, TEXT("faceMaterials"), OutInput.Faces.Num(), MAX_MATERIAL_ID, OutInput.FaceMaterials, OutError)
        && TriangulateAppendFaces(OutInput, OutError)
        && CheckAppendEdges(OutInput, OutError);
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
