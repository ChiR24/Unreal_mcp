// Copyright (c) 2024 EngineWire Contributors
//
// What an assemble_modules call asks for (McpModuleKit.h): its parameters read and checked, and every piece's faces read
// at each turn it may take, in parallel and against a deadline.

#include "Domains/Environment/Modules/McpModuleKit.h"

#include "Async/ParallelFor.h"
#include "Dom/JsonObject.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "HAL/PlatformTime.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"

#include <atomic>

namespace McpModuleAssembly
{
using namespace McpModuleKit;

namespace
{
constexpr int32 ModuleMaxCells = 4096;
} // namespace

bool ReadRequest(const TSharedPtr<FJsonObject>& Payload, FModuleRequest& Out, FString& Why, FString& Code)
{
    Code = TEXT("INVALID_ARGUMENT");
    Out.Cell = ExtractVectorField(Payload, TEXT("cellSize"), FVector::ZeroVector);
    const FVector Size = ExtractVectorField(Payload, TEXT("size"), FVector::ZeroVector);
    Out.In.Size = FIntVector(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y), FMath::RoundToInt(Size.Z));
    const FString Pivot = GetJsonStringField(Payload, TEXT("pivot")).ToLower();
    Out.EmptyWeight = GetJsonNumberField(Payload, TEXT("emptyWeight"), 1.0);
    Payload->TryGetBoolField(TEXT("rotations"), Out.bTurn);
    Out.In.Seed = FMath::RoundToInt(GetJsonNumberField(Payload, TEXT("seed"), 1.0));
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    Why = Out.Cell.X <= 0.0 || Out.Cell.Y <= 0.0 || Out.Cell.Z <= 0.0 ? FString(TEXT("cellSize must be {x, y, z} above 0, in cm"))
        : Pivot != TEXT("corner") && Pivot != TEXT("center") && Pivot != TEXT("bottom_center") ? FString(TEXT("pivot must be corner, center or bottom_center"))
        : Out.In.Size.X < 1 || Out.In.Size.Y < 1 || Out.In.Size.Z < 1 || int64(Out.In.Size.X) * Out.In.Size.Y * Out.In.Size.Z > ModuleMaxCells
            ? FString::Printf(TEXT("size must be at least 1 cell a side and %d cells in all"), ModuleMaxCells)
        : !Payload->TryGetArrayField(TEXT("modules"), Entries) || !Entries || Entries->Num() < 1 || Entries->Num() > 32
            ? FString(TEXT("modules must list 1-32 pieces {meshPath, weight}"))
        : Out.EmptyWeight < 0.0 ? FString(TEXT("emptyWeight must be 0 or more")) : FString();
    Out.Centre = Pivot == TEXT("corner") ? Out.Cell / 2.0 : Pivot == TEXT("bottom_center") ? FVector(0.0, 0.0, Out.Cell.Z / 2.0) : FVector::ZeroVector;
    for (int32 Index = 0; Why.IsEmpty() && Index < Entries->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        const FString Path = (*Entries)[Index]->TryGetObject(Entry) && Entry ? GetJsonStringField(*Entry, TEXT("meshPath")) : FString();
        const FString Safe = SanitizeProjectRelativePath(Path);
        FModulePiece& Piece = Out.Pieces.AddDefaulted_GetRef();
        Piece.Mesh = Safe.IsEmpty() ? nullptr : Cast<UStaticMesh>(McpLoadAsset(Safe));
        Piece.Weight = Entry ? GetJsonNumberField(*Entry, TEXT("weight"), 1.0) : 1.0;
        if (!Piece.Mesh || !Piece.Mesh->GetMeshDescription(0))
        {
            Why = FString::Printf(TEXT("modules[%d]: '%s' is not a static mesh with source geometry"), Index, *Path);
            Code = TEXT("MESH_NOT_FOUND");
        }
        Why = Why.IsEmpty() && Piece.Weight <= 0.0 ? FString::Printf(TEXT("modules[%d]: weight must be above 0"), Index) : Why;
    }
    // Which sides of the grid anything may face out of; the rest take only air. By default the floor: pieces stand on it.
    TArray<FString> Open = {TEXT("-Z")};
    const TArray<TSharedPtr<FJsonValue>>* Sides = nullptr;
    if (Payload->TryGetArrayField(TEXT("openSides"), Sides) && Sides)
    {
        Open.Reset();
        for (const TSharedPtr<FJsonValue>& Side : *Sides)
        {
            Open.Add(Side.IsValid() ? Side->AsString().ToUpper() : FString());
        }
    }
    for (int32 Side = 0; Side < 6; ++Side)
    {
        Out.In.OpenSide[Side] = Open.Contains(SideName(Side));
    }
    for (const FString& Side : Open)
    {
        bool bKnown = false;
        for (int32 Index = 0; Index < 6; ++Index)
        {
            bKnown = bKnown || Side == SideName(Index);
        }
        Why = Why.IsEmpty() && !bKnown ? FString::Printf(TEXT("openSides: '%s' is not +X, -X, +Y, -Y, +Z or -Z"), *Side) : Why;
    }
    return Why.IsEmpty();
}


