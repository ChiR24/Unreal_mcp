#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT
#include "Domains/Geometry/Primitives/McpAutomationBridge_GeometrySdfCopies.h"
#include "Domains/Geometry/Primitives/McpAutomationBridge_GeometrySdfField.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Generators/MarchingCubes.h"

// create_sdf: shapes blended as signed distance fields and meshed by the engine's
// marching cubes. Organic forms - a helmet with a filleted visor recess, a mitten
// hand, a sneaker - come out of one call with soft fillets where parts meet, which
// no boolean of primitives gives.
namespace McpGeometryHandlers
{
namespace
{
using McpGeometrySdf::EOp;
using McpGeometrySdf::EShape;
using McpGeometrySdf::FShape;

FVector3d ReadVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FVector3d& Default)
{
    const TSharedPtr<FJsonObject>* V = nullptr;
    FVector3d Out = Default;
    if (Obj->TryGetObjectField(Field, V) && V && V->IsValid())
    {
        (*V)->TryGetNumberField(TEXT("x"), Out.X);
        (*V)->TryGetNumberField(TEXT("y"), Out.Y);
        (*V)->TryGetNumberField(TEXT("z"), Out.Z);
    }
    return Out;
}

FQuat ReadRotation(const TSharedPtr<FJsonObject>& Obj)
{
    const TSharedPtr<FJsonObject>* R = nullptr;
    double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
    if (Obj->TryGetObjectField(TEXT("rotation"), R) && R && R->IsValid())
    {
        (*R)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*R)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*R)->TryGetNumberField(TEXT("roll"), Roll);
    }
    return FRotator(Pitch, Yaw, Roll).Quaternion();
}

// Reads shapes[]; returns the refusal text, empty when every shape is usable.
FString ReadShapes(const TSharedPtr<FJsonObject>& Payload, TArray<FShape>& Out)
{
    static const TMap<FString, EShape> Types = {{TEXT("sphere"), EShape::Sphere}, {TEXT("ellipsoid"), EShape::Ellipsoid},
        {TEXT("box"), EShape::Box}, {TEXT("capsule"), EShape::Capsule}, {TEXT("cylinder"), EShape::Cylinder},
        {TEXT("torus"), EShape::Torus}, {TEXT("cone"), EShape::Cone}};
    static const TMap<FString, EOp> Ops = {{TEXT("union"), EOp::Union}, {TEXT("subtract"), EOp::Subtract}, {TEXT("intersect"), EOp::Intersect}};
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Payload->TryGetArrayField(TEXT("shapes"), Items) || !Items || Items->Num() == 0 || Items->Num() > 64)
    {
        return TEXT("shapes must list 1 to 64 shapes, each {type: sphere|ellipsoid|box|capsule|cylinder|torus|cone, ...}.");
    }
    for (int32 Index = 0; Index < Items->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!(*Items)[Index].IsValid() || !(*Items)[Index]->TryGetObject(Obj) || !Obj || !Obj->IsValid())
        {
            return FString::Printf(TEXT("shapes[%d] is not an object."), Index);
        }
        const EShape* Type = Types.Find(GetJsonStringField(*Obj, TEXT("type")).ToLower());
        const EOp* Op = Ops.Find(GetJsonStringField(*Obj, TEXT("operation"), TEXT("union")).ToLower());
        if (!Type || !Op)
        {
            return FString::Printf(TEXT("shapes[%d] needs type sphere, ellipsoid, box, capsule, cylinder, torus or cone, and operation union, subtract or intersect."), Index);
        }
        if (Index == 0 && *Op != EOp::Union)
        {
            return TEXT("shapes[0] is the base the others apply to, so its operation must be union.");
        }
        FShape S;
        S.Type = *Type;
        S.Op = *Op;
        S.Frame = FTransform(ReadRotation(*Obj), ReadVec(*Obj, TEXT("center"), FVector3d::ZeroVector));
        S.Radius = GetJsonNumberField(*Obj, TEXT("radius"), 50.0);
        S.TopRadius = GetJsonNumberField(*Obj, TEXT("topRadius"), 0.0);
        S.Length = GetJsonNumberField(*Obj, TEXT("length"), 100.0);
        S.Rounding = FMath::Max(0.0, GetJsonNumberField(*Obj, TEXT("rounding"), 0.0));
        S.Thickness = GetJsonNumberField(*Obj, TEXT("thickness"), 10.0);
        S.Blend = FMath::Max(0.0, GetJsonNumberField(*Obj, TEXT("blend"), 0.0));
        S.MaterialId = FMath::Clamp(GetJsonIntField(*Obj, TEXT("materialId"), 0), 0, 63);
        S.Radii = ReadVec(*Obj, TEXT("radii"), FVector3d(S.Radius));
        S.Extent = ReadVec(*Obj, TEXT("extent"), FVector3d(S.Radius));
        if (S.Radius <= 0.0 || S.TopRadius < 0.0 || S.Length < 0.0 || S.Thickness <= 0.0 || S.Radii.GetMin() <= 0.0 || S.Extent.GetMin() <= 0.0)
        {
            return FString::Printf(TEXT("shapes[%d] needs positive sizes (radius, radii, extent, thickness; length and topRadius 0 or more)."), Index);
        }
        S.Source = Index;
        S.Reach = McpGeometrySdf::BoundRadius(S);
        TArray<FShape> Copies;
        FString CopyError;
        if (!McpGeometrySdf::ExpandRepeat(*Obj, S, Copies, CopyError) || !McpGeometrySdf::ExpandMirror(*Obj, Copies, CopyError))
        {
            return FString::Printf(TEXT("shapes[%d]: %s"), Index, *CopyError);
        }
        Out.Append(Copies);
        if (Out.Num() > 1024)
        {
            return TEXT("repeat and mirror copies come to more than 1024 shapes; split the part into two meshes.");
        }
    }
    return FString();
}

