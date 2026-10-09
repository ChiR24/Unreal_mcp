#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "DynamicSubmesh3.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "Materials/MaterialInterface.h"
#include "Selections/MeshConnectedComponents.h"

// split_mesh: a merged mesh (a library pack of props imported as one asset, a kit baked together) could only be placed
// whole. Its triangles are grouped into objects and each object is saved as a static mesh of its own with the source's
// materials (compacted to the ones it uses) and a collision choice.
namespace McpGeometryHandlers
{
namespace
{
struct FMcpTriangleGroup
{
    TArray<int32> Triangles;
    FBox Box = FBox(ForceInit);
};

TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const double Axis : {Value.X, Value.Y, Value.Z})
    {
        Out.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Axis * 10.0) / 10.0));
    }
    return Out;
}

// The objects of Mesh. By material, one per material id. By parts, connected triangles first (an imported mesh is often
// not welded, so a prop arrives as thousands of loose faces), then every piece whose bounds come within Gap of another
// (a crate's planks), swept along X so ten thousand pieces stay cheap. Worked on the mesh itself: Geometry Script's split
// made one pooled mesh per piece, and past a thousand the pool let go of them all behind an ensure.
TArray<FMcpTriangleGroup> GroupTriangles(const UE::Geometry::FDynamicMesh3& Mesh, bool bByMaterial, double Gap)
{
    auto TriangleBox = [&Mesh](int32 Triangle)
    {
        const UE::Geometry::FIndex3i Corners = Mesh.GetTriangle(Triangle);
        FBox Box(ForceInit);
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Box += FVector(Mesh.GetVertex(Corners[Corner]));
        }
        return Box;
    };
    TArray<FMcpTriangleGroup> Pieces;
    if (bByMaterial)
    {
        const UE::Geometry::FDynamicMeshMaterialAttribute* Ids = Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
        TMap<int32, int32> PieceOfId;
        for (const int32 Triangle : Mesh.TriangleIndicesItr())
        {
            const int32 At = PieceOfId.FindOrAdd(Ids ? Ids->GetValue(Triangle) : 0, Pieces.Num());
            if (At == Pieces.Num())
            {
                Pieces.AddDefaulted();
            }
            Pieces[At].Triangles.Add(Triangle);
            Pieces[At].Box += TriangleBox(Triangle);
        }
        return Pieces;
    }
    UE::Geometry::FMeshConnectedComponents Components(&Mesh);
    Components.FindConnectedTriangles();
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        FMcpTriangleGroup& Piece = Pieces.AddDefaulted_GetRef();
        Piece.Triangles = Components[Index].Indices;
        for (const int32 Triangle : Piece.Triangles)
        {
            Piece.Box += TriangleBox(Triangle);
        }
    }
    TArray<int32> Root, Order;
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        Root.Add(Index);
        Order.Add(Index);
    }
    auto Find = [&Root](int32 Index) { while (Root[Index] != Index) { Root[Index] = Root[Root[Index]]; Index = Root[Index]; } return Index; };
    Order.Sort([&Pieces](int32 A, int32 B) { return Pieces[A].Box.Min.X < Pieces[B].Box.Min.X; });
    for (int32 I = 0; I < Order.Num(); ++I)
    {
        const FBox Reach = Pieces[Order[I]].Box.ExpandBy(Gap);
        for (int32 J = I + 1; J < Order.Num() && Pieces[Order[J]].Box.Min.X <= Reach.Max.X; ++J)
        {
            if (Reach.Intersect(Pieces[Order[J]].Box))
            {
                Root[Find(Order[I])] = Find(Order[J]);
            }
        }
    }
    TArray<FMcpTriangleGroup> Groups;
    TMap<int32, int32> GroupOfRoot;
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        const int32 At = GroupOfRoot.FindOrAdd(Find(Index), Groups.Num());
        if (At == Groups.Num())
        {
            Groups.AddDefaulted();
        }
        Groups[At].Triangles.Append(Pieces[Index].Triangles);
        Groups[At].Box += Pieces[Index].Box;
    }
    return Groups;
}
}

