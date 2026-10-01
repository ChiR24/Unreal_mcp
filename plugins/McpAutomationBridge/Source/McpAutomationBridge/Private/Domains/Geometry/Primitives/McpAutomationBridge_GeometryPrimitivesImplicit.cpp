#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT
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
        Out.Add(S);
    }
    return FString();
}

// Each triangle belongs to the shape whose surface is nearest: polygroup index+1 and that
// shape's materialId, so a later edit or a material slot can pick out a part.
void AssignParts(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FShape>& Shapes, TArray<int32>& OutTriangles)
{
    Mesh.EnableTriangleGroups();
    Mesh.EnableAttributes();
    Mesh.Attributes()->EnableMaterialID();
    UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = Mesh.Attributes()->GetMaterialID();
    OutTriangles.SetNumZeroed(Shapes.Num());
    for (const int32 Tid : Mesh.TriangleIndicesItr())
    {
        const FVector3d Centroid = Mesh.GetTriCentroid(Tid);
        int32 Owner = 0;
        double Best = TNumericLimits<double>::Max();
        for (int32 Index = 0; Index < Shapes.Num(); ++Index)
        {
            const double Ds = FMath::Abs(McpGeometrySdf::LocalDistance(Shapes[Index], Centroid));
            if (Ds < Best) { Best = Ds; Owner = Index; }
        }
        Mesh.SetTriangleGroup(Tid, Owner + 1);
        MaterialIds->SetValue(Tid, Shapes[Owner].MaterialId);
        ++OutTriangles[Owner];
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
    if (!GuardMeshBudget(Self, RequestId, Socket, 8LL * Resolution * Resolution, TEXT("SDF meshing"))) return true;

    // Only unions add volume; subtract and intersect shapes stay inside these bounds.
    UE::Geometry::FAxisAlignedBox3d Bounds = UE::Geometry::FAxisAlignedBox3d::Empty();
    for (const FShape& S : Shapes)
    {
        if (S.Op == EOp::Union)
        {
            const double R = McpGeometrySdf::BoundRadius(S) + S.Blend;
            Bounds.Contain(S.Frame.GetLocation() - FVector3d(R));
            Bounds.Contain(S.Frame.GetLocation() + FVector3d(R));
        }
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
    TArray<int32> PartTriangles;
    AssignParts(Mesh, Shapes, PartTriangles);
    const int32 VertexCount = Mesh.VertexCount();
    const int32 TriangleCount = Mesh.TriangleCount();
    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    DynMesh->SetMesh(MoveTemp(Mesh));
    UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynMesh, nullptr);

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result);
    if (!NewActor) return true;
    TArray<TSharedPtr<FJsonValue>> Parts;
    for (int32 Index = 0; Index < Shapes.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
        Part->SetNumberField(TEXT("shape"), Index);
        Part->SetNumberField(TEXT("groupId"), Index + 1);
        Part->SetNumberField(TEXT("materialId"), Shapes[Index].MaterialId);
        Part->SetNumberField(TEXT("triangles"), PartTriangles[Index]);
        Parts.Add(MakeShared<FJsonValueObject>(Part));
    }
    Result->SetArrayField(TEXT("parts"), Parts);
    Result->SetNumberField(TEXT("resolution"), Resolution);
    Result->SetNumberField(TEXT("cellSize"), Cubes.CubeSize);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("triangleCount"), TriangleCount);
    McpHandlerUtils::AddVerification(Result, NewActor);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("SDF mesh created from %d shapes"), Shapes.Num()), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