// Which shapes[] entry decides the surface at Pt: copies count as the shape that made them.
int32 SourceOwner(const TArray<FShape>& Shapes, const FVector3d& Pt)
{
    return Shapes[McpGeometrySdf::FieldOwner(Shapes, Pt)].Source;
}

// Splits every edge whose ends belong to different shapes at the point where ownership
// changes (bisection along the edge), so the part boundary runs through those points as a
// smooth line instead of stepping from whole triangle to whole triangle.
void SplitAtPartBoundaries(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FShape>& Shapes)
{
    TArray<int32> Crossing;
    for (const int32 Eid : Mesh.EdgeIndicesItr())
    {
        const UE::Geometry::FIndex2i V = Mesh.GetEdgeV(Eid);
        if (SourceOwner(Shapes, Mesh.GetVertex(V.A)) != SourceOwner(Shapes, Mesh.GetVertex(V.B))) Crossing.Add(Eid);
    }
    // Splitting an edge leaves the other listed edges' ends untouched, so each is split once, end to end.
    for (const int32 Eid : Crossing)
    {
        const UE::Geometry::FIndex2i V = Mesh.GetEdgeV(Eid);
        const FVector3d A = Mesh.GetVertex(V.A);
        const FVector3d B = Mesh.GetVertex(V.B);
        const int32 OwnerA = SourceOwner(Shapes, A);
        double Lo = 0.0, Hi = 1.0;
        for (int32 Step = 0; Step < 8; ++Step)
        {
            const double Mid = 0.5 * (Lo + Hi);
            if (SourceOwner(Shapes, FMath::Lerp(A, B, Mid)) == OwnerA) Lo = Mid;
            else Hi = Mid;
        }
        UE::Geometry::FDynamicMesh3::FEdgeSplitInfo Info;
        Mesh.SplitEdge(Eid, Info, 0.5 * (Lo + Hi));
    }
}

// Each triangle belongs to the shapes[] entry that decides the surface at its centre (a copy
// counts as its author): polygroup index+1 and that shape's materialId, so a later edit or a
// material slot can pick out a part.
void AssignParts(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FShape>& Shapes, int32 Authored, TArray<int32>& OutTriangles)
{
    SplitAtPartBoundaries(Mesh, Shapes);
    Mesh.EnableTriangleGroups();
    Mesh.EnableAttributes();
    Mesh.Attributes()->EnableMaterialID();
    UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = Mesh.Attributes()->GetMaterialID();
    OutTriangles.SetNumZeroed(Authored);
    for (const int32 Tid : Mesh.TriangleIndicesItr())
    {
        const FShape& Owner = Shapes[McpGeometrySdf::FieldOwner(Shapes, Mesh.GetTriCentroid(Tid))];
        Mesh.SetTriangleGroup(Tid, Owner.Source + 1);
        MaterialIds->SetValue(Tid, Owner.MaterialId);
        ++OutTriangles[Owner.Source];
    }
}
} // namespace