bool HandleSplitMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    UStaticMesh* Source = Cast<UStaticMesh>(McpLoadAsset(SanitizeProjectRelativePath(MeshPath)));
    const FString By = GetJsonStringField(Payload, TEXT("by"), TEXT("parts")).ToLower();
    const FString Pivot = GetJsonStringField(Payload, TEXT("pivot"), TEXT("bottom")).ToLower();
    const FString CollisionMode = GetJsonStringField(Payload, TEXT("collision"), TEXT("box")).ToLower();
    FString Problem = !Source ? FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath)
        : By != TEXT("parts") && By != TEXT("material") ? FString::Printf(TEXT("Unknown by '%s'; use parts or material."), *By)
        : Pivot != TEXT("bottom") && Pivot != TEXT("center") && Pivot != TEXT("source") ? FString::Printf(TEXT("Unknown pivot '%s'; use bottom, center or source."), *Pivot)
        : CollisionMode != TEXT("box") && CollisionMode != TEXT("complex") && CollisionMode != TEXT("none") ? FString::Printf(TEXT("Unknown collision '%s'; use box, complex or none."), *CollisionMode)
        : FString();
    if (!Problem.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, Problem, Source ? TEXT("INVALID_ARGUMENT") : TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    double MinTriangles = 12.0, MaxParts = 64.0, Gap = 2.0;
    Payload->TryGetNumberField(TEXT("minTriangles"), MinTriangles);
    Payload->TryGetNumberField(TEXT("maxParts"), MaxParts);
    Payload->TryGetNumberField(TEXT("gap"), Gap);
    bool bDropRepeats = true, bDryRun = false;
    Payload->TryGetBoolField(TEXT("dropRepeats"), bDropRepeats);
    // dryRun answers the meshes a split would make, so gap and minTriangles can be tuned before anything is created.
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    // The parts go beside the source unless outputPath names a folder; the names are checked before anything is made.
    FString Folder = GetJsonStringField(Payload, TEXT("outputPath"));
    Folder = Folder.IsEmpty() ? FPackageName::GetLongPackagePath(Source->GetOutermost()->GetName()) : SanitizeProjectRelativePath(Folder);
    Folder.RemoveFromEnd(TEXT("/"));
    const FString Prefix = GetJsonStringField(Payload, TEXT("namePrefix"), Source->GetName() + TEXT("_Part"));
    if (Folder.IsEmpty() || !FPackageName::IsValidLongPackageName(Folder / Prefix))
    {
        Self->SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("outputPath"), GetJsonStringField(Payload, TEXT("outputPath"))),
                                  TEXT("INVALID_PATH"));
        return true;
    }

    UDynamicMesh* Whole = NewObject<UDynamicMesh>();
    EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
    UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(Source, Whole, FGeometryScriptCopyMeshFromAssetOptions(),
                                                                       FGeometryScriptMeshReadLOD(), Outcome);
    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not read geometry from %s"), *MeshPath), TEXT("MESH_READ_FAILED"));
        return true;
    }
    TArray<FMcpTriangleGroup> Groups;
    Whole->ProcessMesh([&Groups, &By, Gap](const UE::Geometry::FDynamicMesh3& Mesh) { Groups = GroupTriangles(Mesh, By == TEXT("material"), Gap); });

    // Small objects (loose bits, decal cards) are left out. Largest first, so an object that only repeats a larger one
    // where it stands (an LOD copy an importer merged in) is the one dropped.
    Groups.Sort([](const FMcpTriangleGroup& A, const FMcpTriangleGroup& B) { return A.Triangles.Num() > B.Triangles.Num(); });
    TArray<const FMcpTriangleGroup*> Kept;
    int32 SkippedSmall = 0, DroppedRepeats = 0;
    for (const FMcpTriangleGroup& Group : Groups)
    {
        const bool bSmall = Group.Triangles.Num() < MinTriangles;
        const bool bRepeat = !bSmall && bDropRepeats && Kept.ContainsByPredicate([&Group](const FMcpTriangleGroup* Larger)
        {
            const double Tolerance = 0.05 * Larger->Box.GetSize().GetMax();
            return Larger->Box.Min.Equals(Group.Box.Min, Tolerance) && Larger->Box.Max.Equals(Group.Box.Max, Tolerance);
        });
        SkippedSmall += bSmall ? 1 : 0;
        DroppedRepeats += bRepeat ? 1 : 0;
        if (!bSmall && !bRepeat)
        {
            Kept.Add(&Group);
        }
    }
    TArray<FString> AssetPaths;
    for (int32 Index = 0; Index < Kept.Num(); ++Index)
    {
        AssetPaths.Add(Folder / FString::Printf(TEXT("%s%02d"), *Prefix, Index + 1));
    }
    const FString* Taken = bDryRun ? nullptr : AssetPaths.FindByPredicate([](const FString& Path) { return McpAssetExists(Path); });
    Problem = Kept.Num() == 0 ? FString::Printf(TEXT("No object has %d triangles or more (%d smaller ones were left out); lower minTriangles."),
                                                FMath::RoundToInt(MinTriangles), SkippedSmall)
        : Kept.Num() > MaxParts ? FString::Printf(TEXT("The mesh splits into %d objects, more than maxParts %d; raise gap, minTriangles or maxParts, or split by material."),
                                                  Kept.Num(), FMath::RoundToInt(MaxParts))
        : Taken ? FString::Printf(TEXT("%s already exists; give another namePrefix or outputPath."), **Taken)
        : FString();
    if (!Problem.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s Nothing was created."), *Problem),
                                  Taken ? TEXT("ASSET_EXISTS") : TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<UMaterialInterface*> SourceMaterials;
    for (const FStaticMaterial& Slot : Source->GetStaticMaterials())
    {
        SourceMaterials.Add(Slot.MaterialInterface);
    }
    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = false;
    CreateOptions.bEnableRecomputeTangents = true;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Kept.Num(); ++Index)
    {
        const FMcpTriangleGroup& Group = *Kept[Index];
        UDynamicMesh* Part = NewObject<UDynamicMesh>();
        Whole->ProcessMesh([Part, &Group](const UE::Geometry::FDynamicMesh3& Mesh)
        {
            UE::Geometry::FDynamicSubmesh3 Submesh(&Mesh, Group.Triangles);
            Part->SetMesh(MoveTemp(Submesh.GetSubmesh()));
        });
        // Each part keeps only the slots it draws with, in the source's order.
        TArray<UMaterialInterface*> PartMaterials;
        UGeometryScriptLibrary_MeshMaterialFunctions::CompactMaterialIDs(Part, SourceMaterials, PartMaterials);
        const FVector Center = Group.Box.GetCenter();
        const FVector PivotAt = Pivot == TEXT("source") ? FVector::ZeroVector
            : Pivot == TEXT("center") ? Center : FVector(Center.X, Center.Y, Group.Box.Min.Z);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("assetPath"), AssetPaths[Index]);
        Row->SetNumberField(TEXT("triangles"), Group.Triangles.Num());
        Row->SetArrayField(TEXT("size"), VectorJson(Group.Box.GetSize()));
        // Where the part's pivot sat in the source: a copy placed at the source actor's transform times this offset
        // stands where the part stood.
        Row->SetArrayField(TEXT("offset"), VectorJson(PivotAt));
        Row->SetNumberField(TEXT("slots"), FMath::Max(1, PartMaterials.Num()));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        if (bDryRun)
        {
            Row->SetBoolField(TEXT("exists"), McpAssetExists(AssetPaths[Index]));
            continue;
        }
        UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Part, -PivotAt);
        UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(Part, AssetPaths[Index], CreateOptions, Outcome, nullptr);
        UStaticMesh* Created = Outcome == EGeometryScriptOutcomePins::Success
            ? Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *AssetPaths[Index])) : nullptr;
        if (!Created)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not create %s; the %d part(s) before it were created."),
                                                                         *AssetPaths[Index], Index), TEXT("ASSET_CREATION_FAILED"));
            return true;
        }
        ApplyConversionMaterials(Created, PartMaterials, FMath::Max(1, PartMaterials.Num()));
        ApplyConversionCollision(Created, CollisionMode);
        Row->SetStringField(TEXT("assetPath"), Created->GetPathName());
        if (!McpSafeAssetSave(Created))
        {
            Row->SetBoolField(TEXT("saved"), false);
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("meshPath"), Source->GetPathName());
    Result->SetArrayField(TEXT("meshes"), Rows);
    Result->SetNumberField(TEXT("skippedSmall"), SkippedSmall);
    Result->SetNumberField(TEXT("droppedRepeats"), DroppedRepeats);
    Self->SendAutomationResponse(Socket, RequestId, true, bDryRun
        ? FString::Printf(TEXT("%s would split into %d static meshes; dryRun created nothing."), *Source->GetName(), Rows.Num())
        : FString::Printf(TEXT("Split %s into %d static meshes."), *Source->GetName(), Rows.Num()), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
