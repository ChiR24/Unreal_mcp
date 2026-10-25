// McpGeometryAppendShapes.cpp — append_cuboids and append_loft: whole parts as one cage, wound for the caller.
//
// Both build the input append_polygons takes (McpGeometryPolygonInput.cpp), so they append, refuse and reply the
// same way. append_cuboids lays out axis-aligned boxes, each repeated along up to three axes (a railing, a container
// stack, a window grid); append_loft joins cross sections in order (a hull, a car body, a bottle) with every face
// turned outward from the way the sections run. A cage of hundreds of hand-wound faces took one call each way.
#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
namespace
{
constexpr int32 MaxShapePoints = 20000;
constexpr int32 MaxRepeat = 1000;

bool ReadPointValue(const TSharedPtr<FJsonValue>& Value, FVector3d& Out)
{
    const TSharedPtr<FJsonObject>* Point = nullptr;
    double X = 0.0, Y = 0.0, Z = 0.0;
    if (!Value.IsValid() || !Value->TryGetObject(Point) || !Point->IsValid() || !(*Point)->TryGetNumberField(TEXT("x"), X)
        || !(*Point)->TryGetNumberField(TEXT("y"), Y) || !(*Point)->TryGetNumberField(TEXT("z"), Z)
        || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        return false;
    }
    Out = FVector3d(X, Y, Z);
    return true;
}

// An optional {x, y, z} field; false only when it is present and malformed.
bool ReadOptionalPoint(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector3d& Out, bool& bOutFound)
{
    bOutFound = Object->HasField(Field);
    return !bOutFound || ReadPointValue(Object->TryGetField(Field), Out);
}

// A repeat step: an axis left out is 0, so {x: 200} steps along X only.
bool ReadOffset(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector3d& Out)
{
    const TSharedPtr<FJsonObject>* Step = nullptr;
    if (!Object->HasField(Field)) return true;
    if (!Object->TryGetObjectField(Field, Step) || !Step->IsValid()) return false;
    const TCHAR* Axes[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        double Value = 0.0;
        if ((*Step)->HasField(Axes[Axis]) && (!(*Step)->TryGetNumberField(Axes[Axis], Value) || !FMath::IsFinite(Value))) return false;
        Out[Axis] = Value;
    }
    return true;
}

// A whole number in [Min, Max] when present, else Default.
bool ReadCount(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default, int32 Min, int32 Max, int32& Out)
{
    double Number = Default;
    if (Object->HasField(Field) && !Object->TryGetNumberField(Field, Number)) return false;
    if (!FMath::IsFinite(Number) || Number != FMath::FloorToDouble(Number) || Number < Min || Number > Max) return false;
    Out = static_cast<int32>(Number);
    return true;
}

bool ReadMaterialList(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<int32>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object->TryGetArrayField(Field, Values)) return true;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        double Number = 0.0;
        if (!Value.IsValid() || !Value->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number) || Number < 0 || Number > MAX_MATERIAL_ID) return false;
        Out.Add(static_cast<int32>(Number));
    }
    return true;
}

// The six faces of a box from its 8 corners (corner index bit 0 = x, bit 1 = y, bit 2 = z), each wound outward.
struct FBoxFace { const TCHAR* Name; int32 Corners[4]; };
constexpr FBoxFace BoxFaces[6] = {
    {TEXT("-z"), {0, 2, 3, 1}}, {TEXT("+z"), {4, 5, 7, 6}}, {TEXT("-y"), {0, 1, 5, 4}},
    {TEXT("+y"), {3, 2, 6, 7}}, {TEXT("-x"), {2, 0, 4, 6}}, {TEXT("+x"), {1, 3, 7, 5}},
};

void AddBox(FAppendPolygonsInput& Out, const FVector3d& Min, const FVector3d& Max, int32 Material, const TArray<FString>& Open)
{
    ++Out.BoxCount;
    const int32 Base = Out.Vertices.Num();
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        Out.Vertices.Add(FVector3d((Corner & 1) ? Max.X : Min.X, (Corner & 2) ? Max.Y : Min.Y, (Corner & 4) ? Max.Z : Min.Z));
    }
    for (const FBoxFace& Face : BoxFaces)
    {
        if (Open.Contains(Face.Name)) continue;
        Out.Faces.Add({Base + Face.Corners[0], Base + Face.Corners[1], Base + Face.Corners[2], Base + Face.Corners[3]});
        Out.FaceMaterials.Add(Material);
    }
}
} // namespace