bool HandleCreateSdf(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TArray<FShape> Shapes;
    const FString ShapeError = ReadShapes(Payload, Shapes);
    if (!ShapeError.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, ShapeError, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("GeneratedSdf");
    const int32 Resolution = FMath::Clamp(GetJsonIntField(Payload, TEXT("resolution"), 128), 16, 256);
    // Memory now; the triangle count is only known once the surface is meshed.
    if (!GuardMeshBudget(Self, RequestId, Socket, 0, TEXT("SDF meshing"))) return true;

    // Only unions add volume; subtract and intersect shapes stay inside these bounds.
    UE::Geometry::FAxisAlignedBox3d Bounds = UE::Geometry::FAxisAlignedBox3d::Empty();
    for (const FShape& S : Shapes)
    {
        if (S.Op == EOp::Union)
        {
            const double R = S.Reach + S.Blend;
            Bounds.Contain(S.Frame.GetLocation() - FVector3d(R));
            Bounds.Contain(S.Frame.GetLocation() + FVector3d(R));
        }
    }
    if (!FMath::IsFinite(Bounds.MaxDim()) || Bounds.MaxDim() > MAX_DIMENSION)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("The shapes span more than %.0f units; keep sizes, centers and blends within that."), MAX_DIMENSION), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UE::Geometry::FMarchingCubes Cubes;
    Cubes.CubeSize = Bounds.MaxDim() / Resolution;
    Bounds.Expand(2.0 * Cubes.CubeSize);
    Cubes.Bounds = Bounds;
    Cubes.IsoValue = 0.0;
    Cubes.RootMode = UE::Geometry::ERootfindingModes::Bisection;
    Cubes.RootModeSteps = 6;
    Cubes.bParallelCompute = true;
    // Engine convention (Implicit/Morphology.h): positive inside, so the field is negated.
    Cubes.Implicit = [&Shapes](UE::Math::TVector<double> Pt) { return -McpGeometrySdf::FieldDistance(Shapes, Pt); };
    Cubes.Generate();
    UE::Geometry::FDynamicMesh3 Mesh(&Cubes);
    Cubes.Implicit = nullptr;
    if (Mesh.TriangleCount() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("The shapes enclose no volume: a subtract or intersect removed everything."), TEXT("SDF_EMPTY"));
        return true;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, Mesh.TriangleCount(), TEXT("SDF meshing at this resolution"))) return true;
    const int32 Authored = Shapes.Last().Source + 1;
    TArray<int32> PartTriangles;
    AssignParts(Mesh, Shapes, Authored, PartTriangles);
    const int32 VertexCount = Mesh.VertexCount();
    const int32 TriangleCount = Mesh.TriangleCount();
    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    DynMesh->SetMesh(MoveTemp(Mesh));
    UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynMesh, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result);
    if (!NewActor) return true;
    TArray<TSharedPtr<FJsonValue>> Parts;
    TArray<int32> CopyCounts;
    CopyCounts.SetNumZeroed(Authored);
    for (const FShape& S : Shapes)
    {
        ++CopyCounts[S.Source];
    }
    for (int32 Index = 0; Index < Authored; ++Index)
    {
        const FShape* First = Shapes.FindByPredicate([Index](const FShape& S) { return S.Source == Index; });
        TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
        Part->SetNumberField(TEXT("shape"), Index);
        Part->SetNumberField(TEXT("groupId"), Index + 1);
        Part->SetNumberField(TEXT("materialId"), First ? First->MaterialId : 0);
        Part->SetNumberField(TEXT("triangles"), PartTriangles[Index]);
        if (CopyCounts[Index] > 1)
        {
            Part->SetNumberField(TEXT("copies"), CopyCounts[Index]);
        }
        const double Thinnest = First ? McpGeometrySdf::ThinnestSize(*First) : 0.0;
        if (First && First->Op == EOp::Union && Thinnest < 2.0 * Cubes.CubeSize)
        {
            // A thin strip on a broad face still meshes (thousands of triangles), only raggedly; only a part
            // that formed almost nothing is lost.
            const TCHAR* Outcome = PartTriangles[Index] < 24 ? TEXT("it meshed to almost nothing")
                                                             : TEXT("its surface can come out ragged or patchy");
            Part->SetStringField(TEXT("warning"), FString::Printf(TEXT("%.1f cm across, under two %.1f cm cells: %s. Raise resolution or thicken it."), Thinnest, Cubes.CubeSize, Outcome));
        }
        Parts.Add(MakeShared<FJsonValueObject>(Part));
    }
    Result->SetArrayField(TEXT("parts"), Parts);
    Result->SetNumberField(TEXT("resolution"), Resolution);
    Result->SetNumberField(TEXT("cellSize"), Cubes.CubeSize);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("triangleCount"), TriangleCount);
    McpHandlerUtils::AddVerification(Result, NewActor);
    Self->SendAutomationResponse(Socket, RequestId, true, Authored == Shapes.Num()
        ? FString::Printf(TEXT("SDF mesh created from %d shapes"), Authored)
        : FString::Printf(TEXT("SDF mesh created from %d shapes (%d with repeat and mirror copies)"), Authored, Shapes.Num()), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
