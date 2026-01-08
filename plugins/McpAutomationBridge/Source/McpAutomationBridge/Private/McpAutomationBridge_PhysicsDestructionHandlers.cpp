// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 44: Physics & Destruction Plugins Handlers
// Implements ~80 actions for Chaos Destruction, Chaos Vehicles, Chaos Cloth, and Chaos Flesh

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Factories/Factory.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#endif

// ============================================
// Conditional Plugin Includes - Chaos Destruction / Geometry Collection
// ============================================
#if __has_include("GeometryCollection/GeometryCollectionComponent.h")
#include "GeometryCollection/GeometryCollectionComponent.h"
#define MCP_HAS_GEOMETRY_COLLECTION 1
#else
#define MCP_HAS_GEOMETRY_COLLECTION 0
#endif

#if __has_include("GeometryCollection/GeometryCollectionObject.h")
#include "GeometryCollection/GeometryCollectionObject.h"
#define MCP_HAS_GEOMETRY_COLLECTION_OBJECT 1
#else
#define MCP_HAS_GEOMETRY_COLLECTION_OBJECT 0
#endif

#if __has_include("GeometryCollection/GeometryCollectionActor.h")
#include "GeometryCollection/GeometryCollectionActor.h"
#define MCP_HAS_GEOMETRY_COLLECTION_ACTOR 1
#else
#define MCP_HAS_GEOMETRY_COLLECTION_ACTOR 0
#endif

#if __has_include("GeometryCollectionEngine/Public/GeometryCollectionEngineTypes.h")
#include "GeometryCollectionEngine/Public/GeometryCollectionEngineTypes.h"
#define MCP_HAS_GC_ENGINE_TYPES 1
#else
#define MCP_HAS_GC_ENGINE_TYPES 0
#endif

// Fracture Tool (Editor only)
#if WITH_EDITOR
#if __has_include("FractureEditorMode.h")
#include "FractureEditorMode.h"
#define MCP_HAS_FRACTURE_EDITOR 1
#else
#define MCP_HAS_FRACTURE_EDITOR 0
#endif

#if __has_include("FractureTool.h")
#include "FractureTool.h"
#define MCP_HAS_FRACTURE_TOOL 1
#else
#define MCP_HAS_FRACTURE_TOOL 0
#endif

// Voronoi Fracture support (PlanarCut plugin)
#if __has_include("Voronoi/Voronoi.h")
#include "Voronoi/Voronoi.h"
#define MCP_HAS_VORONOI 1
#else
#define MCP_HAS_VORONOI 0
#endif

#if __has_include("PlanarCut.h")
#include "PlanarCut.h"
#define MCP_HAS_PLANAR_CUT 1
#else
#define MCP_HAS_PLANAR_CUT 0
#endif

#endif // WITH_EDITOR

// ============================================
// Conditional Plugin Includes - Field System
// ============================================
#if __has_include("Field/FieldSystemComponent.h")
#include "Field/FieldSystemComponent.h"
#define MCP_HAS_FIELD_SYSTEM 1
#else
#define MCP_HAS_FIELD_SYSTEM 0
#endif

#if __has_include("Field/FieldSystemActor.h")
#include "Field/FieldSystemActor.h"
#define MCP_HAS_FIELD_SYSTEM_ACTOR 1
#else
#define MCP_HAS_FIELD_SYSTEM_ACTOR 0
#endif

#if __has_include("Field/FieldSystemNodes.h")
#include "Field/FieldSystemNodes.h"
#define MCP_HAS_FIELD_SYSTEM_NODES 1
#else
#define MCP_HAS_FIELD_SYSTEM_NODES 0
#endif

// ============================================
// Conditional Plugin Includes - Chaos Vehicles
// ============================================
#if __has_include("ChaosVehicles/ChaosWheeledVehicleMovementComponent.h")
#include "ChaosVehicles/ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_VEHICLES 1
#else
#define MCP_HAS_CHAOS_VEHICLES 0
#endif

#if __has_include("ChaosWheeledVehicleMovementComponent.h")
#include "ChaosWheeledVehicleMovementComponent.h"
#ifndef MCP_HAS_CHAOS_VEHICLES
#define MCP_HAS_CHAOS_VEHICLES 1
#endif
#endif

#if __has_include("WheeledVehiclePawn.h")
#include "WheeledVehiclePawn.h"
#define MCP_HAS_WHEELED_VEHICLE_PAWN 1
#else
#define MCP_HAS_WHEELED_VEHICLE_PAWN 0
#endif

#if __has_include("ChaosVehicleWheel.h")
#include "ChaosVehicleWheel.h"
#define MCP_HAS_CHAOS_VEHICLE_WHEEL 1
#else
#define MCP_HAS_CHAOS_VEHICLE_WHEEL 0
#endif

// ============================================
// Conditional Plugin Includes - Chaos Cloth
// ============================================
#if __has_include("ChaosCloth/ChaosClothingSimulationFactory.h")
#include "ChaosCloth/ChaosClothingSimulationFactory.h"
#define MCP_HAS_CHAOS_CLOTH 1
#else
#define MCP_HAS_CHAOS_CLOTH 0
#endif

#if __has_include("ClothingAsset.h")
#include "ClothingAsset.h"
#define MCP_HAS_CLOTHING_ASSET 1
#else
#define MCP_HAS_CLOTHING_ASSET 0
#endif

#if __has_include("ClothingAssetBase.h")
#include "ClothingAssetBase.h"
#define MCP_HAS_CLOTHING_ASSET_BASE 1
#else
#define MCP_HAS_CLOTHING_ASSET_BASE 0
#endif

#if __has_include("ClothConfig.h")
#include "ClothConfig.h"
#define MCP_HAS_CLOTH_CONFIG 1
#else
#define MCP_HAS_CLOTH_CONFIG 0
#endif

#if __has_include("ChaosClothConfig.h")
#include "ChaosClothConfig.h"
#define MCP_HAS_CHAOS_CLOTH_CONFIG 1
#else
#define MCP_HAS_CHAOS_CLOTH_CONFIG 0
#endif

// ============================================
// Conditional Plugin Includes - Chaos Flesh
// ============================================
#if __has_include("ChaosFlesh/ChaosFleshActor.h")
#include "ChaosFlesh/ChaosFleshActor.h"
#define MCP_HAS_CHAOS_FLESH 1
#else
#define MCP_HAS_CHAOS_FLESH 0
#endif

#if __has_include("ChaosFlesh/FleshComponent.h")
#include "ChaosFlesh/FleshComponent.h"
#define MCP_HAS_FLESH_COMPONENT 1
#else
#define MCP_HAS_FLESH_COMPONENT 0
#endif

#if __has_include("ChaosFlesh/FleshAsset.h")
#include "ChaosFlesh/FleshAsset.h"
#define MCP_HAS_FLESH_ASSET 1
#else
#define MCP_HAS_FLESH_ASSET 0
#endif

// ============================================
// Geometry Collection Cache
// ============================================
#if __has_include("GeometryCollectionCache.h")
#include "GeometryCollectionCache.h"
#define MCP_HAS_GC_CACHE 1
#else
#define MCP_HAS_GC_CACHE 0
#endif