bool ReadRepeatGrid(const TSharedPtr<FJsonObject>& Owner, FMcpRepeatGrid& Out)
{
    const TSharedPtr<FJsonObject>* Repeat = nullptr;
    if (!Owner->HasField(TEXT("repeat"))) return true;
    return Owner->TryGetObjectField(TEXT("repeat"), Repeat) && Repeat->IsValid()
        && ReadCount(*Repeat, TEXT("count"), 1, 1, MaxRepeat, Out.Count) && ReadCount(*Repeat, TEXT("rows"), 1, 1, MaxRepeat, Out.Rows)
        && ReadCount(*Repeat, TEXT("layers"), 1, 1, MaxRepeat, Out.Layers) && ReadOffset(*Repeat, TEXT("offset"), Out.Offset)
        && ReadOffset(*Repeat, TEXT("rowOffset"), Out.RowOffset) && ReadOffset(*Repeat, TEXT("layerOffset"), Out.LayerOffset);
}

bool ParseAppendBoxesInput(const TSharedPtr<FJsonObject>& Payload, FAppendPolygonsInput& OutInput, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Boxes = nullptr;
    if (!Payload->TryGetArrayField(TEXT("boxes"), Boxes) || Boxes->Num() == 0)
    {
        OutError = TEXT("boxes required: a list of {min, max} boxes, each with an optional materialId, open faces and repeat");
        return false;
    }
    for (int32 Index = 0; Index < Boxes->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Box = nullptr;
        FVector3d Min, Max;
        if (!(*Boxes)[Index]->TryGetObject(Box) || !Box->IsValid() || !ReadPointValue((*Box)->TryGetField(TEXT("min")), Min)
            || !ReadPointValue((*Box)->TryGetField(TEXT("max")), Max) || Max.X <= Min.X || Max.Y <= Min.Y || Max.Z <= Min.Z)
        {
            OutError = FString::Printf(TEXT("boxes[%d] needs min and max {x, y, z} with max larger than min on every axis"), Index);
            return false;
        }
        int32 Material = 0;
        TArray<int32> Cycle;
        FMcpRepeatGrid Grid;
        if (!ReadCount(*Box, TEXT("materialId"), 0, 0, MAX_MATERIAL_ID, Material) || !ReadMaterialList(*Box, TEXT("materialIds"), Cycle)
            || !ReadRepeatGrid(*Box, Grid))
        {
            OutError = FString::Printf(TEXT("boxes[%d]: materialId and materialIds take 0 to %d, repeat count, rows and layers 1 to %d, and offsets are {x, y, z}"),
                                       Index, MAX_MATERIAL_ID, MaxRepeat);
            return false;
        }
        if (OutInput.Vertices.Num() + Grid.Num() * 8 > MaxShapePoints)
        {
            OutError = FString::Printf(TEXT("boxes come to more than %d boxes once repeated; append them in several calls"), MaxShapePoints / 8);
            return false;
        }
        const TArray<FString> Open = McpGetStringListField(*Box, TEXT("open"), TEXT("open"));
        for (int32 Copy = 0; Copy < static_cast<int32>(Grid.Num()); ++Copy)
        {
            const FVector3d Shift = Grid.At(Copy);
            AddBox(OutInput, Min + Shift, Max + Shift, Cycle.Num() > 0 ? Cycle[Copy % Cycle.Num()] : Material, Open);
        }
    }
    if (OutInput.Faces.Num() == 0)
    {
        OutError = TEXT("every face of every box is open, so nothing would be appended");
        return false;
    }
    return FinishAppendPolygonsInput(OutInput, OutError);
}