bool BuildVariants(FModuleRequest& Request, double Deadline, FString& OutWhy)
{
    const FVector& Cell = Request.Cell;
    const TArray<int32> Turns = !Request.bTurn ? TArray<int32>{0} : FMath::IsNearlyEqual(Cell.X, Cell.Y) ? TArray<int32>{0, 1, 2, 3} : TArray<int32>{0, 2};
    // The meshes stay where they are while their faces are read: each tree points at its mesh.
    TArray<TUniquePtr<UE::Geometry::FDynamicMesh3>> Shapes;
    TArray<TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3>> Trees;
    for (int32 Index = 0; Index < Request.Pieces.Num(); ++Index)
    {
        UE::Geometry::FDynamicMesh3& Shape = *Shapes.Add_GetRef(MakeUnique<UE::Geometry::FDynamicMesh3>());
        FMeshDescriptionToDynamicMesh Converter;
        Converter.Convert(Request.Pieces[Index].Mesh->GetMeshDescription(0), Shape);
        // A piece reaching outside its cell box was made for another cell size or pivot: every layout would be shifted.
        const UE::Geometry::FAxisAlignedBox3d Bounds = Shape.GetBounds();
        const FVector Low = Request.Centre - Cell * 0.51, High = Request.Centre + Cell * 0.51;
        if (Shape.TriangleCount() == 0 || Bounds.Min.X < Low.X || Bounds.Min.Y < Low.Y || Bounds.Min.Z < Low.Z ||
            Bounds.Max.X > High.X || Bounds.Max.Y > High.Y || Bounds.Max.Z > High.Z)
        {
            OutWhy = FString::Printf(TEXT("'%s' (bounds %s to %s) does not fit the cell box %s to %s that cellSize and pivot give it"),
                                     *Request.Pieces[Index].Mesh->GetPathName(), *FVector(Bounds.Min).ToCompactString(),
                                     *FVector(Bounds.Max).ToCompactString(), *(Request.Centre - Cell / 2.0).ToCompactString(),
                                     *(Request.Centre + Cell / 2.0).ToCompactString());
            return false;
        }
        Trees.Add(MakeUnique<UE::Geometry::FDynamicMeshAABBTree3>(&Shape, true));
        for (const int32 Turn : Turns)
        {
            FModuleVariant& Variant = Request.In.Variants.AddDefaulted_GetRef();
            Variant.Module = Index;
            Variant.Turns = Turn;
            Variant.Weight = Request.Pieces[Index].Weight / Turns.Num();
        }
    }
    // Each variant's faces on a thread of their own (the trees are only queried); none starts past the deadline.
    std::atomic<bool> bLate(false);
    ParallelFor(Request.In.Variants.Num(), [&](int32 Index)
    {
        FModuleVariant& Variant = Request.In.Variants[Index];
        if (bLate || FPlatformTime::Seconds() > Deadline)
        {
            bLate = true;
            return;
        }
        SampleFaces(*Trees[Variant.Module], Cell, CellFromMesh(Request.Centre, Variant.Turns), Variant.Faces);
    });
    if (bLate)
    {
        OutWhy = TEXT("reading the pieces' faces ran out of time: use fewer pieces, or pieces with fewer triangles");
        return false;
    }
    if (Request.EmptyWeight > 0.0)
    {
        Request.In.Variants.AddDefaulted_GetRef().Weight = Request.EmptyWeight;
    }
    return true;
}
} // namespace McpModuleAssembly