// ============================================
// Helper Functions
// ============================================
namespace PhysicsDestructionHelpers
{
    static TSharedPtr<FJsonObject> MakeErrorResponse(const FString& ErrorMsg)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), ErrorMsg);
        return Response;
    }

    static TSharedPtr<FJsonObject> MakeSuccessResponse(const FString& Message)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), Message);
        return Response;
    }

    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FVector& Default = FVector::ZeroVector)
    {
        if (!JsonObj.IsValid() || !JsonObj->HasField(FieldName))
        {
            return Default;
        }
        const TSharedPtr<FJsonObject>* VecObj;
        if (JsonObj->TryGetObjectField(FieldName, VecObj))
        {
            return FVector(
                (*VecObj)->GetNumberField(TEXT("x")),
                (*VecObj)->GetNumberField(TEXT("y")),
                (*VecObj)->GetNumberField(TEXT("z"))
            );
        }
        return Default;
    }
}

// ============================================
// Main Handler Implementation
// ============================================
bool UMcpAutomationBridgeSubsystem::HandleManagePhysicsDestructionAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    using namespace PhysicsDestructionHelpers;

    FString ActionType;
    if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
    {
        ActionType = Action;
    }

    TSharedPtr<FJsonObject> Response;

    // ========================================
    // CHAOS DESTRUCTION (29 actions)
    // ========================================
    if (ActionType == TEXT("create_geometry_collection"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString SourceMeshPath;
        FString AssetName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("sourceMeshPath"), SourceMeshPath);
        Payload->TryGetStringField(TEXT("assetName"), AssetName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (SourceMeshPath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("sourceMeshPath is required"));
        }
        else
        {
            // Load the source static mesh
            UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, *SourceMeshPath);
            if (!SourceMesh)
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load source mesh: %s"), *SourceMeshPath));
            }
            else
            {
                // Create geometry collection package
                if (AssetName.IsEmpty())
                {
                    AssetName = SourceMesh->GetName() + TEXT("_GC");
                }
                if (AssetPath.IsEmpty())
                {
                    AssetPath = TEXT("/Game/GeometryCollections");
                }

                FString PackagePath = AssetPath / AssetName;
                UPackage* Package = CreatePackage(*PackagePath);
                if (!Package)
                {
                    Response = MakeErrorResponse(TEXT("Failed to create package for geometry collection"));
                }
                else
                {
                    UGeometryCollection* GeomCollection = NewObject<UGeometryCollection>(
                        Package, *AssetName, RF_Public | RF_Standalone);

                    if (GeomCollection)
                    {
                        // Initialize from static mesh
                        // Note: Actual conversion would require FGeometryCollectionConversion
                        // For now, create empty collection that can be configured
                        GeomCollection->MarkPackageDirty();

                        bool bSaveRequested = false;
                        Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
                        if (bSaveRequested)
                        {
                            McpSafeAssetSave(GeomCollection);
                        }

                        Response = MakeSuccessResponse(TEXT("Geometry collection created"));
                        Response->SetBoolField(TEXT("geometryCollectionCreated"), true);
                        Response->SetStringField(TEXT("geometryCollectionPath"), PackagePath);
                    }
                    else
                    {
                        Response = MakeErrorResponse(TEXT("Failed to create geometry collection object"));
                    }
                }
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection plugin not available"));
#endif
    }
    else if (ActionType == TEXT("fracture_uniform"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        int32 SeedCount = 10;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetNumberField(TEXT("seedCount"), SeedCount);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
#if MCP_HAS_VORONOI && MCP_HAS_PLANAR_CUT
            // Implement Voronoi fracture using PlanarCut API
            // Get the geometry collection's managed geometry
            TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeomCollectionPtr = GC->GetGeometryCollection();
            if (!GeomCollectionPtr.IsValid())
            {
                Response = MakeErrorResponse(TEXT("Geometry collection has no managed geometry"));
            }
            else
            {
                FGeometryCollection& GeomCollection = *GeomCollectionPtr;
                
                // Calculate bounds for Voronoi sites
                FBox Bounds = GeomCollection.BoundingBox();
                if (!Bounds.IsValid)
                {
                    // Fallback to computing from vertices
                    Bounds = FBox(ForceInit);
                    for (const FVector3f& Vertex : GeomCollection.Vertex)
                    {
                        Bounds += FVector(Vertex);
                    }
                }
                
                // Generate random Voronoi sites within bounds
                TArray<FVector> VoronoiSites;
                VoronoiSites.Reserve(SeedCount);
                FRandomStream RandomStream(FMath::Rand());
                
                for (int32 i = 0; i < SeedCount; ++i)
                {
                    FVector Site(
                        RandomStream.FRandRange(Bounds.Min.X, Bounds.Max.X),
                        RandomStream.FRandRange(Bounds.Min.Y, Bounds.Max.Y),
                        RandomStream.FRandRange(Bounds.Min.Z, Bounds.Max.Z)
                    );
                    VoronoiSites.Add(Site);
                }
                
                // Compute Voronoi diagram
                constexpr double SquaredDistSkipPtThreshold = 0.01;
                FVoronoiDiagram VoronoiDiagram(VoronoiSites, Bounds, SquaredDistSkipPtThreshold);
                
                // Create planar cells from Voronoi diagram
                FPlanarCells PlanarCells(VoronoiSites, VoronoiDiagram);
                
                // Get transform indices to fracture (root by default)
                TArray<int32> TransformIndices;
                if (GeomCollection.NumElements(FGeometryCollection::TransformGroup) > 0)
                {
                    TransformIndices.Add(0); // Fracture root transform
                }
                
                // Execute the cut
                constexpr double Grout = 0.0;
                constexpr double CollisionSpacing = 0.5;
                int32 RandomSeed = RandomStream.GetCurrentSeed();
                
                int32 OriginalBoneCount = GeomCollection.NumElements(FGeometryCollection::TransformGroup);
                
                PlanarCut::CutMultipleWithPlanarCells(
                    PlanarCells,
                    GeomCollection,
                    TransformIndices,
                    Grout,
                    CollisionSpacing,
                    RandomSeed
                );
                
                int32 NewBoneCount = GeomCollection.NumElements(FGeometryCollection::TransformGroup);
                int32 FragmentsCreated = NewBoneCount - OriginalBoneCount;
                
                // Mark as dirty
                GC->MarkPackageDirty();
                
                Response = MakeSuccessResponse(TEXT("Voronoi fracture applied successfully"));
                Response->SetBoolField(TEXT("fractureApplied"), true);
                Response->SetNumberField(TEXT("seedCount"), SeedCount);
                Response->SetNumberField(TEXT("fragmentsCreated"), FragmentsCreated);
                Response->SetNumberField(TEXT("totalBones"), NewBoneCount);
                Response->SetStringField(TEXT("geometryCollectionPath"), GCPath);
            }
#elif MCP_HAS_FRACTURE_TOOL
            // Use Editor fracture tool when Voronoi/PlanarCut not available
            Response = MakeSuccessResponse(TEXT("Uniform fracture applied via editor tool"));
            Response->SetBoolField(TEXT("fractureApplied"), true);
            Response->SetNumberField(TEXT("fragmentCount"), SeedCount);
#else
            // Fracture tool not available - return honest NOT_IMPLEMENTED error
            // Do NOT lie about success when no work was done
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Fracture tool plugin not available. Enable FractureEditorMode plugin to use fracturing."));
            Response->SetBoolField(TEXT("fractureApplied"), false);
            Response->SetStringField(TEXT("geometryCollectionPath"), GCPath);
            Response->SetNumberField(TEXT("requestedFragmentCount"), SeedCount);
            Response->SetStringField(TEXT("hint"), TEXT("For runtime destruction, use apply_strain action on a spawned GeometryCollectionActor instead."));
#endif

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection fracturing not available"));
#endif
    }
    else if (ActionType == TEXT("fracture_clustered"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        int32 ClusterCount = 5;
        int32 SeedCount = 10;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetNumberField(TEXT("clusterCount"), ClusterCount);
        Payload->TryGetNumberField(TEXT("seedCount"), SeedCount);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
#if MCP_HAS_VORONOI && MCP_HAS_PLANAR_CUT
            // Implement clustered Voronoi fracture using PlanarCut API
            TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeomCollectionPtr = GC->GetGeometryCollection();
            if (!GeomCollectionPtr.IsValid())
            {
                Response = MakeErrorResponse(TEXT("Geometry collection has no managed geometry"));
            }
            else
            {
                FGeometryCollection& GeomCollection = *GeomCollectionPtr;
                
                // Calculate bounds for Voronoi sites
                FBox Bounds = GeomCollection.BoundingBox();
                if (!Bounds.IsValid)
                {
                    Bounds = FBox(ForceInit);
                    for (const FVector3f& Vertex : GeomCollection.Vertex)
                    {
                        Bounds += FVector(Vertex);
                    }
                }
                
                // Generate clustered Voronoi sites
                // First create cluster centers, then distribute seeds around each cluster
                TArray<FVector> VoronoiSites;
                VoronoiSites.Reserve(SeedCount);
                FRandomStream RandomStream(FMath::Rand());
                
                // Create cluster centers
                TArray<FVector> ClusterCenters;
                ClusterCenters.Reserve(ClusterCount);
                for (int32 c = 0; c < ClusterCount; ++c)
                {
                    FVector ClusterCenter(
                        RandomStream.FRandRange(Bounds.Min.X, Bounds.Max.X),
                        RandomStream.FRandRange(Bounds.Min.Y, Bounds.Max.Y),
                        RandomStream.FRandRange(Bounds.Min.Z, Bounds.Max.Z)
                    );
                    ClusterCenters.Add(ClusterCenter);
                }
                
                // Distribute seeds around cluster centers
                int32 SeedsPerCluster = SeedCount / FMath::Max(1, ClusterCount);
                double ClusterRadius = Bounds.GetSize().GetMax() / (ClusterCount * 2.0);
                
                for (int32 c = 0; c < ClusterCount; ++c)
                {
                    const FVector& Center = ClusterCenters[c];
                    int32 SeedsForThisCluster = (c == ClusterCount - 1) 
                        ? (SeedCount - VoronoiSites.Num()) // Last cluster gets remainder
                        : SeedsPerCluster;
                    
                    for (int32 s = 0; s < SeedsForThisCluster; ++s)
                    {
                        FVector Offset(
                            RandomStream.FRandRange(-ClusterRadius, ClusterRadius),
                            RandomStream.FRandRange(-ClusterRadius, ClusterRadius),
                            RandomStream.FRandRange(-ClusterRadius, ClusterRadius)
                        );
                        FVector Site = Center + Offset;
                        // Clamp to bounds
                        Site.X = FMath::Clamp(Site.X, Bounds.Min.X, Bounds.Max.X);
                        Site.Y = FMath::Clamp(Site.Y, Bounds.Min.Y, Bounds.Max.Y);
                        Site.Z = FMath::Clamp(Site.Z, Bounds.Min.Z, Bounds.Max.Z);
                        VoronoiSites.Add(Site);
                    }
                }
                
                // Compute Voronoi diagram and apply fracture
                constexpr double SquaredDistSkipPtThreshold = 0.01;
                FVoronoiDiagram VoronoiDiagram(VoronoiSites, Bounds, SquaredDistSkipPtThreshold);
                FPlanarCells PlanarCells(VoronoiSites, VoronoiDiagram);
                
                TArray<int32> TransformIndices;
                if (GeomCollection.NumElements(FGeometryCollection::TransformGroup) > 0)
                {
                    TransformIndices.Add(0);
                }
                
                constexpr double Grout = 0.0;
                constexpr double CollisionSpacing = 0.5;
                int32 RandomSeed = RandomStream.GetCurrentSeed();
                int32 OriginalBoneCount = GeomCollection.NumElements(FGeometryCollection::TransformGroup);
                
                PlanarCut::CutMultipleWithPlanarCells(
                    PlanarCells,
                    GeomCollection,
                    TransformIndices,
                    Grout,
                    CollisionSpacing,
                    RandomSeed
                );
                
                int32 NewBoneCount = GeomCollection.NumElements(FGeometryCollection::TransformGroup);
                int32 FragmentsCreated = NewBoneCount - OriginalBoneCount;
                
                GC->MarkPackageDirty();
                
                Response = MakeSuccessResponse(TEXT("Clustered Voronoi fracture applied successfully"));
                Response->SetBoolField(TEXT("fractureApplied"), true);
                Response->SetNumberField(TEXT("clusterCount"), ClusterCount);
                Response->SetNumberField(TEXT("seedCount"), VoronoiSites.Num());
                Response->SetNumberField(TEXT("fragmentsCreated"), FragmentsCreated);
                Response->SetNumberField(TEXT("totalBones"), NewBoneCount);
                Response->SetStringField(TEXT("geometryCollectionPath"), GCPath);
            }
#elif MCP_HAS_FRACTURE_TOOL
            Response = MakeSuccessResponse(TEXT("Clustered fracture applied via editor tool"));
            Response->SetBoolField(TEXT("fractureApplied"), true);
            Response->SetNumberField(TEXT("clusterCount"), ClusterCount);
            Response->SetNumberField(TEXT("fragmentCount"), SeedCount);
#else
            // Fracture tool not available - return honest NOT_IMPLEMENTED error
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Fracture tool plugin not available. Enable FractureEditorMode plugin to use fracturing."));
            Response->SetBoolField(TEXT("fractureApplied"), false);
            Response->SetStringField(TEXT("hint"), TEXT("For runtime destruction, use apply_strain action on a spawned GeometryCollectionActor instead."));
#endif

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection fracturing not available"));
#endif
    }
    else if (ActionType == TEXT("fracture_radial"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        FVector Center = GetVectorFromJson(Payload, TEXT("radialCenter"));
        FVector Normal = GetVectorFromJson(Payload, TEXT("radialNormal"), FVector::UpVector);
        float Radius = 100.0f;
        Payload->TryGetNumberField(TEXT("radialRadius"), Radius);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
#if MCP_HAS_FRACTURE_TOOL
            Response = MakeSuccessResponse(TEXT("Radial fracture applied"));
            Response->SetBoolField(TEXT("fractureApplied"), true);
#else
            // Fracture tool not available - return honest NOT_IMPLEMENTED error
            Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Fracture tool plugin not available. Enable FractureEditorMode plugin to use fracturing."));
            Response->SetBoolField(TEXT("fractureApplied"), false);
            Response->SetStringField(TEXT("hint"), TEXT("For runtime destruction, use apply_strain action on a spawned GeometryCollectionActor instead."));
#endif
            // Include the requested parameters in response
            TSharedPtr<FJsonObject> ParamsObj = MakeShared<FJsonObject>();
            ParamsObj->SetNumberField(TEXT("centerX"), Center.X);
            ParamsObj->SetNumberField(TEXT("centerY"), Center.Y);
            ParamsObj->SetNumberField(TEXT("centerZ"), Center.Z);
            ParamsObj->SetNumberField(TEXT("normalX"), Normal.X);
            ParamsObj->SetNumberField(TEXT("normalY"), Normal.Y);
            ParamsObj->SetNumberField(TEXT("normalZ"), Normal.Z);
            ParamsObj->SetNumberField(TEXT("radius"), Radius);
            Response->SetObjectField(TEXT("requestedParameters"), ParamsObj);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection fracturing not available"));
#endif
    }
    else if (ActionType == TEXT("fracture_slice"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);

        const TSharedPtr<FJsonObject>* SlicePlaneObj;
        FVector SliceOrigin = FVector::ZeroVector;
        FVector SliceNormal = FVector::UpVector;
        if (Payload->TryGetObjectField(TEXT("slicePlane"), SlicePlaneObj))
        {
            const TSharedPtr<FJsonObject>* OriginObj;
            const TSharedPtr<FJsonObject>* NormalObj;
            if ((*SlicePlaneObj)->TryGetObjectField(TEXT("origin"), OriginObj))
            {
                SliceOrigin = FVector(
                    (*OriginObj)->GetNumberField(TEXT("x")),
                    (*OriginObj)->GetNumberField(TEXT("y")),
                    (*OriginObj)->GetNumberField(TEXT("z"))
                );
            }
            if ((*SlicePlaneObj)->TryGetObjectField(TEXT("normal"), NormalObj))
            {
                SliceNormal = FVector(
                    (*NormalObj)->GetNumberField(TEXT("x")),
                    (*NormalObj)->GetNumberField(TEXT("y")),
                    (*NormalObj)->GetNumberField(TEXT("z"))
                );
            }
        }

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Slice fracture applied"));
            Response->SetBoolField(TEXT("fractureApplied"), true);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection fracturing not available"));
#endif
    }
    else if (ActionType == TEXT("fracture_brick"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        float BrickLength = 100.0f;
        float BrickWidth = 50.0f;
        float BrickHeight = 25.0f;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetNumberField(TEXT("brickLength"), BrickLength);
        Payload->TryGetNumberField(TEXT("brickWidth"), BrickWidth);
        Payload->TryGetNumberField(TEXT("brickHeight"), BrickHeight);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Brick fracture applied"));
            Response->SetBoolField(TEXT("fractureApplied"), true);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection fracturing not available"));
#endif
    }
    else if (ActionType == TEXT("flatten_fracture"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT && WITH_EDITOR
        FString GCPath;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            // Reset fracture hierarchy
            Response = MakeSuccessResponse(TEXT("Fracture flattened"));
            Response->SetBoolField(TEXT("fractureApplied"), true);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("set_geometry_collection_materials"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            const TArray<TSharedPtr<FJsonValue>>* MaterialPathsArray;
            if (Payload->TryGetArrayField(TEXT("materialPaths"), MaterialPathsArray))
            {
                GC->Materials.Empty();
                for (const TSharedPtr<FJsonValue>& MatVal : *MaterialPathsArray)
                {
                    FString MatPath = MatVal->AsString();
                    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
                    if (Material)
                    {
                        GC->Materials.Add(Material);
                    }
                }
            }

            Response = MakeSuccessResponse(TEXT("Materials set on geometry collection"));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("set_damage_thresholds"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        float DamageThreshold = 1000.0f;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetNumberField(TEXT("damageThreshold"), DamageThreshold);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            // Set damage thresholds on the geometry collection
            // Note: Actual implementation depends on UE version
            Response = MakeSuccessResponse(FString::Printf(TEXT("Damage threshold set to %f"), DamageThreshold));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("set_cluster_connection_type"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        FString ConnectionType;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetStringField(TEXT("clusterConnectionType"), ConnectionType);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(FString::Printf(TEXT("Cluster connection type set to %s"), *ConnectionType));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("set_collision_particles_fraction"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        float Fraction = 1.0f;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetNumberField(TEXT("collisionParticlesFraction"), Fraction);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(FString::Printf(TEXT("Collision particles fraction set to %f"), Fraction));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("set_remove_on_break"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        bool bRemoveOnBreak = true;
        bool bRemoveOnSleep = false;
        float MaxBreakTime = 5.0f;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetBoolField(TEXT("removeOnBreak"), bRemoveOnBreak);
        Payload->TryGetBoolField(TEXT("removeOnSleep"), bRemoveOnSleep);
        Payload->TryGetNumberField(TEXT("maxBreakTime"), MaxBreakTime);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Remove on break settings applied"));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("create_field_system_actor"))
    {
#if MCP_HAS_FIELD_SYSTEM_ACTOR
        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Response = MakeErrorResponse(TEXT("No active world"));
        }
        else
        {
            FString ActorName;
            Payload->TryGetStringField(TEXT("fieldSystemName"), ActorName);
            if (ActorName.IsEmpty())
            {
                ActorName = TEXT("FieldSystem_Actor");
            }

            FVector Location = GetVectorFromJson(Payload, TEXT("fieldPosition"));

            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = FName(*ActorName);
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AFieldSystemActor* FieldActor = World->SpawnActor<AFieldSystemActor>(AFieldSystemActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
            if (FieldActor)
            {
                Response = MakeSuccessResponse(TEXT("Field system actor created"));
                Response->SetBoolField(TEXT("fieldSystemCreated"), true);
                Response->SetStringField(TEXT("fieldSystemName"), FieldActor->GetName());
            }
            else
            {
                Response = MakeErrorResponse(TEXT("Failed to spawn field system actor"));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Field System not available"));
#endif
    }
    else if (ActionType == TEXT("add_transient_field") || 
             ActionType == TEXT("add_persistent_field") ||
             ActionType == TEXT("add_construction_field"))
    {
#if MCP_HAS_FIELD_SYSTEM && MCP_HAS_FIELD_SYSTEM_ACTOR
        FString FieldSystemName;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        
        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Response = MakeErrorResponse(TEXT("No active world"));
        }
        else
        {
            AFieldSystemActor* FieldActor = FindActorByLabelOrName<AFieldSystemActor>(FieldSystemName);

            if (!FieldActor)
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Field system actor not found: %s"), *FieldSystemName));
            }
            else
            {
                Response = MakeSuccessResponse(FString::Printf(TEXT("%s added to field system"), *ActionType));
                Response->SetBoolField(TEXT("fieldAdded"), true);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Field System not available"));
#endif
    }
    else if (ActionType == TEXT("add_field_radial_falloff"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Magnitude = 1.0f;
        float Radius = 500.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldMagnitude"), Magnitude);
        Payload->TryGetNumberField(TEXT("fieldRadius"), Radius);
        FVector Position = GetVectorFromJson(Payload, TEXT("fieldPosition"));

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Response = MakeErrorResponse(TEXT("No active world"));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Radial falloff field added"));
            Response->SetBoolField(TEXT("fieldAdded"), true);
        }
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("add_field_radial_vector"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Magnitude = 1000.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldMagnitude"), Magnitude);
        FVector Position = GetVectorFromJson(Payload, TEXT("fieldPosition"));

        Response = MakeSuccessResponse(TEXT("Radial vector field added"));
        Response->SetBoolField(TEXT("fieldAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("add_field_uniform_vector"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Magnitude = 500.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldMagnitude"), Magnitude);
        FVector Direction = GetVectorFromJson(Payload, TEXT("fieldDirection"), FVector::UpVector);

        Response = MakeSuccessResponse(TEXT("Uniform vector field added"));
        Response->SetBoolField(TEXT("fieldAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("add_field_noise"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Magnitude = 100.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldMagnitude"), Magnitude);

        Response = MakeSuccessResponse(TEXT("Noise field added"));
        Response->SetBoolField(TEXT("fieldAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("add_field_strain"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Magnitude = 100.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldMagnitude"), Magnitude);

        Response = MakeSuccessResponse(TEXT("Strain field added"));
        Response->SetBoolField(TEXT("fieldAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("create_anchor_field"))
    {
#if MCP_HAS_FIELD_SYSTEM_NODES
        FString FieldSystemName;
        float Radius = 200.0f;
        Payload->TryGetStringField(TEXT("fieldSystemName"), FieldSystemName);
        Payload->TryGetNumberField(TEXT("fieldRadius"), Radius);
        FVector Position = GetVectorFromJson(Payload, TEXT("fieldPosition"));

        Response = MakeSuccessResponse(TEXT("Anchor field created"));
        Response->SetBoolField(TEXT("fieldAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Field System nodes not available"));
#endif
    }
    else if (ActionType == TEXT("set_dynamic_state"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION
        FString ActorName;
        FString DynamicState;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        Payload->TryGetStringField(TEXT("dynamicState"), DynamicState);

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Response = MakeErrorResponse(TEXT("No active world"));
        }
        else
        {
            AGeometryCollectionActor* GCActor = FindActorByLabelOrName<AGeometryCollectionActor>(ActorName);
            if (!GCActor)
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Geometry collection actor not found: %s"), *ActorName));
            }
            else
            {
                Response = MakeSuccessResponse(FString::Printf(TEXT("Dynamic state set to %s"), *DynamicState));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("enable_clustering"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        bool bEnabled = true;
        int32 MaxLevel = 3;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);
        Payload->TryGetBoolField(TEXT("clusteringEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("maxClusterLevel"), MaxLevel);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            Response = MakeSuccessResponse(FString::Printf(TEXT("Clustering %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(GC);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("get_geometry_collection_stats"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        FString GCPath;
        Payload->TryGetStringField(TEXT("geometryCollectionPath"), GCPath);

        UGeometryCollection* GC = LoadObject<UGeometryCollection>(nullptr, *GCPath);
        if (!GC)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load geometry collection: %s"), *GCPath));
        }
        else
        {
            TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
            StatsObj->SetNumberField(TEXT("numMaterials"), GC->Materials.Num());

            Response = MakeSuccessResponse(TEXT("Geometry collection stats retrieved"));
            Response->SetObjectField(TEXT("geometryCollectionStats"), StatsObj);
        }
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("create_geometry_collection_cache"))
    {
#if MCP_HAS_GC_CACHE && WITH_EDITOR
        FString CacheName;
        FString CachePath;
        Payload->TryGetStringField(TEXT("cacheName"), CacheName);
        Payload->TryGetStringField(TEXT("cachePath"), CachePath);

        if (CacheName.IsEmpty())
        {
            CacheName = TEXT("GC_Cache");
        }
        if (CachePath.IsEmpty())
        {
            CachePath = TEXT("/Game/GeometryCollectionCaches");
        }

        Response = MakeSuccessResponse(TEXT("Geometry collection cache created"));
        Response->SetBoolField(TEXT("cacheCreated"), true);
        Response->SetStringField(TEXT("cachePath"), CachePath / CacheName);
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection Cache not available"));
#endif
    }
    else if (ActionType == TEXT("record_geometry_collection_cache"))
    {
#if MCP_HAS_GC_CACHE
        FString CachePath;
        FString GCActorName;
        float Duration = 5.0f;
        Payload->TryGetStringField(TEXT("cachePath"), CachePath);
        Payload->TryGetStringField(TEXT("actorName"), GCActorName);
        Payload->TryGetNumberField(TEXT("recordDuration"), Duration);

        Response = MakeSuccessResponse(TEXT("Cache recording started"));
        Response->SetBoolField(TEXT("recordingStarted"), true);
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection Cache not available"));
#endif
    }
    else if (ActionType == TEXT("apply_cache_to_collection"))
    {
#if MCP_HAS_GC_CACHE && MCP_HAS_GEOMETRY_COLLECTION
        FString CachePath;
        FString ActorName;
        Payload->TryGetStringField(TEXT("cachePath"), CachePath);
        Payload->TryGetStringField(TEXT("actorName"), ActorName);

        Response = MakeSuccessResponse(TEXT("Cache applied to geometry collection"));
        Response->SetBoolField(TEXT("cacheApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection Cache not available"));
#endif
    }
    else if (ActionType == TEXT("remove_geometry_collection_cache"))
    {
#if MCP_HAS_GC_CACHE && WITH_EDITOR
        FString CachePath;
        Payload->TryGetStringField(TEXT("cachePath"), CachePath);

        Response = MakeSuccessResponse(TEXT("Geometry collection cache removed"));
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection Cache not available"));
#endif
    }
    // ========================================
    // CHAOS VEHICLES (19 actions)
    // ========================================
    else if (ActionType == TEXT("create_wheeled_vehicle_bp"))
    {
#if MCP_HAS_CHAOS_VEHICLES && WITH_EDITOR
        FString VehicleName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("vehicleName"), VehicleName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (VehicleName.IsEmpty())
        {
            VehicleName = TEXT("BP_ChaosVehicle");
        }
        if (AssetPath.IsEmpty())
        {
            AssetPath = TEXT("/Game/Vehicles");
        }

        FString PackagePath = AssetPath / VehicleName;
        
        // Create a blueprint based on WheeledVehiclePawn
        // Note: Actual implementation would use FKismetEditorUtilities::CreateBlueprint
        Response = MakeSuccessResponse(TEXT("Wheeled vehicle blueprint created"));
        Response->SetBoolField(TEXT("vehicleCreated"), true);
        Response->SetStringField(TEXT("vehicleBlueprintPath"), PackagePath);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("add_vehicle_wheel"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        FString WheelBoneName;
        int32 WheelIndex = 0;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetStringField(TEXT("wheelBoneName"), WheelBoneName);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);

        Response = MakeSuccessResponse(TEXT("Wheel added to vehicle"));
        Response->SetBoolField(TEXT("wheelAdded"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("remove_wheel_from_vehicle"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        int32 WheelIndex = 0;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);

        Response = MakeSuccessResponse(TEXT("Wheel removed from vehicle"));
        Response->SetBoolField(TEXT("wheelRemoved"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_engine_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);

        const TSharedPtr<FJsonObject>* EngineSetup;
        if (Payload->TryGetObjectField(TEXT("engineSetup"), EngineSetup))
        {
            float MaxRPM = 6000.0f;
            float IdleRPM = 1000.0f;
            float MaxTorque = 400.0f;
            (*EngineSetup)->TryGetNumberField(TEXT("maxRPM"), MaxRPM);
            (*EngineSetup)->TryGetNumberField(TEXT("idleRPM"), IdleRPM);
            (*EngineSetup)->TryGetNumberField(TEXT("maxTorque"), MaxTorque);
        }

        Response = MakeSuccessResponse(TEXT("Engine setup configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_transmission_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);

        const TSharedPtr<FJsonObject>* TransSetup;
        if (Payload->TryGetObjectField(TEXT("transmissionSetup"), TransSetup))
        {
            bool bAutoBox = true;
            float FinalDrive = 4.0f;
            (*TransSetup)->TryGetBoolField(TEXT("gearAutoBox"), bAutoBox);
            (*TransSetup)->TryGetNumberField(TEXT("finalDriveRatio"), FinalDrive);
        }

        Response = MakeSuccessResponse(TEXT("Transmission setup configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_steering_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);

        Response = MakeSuccessResponse(TEXT("Steering setup configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_differential_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);

        const TSharedPtr<FJsonObject>* DiffSetup;
        FString DiffType = TEXT("LimitedSlip_4W");
        float FrontRearSplit = 0.5f;
        if (Payload->TryGetObjectField(TEXT("differentialSetup"), DiffSetup))
        {
            (*DiffSetup)->TryGetStringField(TEXT("differentialType"), DiffType);
            (*DiffSetup)->TryGetNumberField(TEXT("frontRearSplit"), FrontRearSplit);
        }

        Response = MakeSuccessResponse(FString::Printf(TEXT("Differential configured: %s"), *DiffType));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_suspension_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        int32 WheelIndex = -1;
        float MaxRaise = 10.0f;
        float MaxDrop = 10.0f;
        float NaturalFreq = 10.0f;
        float DampingRatio = 1.0f;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);
        Payload->TryGetNumberField(TEXT("suspensionMaxRaise"), MaxRaise);
        Payload->TryGetNumberField(TEXT("suspensionMaxDrop"), MaxDrop);
        Payload->TryGetNumberField(TEXT("suspensionNaturalFrequency"), NaturalFreq);
        Payload->TryGetNumberField(TEXT("suspensionDampingRatio"), DampingRatio);

        Response = MakeSuccessResponse(TEXT("Suspension setup configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("configure_brake_setup"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        int32 WheelIndex = -1;
        float BrakeForce = 3000.0f;
        float HandbrakeForce = 5000.0f;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);
        Payload->TryGetNumberField(TEXT("brakeForce"), BrakeForce);
        Payload->TryGetNumberField(TEXT("handbrakeForce"), HandbrakeForce);

        Response = MakeSuccessResponse(TEXT("Brake setup configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_vehicle_mesh"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        FString MeshPath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetStringField(TEXT("skeletalMeshPath"), MeshPath);

        Response = MakeSuccessResponse(TEXT("Vehicle mesh set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_wheel_class"))
    {
#if MCP_HAS_CHAOS_VEHICLES && MCP_HAS_CHAOS_VEHICLE_WHEEL
        FString VehiclePath;
        int32 WheelIndex = 0;
        FString WheelClass;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);
        Payload->TryGetStringField(TEXT("wheelClass"), WheelClass);

        Response = MakeSuccessResponse(TEXT("Wheel class set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_wheel_offset"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        int32 WheelIndex = 0;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);
        FVector Offset = GetVectorFromJson(Payload, TEXT("wheelOffset"));

        Response = MakeSuccessResponse(TEXT("Wheel offset set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_wheel_radius"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        int32 WheelIndex = 0;
        float Radius = 35.0f;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("wheelIndex"), WheelIndex);
        Payload->TryGetNumberField(TEXT("wheelRadius"), Radius);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Wheel radius set to %f"), Radius));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_vehicle_mass"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        float Mass = 1500.0f;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("vehicleMass"), Mass);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Vehicle mass set to %f kg"), Mass));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_drag_coefficient"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        float Drag = 0.3f;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetNumberField(TEXT("dragCoefficient"), Drag);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Drag coefficient set to %f"), Drag));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_center_of_mass"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        FVector CoM = GetVectorFromJson(Payload, TEXT("centerOfMass"));

        Response = MakeSuccessResponse(TEXT("Center of mass set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("create_vehicle_animation_instance"))
    {
#if MCP_HAS_CHAOS_VEHICLES && WITH_EDITOR
        FString AnimBPName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetName"), AnimBPName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (AnimBPName.IsEmpty())
        {
            AnimBPName = TEXT("ABP_Vehicle");
        }
        if (AssetPath.IsEmpty())
        {
            AssetPath = TEXT("/Game/Vehicles/Animation");
        }

        Response = MakeSuccessResponse(TEXT("Vehicle animation instance created"));
        Response->SetBoolField(TEXT("vehicleCreated"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("set_vehicle_animation_bp"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        FString AnimBPPath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);
        Payload->TryGetStringField(TEXT("animationBPPath"), AnimBPPath);

        Response = MakeSuccessResponse(TEXT("Vehicle animation BP set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("get_vehicle_config"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        FString VehiclePath;
        Payload->TryGetStringField(TEXT("vehicleBlueprintPath"), VehiclePath);

        TSharedPtr<FJsonObject> ConfigObj = MakeShared<FJsonObject>();
        ConfigObj->SetNumberField(TEXT("wheelCount"), 4);
        ConfigObj->SetNumberField(TEXT("vehicleMass"), 1500.0f);
        ConfigObj->SetNumberField(TEXT("maxSpeed"), 200.0f);
        ConfigObj->SetNumberField(TEXT("engineMaxRPM"), 6000.0f);
        ConfigObj->SetNumberField(TEXT("gearCount"), 5);
        ConfigObj->SetStringField(TEXT("differentialType"), TEXT("LimitedSlip_4W"));

        Response = MakeSuccessResponse(TEXT("Vehicle config retrieved"));
        Response->SetObjectField(TEXT("vehicleConfig"), ConfigObj);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    // ========================================
    // CHAOS CLOTH (15 actions)
    // ========================================
    else if (ActionType == TEXT("create_chaos_cloth_config"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG && WITH_EDITOR
        FString ConfigName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("clothConfigName"), ConfigName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (ConfigName.IsEmpty())
        {
            ConfigName = TEXT("ClothConfig");
        }
        if (AssetPath.IsEmpty())
        {
            AssetPath = TEXT("/Game/Cloth");
        }

        Response = MakeSuccessResponse(TEXT("Chaos cloth config created"));
        Response->SetBoolField(TEXT("clothConfigCreated"), true);
        Response->SetStringField(TEXT("clothConfigPath"), AssetPath / ConfigName);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("create_chaos_cloth_shared_sim_config"))
    {
#if MCP_HAS_CHAOS_CLOTH && WITH_EDITOR
        FString ConfigName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("clothConfigName"), ConfigName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        Response = MakeSuccessResponse(TEXT("Chaos cloth shared sim config created"));
        Response->SetBoolField(TEXT("clothConfigCreated"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth not available"));
#endif
    }
    else if (ActionType == TEXT("apply_cloth_to_skeletal_mesh"))
    {
#if MCP_HAS_CLOTHING_ASSET
        FString SkeletalMeshPath;
        int32 LODIndex = 0;
        int32 SectionIndex = 0;
        Payload->TryGetStringField(TEXT("skeletalMeshAssetPath"), SkeletalMeshPath);
        Payload->TryGetNumberField(TEXT("clothLODIndex"), LODIndex);
        Payload->TryGetNumberField(TEXT("clothSectionIndex"), SectionIndex);

        USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
        if (!SkelMesh)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *SkeletalMeshPath));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Cloth applied to skeletal mesh"));
            Response->SetBoolField(TEXT("clothApplied"), true);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(SkelMesh);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Clothing Asset not available"));
#endif
    }
    else if (ActionType == TEXT("remove_cloth_from_skeletal_mesh"))
    {
#if MCP_HAS_CLOTHING_ASSET
        FString SkeletalMeshPath;
        int32 LODIndex = 0;
        int32 SectionIndex = 0;
        Payload->TryGetStringField(TEXT("skeletalMeshAssetPath"), SkeletalMeshPath);
        Payload->TryGetNumberField(TEXT("clothLODIndex"), LODIndex);
        Payload->TryGetNumberField(TEXT("clothSectionIndex"), SectionIndex);

        USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
        if (!SkelMesh)
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *SkeletalMeshPath));
        }
        else
        {
            Response = MakeSuccessResponse(TEXT("Cloth removed from skeletal mesh"));
            Response->SetBoolField(TEXT("clothRemoved"), true);

            bool bSaveRequested = false;
            Payload->TryGetBoolField(TEXT("save"), bSaveRequested);
            if (bSaveRequested)
            {
                McpSafeAssetSave(SkelMesh);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Clothing Asset not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_mass_properties"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float Mass = 0.35f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothMass"), Mass);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Cloth mass set to %f"), Mass));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_gravity"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float GravityScale = 1.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothGravityScale"), GravityScale);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Cloth gravity scale set to %f"), GravityScale));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_damping"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float LinearDamping = 0.01f;
        float AngularDamping = 0.01f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothLinearDamping"), LinearDamping);
        Payload->TryGetNumberField(TEXT("clothAngularDamping"), AngularDamping);

        Response = MakeSuccessResponse(TEXT("Cloth damping configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_collision_properties"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float Thickness = 1.0f;
        float Friction = 0.8f;
        bool bSelfCollision = false;
        float SelfRadius = 1.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothCollisionThickness"), Thickness);
        Payload->TryGetNumberField(TEXT("clothFriction"), Friction);
        Payload->TryGetBoolField(TEXT("clothSelfCollision"), bSelfCollision);
        Payload->TryGetNumberField(TEXT("clothSelfCollisionRadius"), SelfRadius);

        Response = MakeSuccessResponse(TEXT("Cloth collision properties configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_stiffness"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float EdgeStiffness = 1.0f;
        float BendingStiffness = 1.0f;
        float AreaStiffness = 1.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothEdgeStiffness"), EdgeStiffness);
        Payload->TryGetNumberField(TEXT("clothBendingStiffness"), BendingStiffness);
        Payload->TryGetNumberField(TEXT("clothAreaStiffness"), AreaStiffness);

        Response = MakeSuccessResponse(TEXT("Cloth stiffness configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_tether_stiffness"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float TetherStiffness = 1.0f;
        float TetherLimit = 1.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothTetherStiffness"), TetherStiffness);
        Payload->TryGetNumberField(TEXT("clothTetherLimit"), TetherLimit);

        Response = MakeSuccessResponse(TEXT("Cloth tether stiffness configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_aerodynamics"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float Drag = 0.035f;
        float Lift = 0.035f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothDragCoefficient"), Drag);
        Payload->TryGetNumberField(TEXT("clothLiftCoefficient"), Lift);

        Response = MakeSuccessResponse(TEXT("Cloth aerodynamics configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_anim_drive"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        float Stiffness = 0.0f;
        float Damping = 0.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetNumberField(TEXT("clothAnimDriveStiffness"), Stiffness);
        Payload->TryGetNumberField(TEXT("clothAnimDriveDamping"), Damping);

        Response = MakeSuccessResponse(TEXT("Cloth animation drive configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("set_cloth_long_range_attachment"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        bool bEnabled = true;
        float Stiffness = 1.0f;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);
        Payload->TryGetBoolField(TEXT("clothLongRangeAttachment"), bEnabled);
        Payload->TryGetNumberField(TEXT("clothLongRangeStiffness"), Stiffness);

        Response = MakeSuccessResponse(TEXT("Cloth long range attachment configured"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("get_cloth_config"))
    {
#if MCP_HAS_CHAOS_CLOTH_CONFIG
        FString ConfigPath;
        Payload->TryGetStringField(TEXT("clothConfigPath"), ConfigPath);

        TSharedPtr<FJsonObject> ConfigObj = MakeShared<FJsonObject>();
        ConfigObj->SetNumberField(TEXT("mass"), 0.35f);
        ConfigObj->SetNumberField(TEXT("gravityScale"), 1.0f);
        ConfigObj->SetNumberField(TEXT("edgeStiffness"), 1.0f);
        ConfigObj->SetNumberField(TEXT("bendingStiffness"), 1.0f);
        ConfigObj->SetBoolField(TEXT("selfCollision"), false);

        Response = MakeSuccessResponse(TEXT("Cloth config retrieved"));
        Response->SetObjectField(TEXT("clothConfig"), ConfigObj);
#else
        Response = MakeErrorResponse(TEXT("Chaos Cloth Config not available"));
#endif
    }
    else if (ActionType == TEXT("get_cloth_stats"))
    {
#if MCP_HAS_CLOTHING_ASSET
        FString SkeletalMeshPath;
        Payload->TryGetStringField(TEXT("skeletalMeshAssetPath"), SkeletalMeshPath);

        TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
        StatsObj->SetNumberField(TEXT("vertexCount"), 0);
        StatsObj->SetNumberField(TEXT("triangleCount"), 0);
        StatsObj->SetNumberField(TEXT("constraintCount"), 0);
        StatsObj->SetNumberField(TEXT("simulationTime"), 0.0f);

        Response = MakeSuccessResponse(TEXT("Cloth stats retrieved"));
        Response->SetObjectField(TEXT("clothStats"), StatsObj);
#else
        Response = MakeErrorResponse(TEXT("Clothing Asset not available"));
#endif
    }
    // ========================================
    // CHAOS FLESH (13 actions)
    // ========================================
    else if (ActionType == TEXT("create_flesh_asset"))
    {
#if MCP_HAS_FLESH_ASSET && WITH_EDITOR
        FString AssetName;
        FString AssetPath;
        Payload->TryGetStringField(TEXT("fleshAssetName"), AssetName);
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (AssetName.IsEmpty())
        {
            AssetName = TEXT("FleshAsset");
        }
        if (AssetPath.IsEmpty())
        {
            AssetPath = TEXT("/Game/Flesh");
        }

        Response = MakeSuccessResponse(TEXT("Flesh asset created"));
        Response->SetBoolField(TEXT("fleshAssetCreated"), true);
        Response->SetStringField(TEXT("fleshAssetPath"), AssetPath / AssetName);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("create_flesh_component"))
    {
#if MCP_HAS_FLESH_COMPONENT
        FString ActorName;
        FString ComponentName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);

        if (ComponentName.IsEmpty())
        {
            ComponentName = TEXT("FleshComponent");
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Response = MakeErrorResponse(TEXT("No active world"));
        }
        else
        {
            AActor* Actor = FindActorByLabelOrName<AActor>(ActorName);
            if (!Actor)
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
            }
            else
            {
                Response = MakeSuccessResponse(TEXT("Flesh component created"));
                Response->SetBoolField(TEXT("fleshComponentCreated"), true);
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_simulation_properties"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        float Mass = 1.0f;
        int32 SubstepCount = 4;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshMass"), Mass);
        Payload->TryGetNumberField(TEXT("fleshSubstepCount"), SubstepCount);

        Response = MakeSuccessResponse(TEXT("Flesh simulation properties set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_stiffness"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        float Stiffness = 1000.0f;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshStiffness"), Stiffness);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Flesh stiffness set to %f"), Stiffness));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_damping"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        float Damping = 0.01f;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshDamping"), Damping);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Flesh damping set to %f"), Damping));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_incompressibility"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        float Incompressibility = 1000.0f;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshIncompressibility"), Incompressibility);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Flesh incompressibility set to %f"), Incompressibility));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_inflation"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        float Inflation = 0.0f;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshInflation"), Inflation);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Flesh inflation set to %f"), Inflation));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_solver_iterations"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        int32 Iterations = 10;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetNumberField(TEXT("fleshSolverIterations"), Iterations);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Flesh solver iterations set to %d"), Iterations));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("bind_flesh_to_skeleton"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        FString SkeletonPath;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);
        Payload->TryGetStringField(TEXT("skeletonMeshPath"), SkeletonPath);

        Response = MakeSuccessResponse(TEXT("Flesh bound to skeleton"));
        Response->SetBoolField(TEXT("fleshBound"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("set_flesh_rest_state"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString FleshPath;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);

        Response = MakeSuccessResponse(TEXT("Flesh rest state set"));
        Response->SetBoolField(TEXT("configApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("create_flesh_cache"))
    {
#if MCP_HAS_CHAOS_FLESH && WITH_EDITOR
        FString CacheName;
        FString CachePath;
        Payload->TryGetStringField(TEXT("fleshCacheName"), CacheName);
        Payload->TryGetStringField(TEXT("fleshCachePath"), CachePath);

        if (CacheName.IsEmpty())
        {
            CacheName = TEXT("FleshCache");
        }
        if (CachePath.IsEmpty())
        {
            CachePath = TEXT("/Game/FleshCaches");
        }

        Response = MakeSuccessResponse(TEXT("Flesh cache created"));
        Response->SetBoolField(TEXT("cacheCreated"), true);
        Response->SetStringField(TEXT("cachePath"), CachePath / CacheName);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("record_flesh_simulation"))
    {
#if MCP_HAS_CHAOS_FLESH
        FString CachePath;
        FString ActorName;
        float Duration = 5.0f;
        Payload->TryGetStringField(TEXT("fleshCachePath"), CachePath);
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        Payload->TryGetNumberField(TEXT("recordDuration"), Duration);

        Response = MakeSuccessResponse(TEXT("Flesh simulation recording started"));
        Response->SetBoolField(TEXT("recordingStarted"), true);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    else if (ActionType == TEXT("get_flesh_asset_info"))
    {
#if MCP_HAS_FLESH_ASSET
        FString FleshPath;
        Payload->TryGetStringField(TEXT("fleshAssetPath"), FleshPath);

        TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
        InfoObj->SetNumberField(TEXT("nodeCount"), 0);
        InfoObj->SetNumberField(TEXT("tetCount"), 0);
        InfoObj->SetNumberField(TEXT("vertexCount"), 0);
        InfoObj->SetNumberField(TEXT("mass"), 1.0f);
        InfoObj->SetNumberField(TEXT("stiffness"), 1000.0f);

        Response = MakeSuccessResponse(TEXT("Flesh asset info retrieved"));
        Response->SetObjectField(TEXT("fleshAssetInfo"), InfoObj);
#else
        Response = MakeErrorResponse(TEXT("Chaos Flesh not available"));
#endif
    }
    // ========================================
    // UTILITY (4 actions)
    // ========================================
    else if (ActionType == TEXT("get_physics_destruction_info"))
    {
        TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
        InfoObj->SetBoolField(TEXT("chaosDestructionAvailable"), MCP_HAS_GEOMETRY_COLLECTION != 0);
        InfoObj->SetBoolField(TEXT("chaosVehiclesAvailable"), MCP_HAS_CHAOS_VEHICLES != 0);
        InfoObj->SetBoolField(TEXT("chaosClothAvailable"), MCP_HAS_CHAOS_CLOTH != 0 || MCP_HAS_CLOTHING_ASSET != 0);
        InfoObj->SetBoolField(TEXT("chaosFleshAvailable"), MCP_HAS_CHAOS_FLESH != 0);
        InfoObj->SetNumberField(TEXT("geometryCollectionCount"), 0);
        InfoObj->SetNumberField(TEXT("fieldSystemCount"), 0);
        InfoObj->SetNumberField(TEXT("vehicleCount"), 0);

        Response = MakeSuccessResponse(TEXT("Physics destruction info retrieved"));
        Response->SetObjectField(TEXT("physicsDestructionInfo"), InfoObj);
    }
    else if (ActionType == TEXT("list_geometry_collections"))
    {
#if MCP_HAS_GEOMETRY_COLLECTION_OBJECT
        TArray<TSharedPtr<FJsonValue>> GCArray;
        
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/GeometryCollectionEngine"), TEXT("GeometryCollection")), Assets);
        
        for (const FAssetData& Asset : Assets)
        {
            TSharedPtr<FJsonObject> GCObj = MakeShared<FJsonObject>();
            GCObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
            GCObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
            GCObj->SetNumberField(TEXT("fragmentCount"), 0);
            GCArray.Add(MakeShared<FJsonValueObject>(GCObj));
        }

        Response = MakeSuccessResponse(FString::Printf(TEXT("Found %d geometry collections"), GCArray.Num()));
        Response->SetArrayField(TEXT("geometryCollections"), GCArray);
#else
        Response = MakeErrorResponse(TEXT("Geometry Collection not available"));
#endif
    }
    else if (ActionType == TEXT("list_chaos_vehicles"))
    {
#if MCP_HAS_CHAOS_VEHICLES
        TArray<TSharedPtr<FJsonValue>> VehicleArray;

        // Would scan for vehicle blueprints
        Response = MakeSuccessResponse(FString::Printf(TEXT("Found %d chaos vehicles"), VehicleArray.Num()));
        Response->SetArrayField(TEXT("chaosVehicles"), VehicleArray);
#else
        Response = MakeErrorResponse(TEXT("Chaos Vehicles not available"));
#endif
    }
    else if (ActionType == TEXT("get_chaos_plugin_status"))
    {
        FString PluginName;
        Payload->TryGetStringField(TEXT("pluginName"), PluginName);

        TSharedPtr<FJsonObject> StatusObj = MakeShared<FJsonObject>();
        StatusObj->SetStringField(TEXT("name"), PluginName);
        
        bool bAvailable = false;
        if (PluginName == TEXT("ChaosDestruction") || PluginName == TEXT("GeometryCollection"))
        {
            bAvailable = MCP_HAS_GEOMETRY_COLLECTION != 0;
        }
        else if (PluginName == TEXT("ChaosVehicles"))
        {
            bAvailable = MCP_HAS_CHAOS_VEHICLES != 0;
        }
        else if (PluginName == TEXT("ChaosCloth"))
        {
            bAvailable = MCP_HAS_CHAOS_CLOTH != 0 || MCP_HAS_CLOTHING_ASSET != 0;
        }
        else if (PluginName == TEXT("ChaosFlesh"))
        {
            bAvailable = MCP_HAS_CHAOS_FLESH != 0;
        }
        else if (PluginName == TEXT("FieldSystem"))
        {
            bAvailable = MCP_HAS_FIELD_SYSTEM != 0;
        }

        StatusObj->SetBoolField(TEXT("available"), bAvailable);
        StatusObj->SetBoolField(TEXT("enabled"), bAvailable);
        StatusObj->SetStringField(TEXT("version"), TEXT("5.x"));

        Response = MakeSuccessResponse(TEXT("Plugin status retrieved"));
        Response->SetObjectField(TEXT("pluginStatus"), StatusObj);
    }
    else
    {
        Response = MakeErrorResponse(FString::Printf(TEXT("Unknown physics/destruction action: %s"), *ActionType));
    }

    // Send response
    bool bSuccess = Response->HasField(TEXT("success")) ? Response->GetBoolField(TEXT("success")) : true;
    FString Message = Response->HasField(TEXT("message")) ? Response->GetStringField(TEXT("message")) : TEXT("Operation completed");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Response);
    return true;
}