bool ParseAppendLoftInput(const TSharedPtr<FJsonObject>& Payload, FAppendPolygonsInput& OutInput, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
    if (!Payload->TryGetArrayField(TEXT("sections"), Sections) || Sections->Num() < 2 || Sections->Num() > 256)
    {
        OutError = TEXT("sections required: 2 to 256 cross sections in order, each a list of {x, y, z} points with the same count");
        return false;
    }
    const bool bClosed = GetJsonBoolField(Payload, TEXT("closed"), true);
    int32 PointCount = 0;
    for (int32 Section = 0; Section < Sections->Num(); ++Section)
    {
        const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
        const int32 Wanted = Section == 0 ? 0 : PointCount;
        if (!(*Sections)[Section]->TryGetArray(Points) || Points->Num() < (bClosed ? 3 : 2) || Points->Num() > 256 || (Wanted > 0 && Points->Num() != Wanted))
        {
            OutError = FString::Printf(TEXT("sections[%d] must hold %s points, as many as sections[0]"), Section, bClosed ? TEXT("3 to 256") : TEXT("2 to 256"));
            return false;
        }
        PointCount = Points->Num();
        if (OutInput.Vertices.Num() + PointCount + 2 > MaxShapePoints)
        {
            OutError = FString::Printf(TEXT("sections hold more than %d points; loft in several calls"), MaxShapePoints);
            return false;
        }
        for (int32 Point = 0; Point < PointCount; ++Point)
        {
            FVector3d& Added = OutInput.Vertices.AddDefaulted_GetRef();
            if (!ReadPointValue((*Points)[Point], Added))
            {
                OutError = FString::Printf(TEXT("sections[%d][%d] must be an {x, y, z} object of finite numbers"), Section, Point);
                return false;
            }
        }
    }
    const int32 SectionCount = Sections->Num();
    const int32 Edges = bClosed ? PointCount : PointCount - 1;
    TArray<int32> EdgeMaterials;
    int32 CapMaterial = 0;
    FVector3d StartPoint, EndPoint;
    bool bStartTip = false, bEndTip = false;
    if (!ReadMaterialList(Payload, TEXT("edgeMaterials"), EdgeMaterials) || (EdgeMaterials.Num() > 0 && EdgeMaterials.Num() != Edges)
        || !ReadCount(Payload, TEXT("capMaterialId"), 0, 0, MAX_MATERIAL_ID, CapMaterial)
        || !ReadOptionalPoint(Payload, TEXT("startPoint"), StartPoint, bStartTip) || !ReadOptionalPoint(Payload, TEXT("endPoint"), EndPoint, bEndTip))
    {
        OutError = FString::Printf(TEXT("edgeMaterials takes one id (0 to %d) per section edge (%d here), capMaterialId one id, and startPoint and endPoint an {x, y, z}"),
                                   MAX_MATERIAL_ID, Edges);
        return false;
    }
    if (!bClosed && (bStartTip || bEndTip))
    {
        OutError = TEXT("startPoint and endPoint close a tube's ends, so they need closed true");
        return false;
    }
    auto At = [PointCount](int32 Section, int32 Point) { return Section * PointCount + (Point % PointCount); };
    // Outward: the first section's loop normal (its vector area) against the way the sections run.
    FVector3d Normal = FVector3d::ZeroVector, First = FVector3d::ZeroVector, Last = FVector3d::ZeroVector;
    for (int32 Point = 0; Point < PointCount; ++Point)
    {
        Normal += OutInput.Vertices[At(0, Point)].Cross(OutInput.Vertices[At(0, Point + 1)]);
        First += OutInput.Vertices[At(0, Point)] / PointCount;
        Last += OutInput.Vertices[At(SectionCount - 1, Point)] / PointCount;
    }
    const bool bAlong = Normal.Dot(Last - First) > 0.0;
    const bool bReverse = bAlong != GetJsonBoolField(Payload, TEXT("flip"), false);
    for (int32 Section = 0; Section + 1 < SectionCount; ++Section)
    {
        for (int32 Edge = 0; Edge < Edges; ++Edge)
        {
            const int32 A0 = At(Section, Edge), A1 = At(Section, Edge + 1), B0 = At(Section + 1, Edge), B1 = At(Section + 1, Edge + 1);
            OutInput.Faces.Add(bReverse ? TArray<int32>{A0, A1, B1, B0} : TArray<int32>{A0, B0, B1, A1});
            OutInput.FaceMaterials.Add(EdgeMaterials.Num() > 0 ? EdgeMaterials[Edge] : 0);
        }
    }
    // Ends: a flat cap, or a fan to the tip point. The first section's loop order faces backward when !bAlong.
    for (int32 End = 0; End < 2 && bClosed; ++End)
    {
        const bool bTip = End == 0 ? bStartTip : bEndTip;
        if (!bTip && !GetJsonBoolField(Payload, TEXT("caps"), true)) continue;
        const int32 Section = End == 0 ? 0 : SectionCount - 1;
        const bool bLoopOrder = (End == 0) == !bReverse;
        if (!bTip)
        {
            TArray<int32>& Cap = OutInput.Faces.AddDefaulted_GetRef();
            for (int32 Point = 0; Point < PointCount; ++Point) Cap.Add(At(Section, bLoopOrder ? Point : PointCount - 1 - Point));
            OutInput.FaceMaterials.Add(CapMaterial);
            continue;
        }
        const int32 Tip = OutInput.Vertices.Add(End == 0 ? StartPoint : EndPoint);
        for (int32 Point = 0; Point < PointCount; ++Point)
        {
            OutInput.Faces.Add(bLoopOrder ? TArray<int32>{Tip, At(Section, Point), At(Section, Point + 1)} : TArray<int32>{Tip, At(Section, Point + 1), At(Section, Point)});
            OutInput.FaceMaterials.Add(CapMaterial);
        }
    }
    return FinishAppendPolygonsInput(OutInput, OutError);
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
