#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"


#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshActor.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UDynamicMesh.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MeshBoundaryLoops.h"
#include "EdgeLoop.h"
#endif

#if __has_include("GeometryScript/GeometryScriptTypes.h")
#include "GeometryScript/GeometryScriptTypes.h"
#else
#include "GeometryScriptTypes.h"
#endif

#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#include "GeometryScript/CollisionFunctions.h"
#endif
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "GeometryScript/MeshRemeshFunctions.h"
#endif
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "GeometryScript/MeshTransformFunctions.h"
#endif
#include "GeometryScript/MeshUVFunctions.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 1
#else
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 0
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogMcpGeometryHandlers, Log, All);

struct FGeometryScriptMeshSelection; // global type (GeometryScript/GeometryScriptSelectionTypes.h), declared outside the namespace

namespace McpGeometryHandlers
{
inline constexpr int32 MAX_SEGMENTS = 256;
inline constexpr double MAX_DIMENSION = 100000.0;
inline constexpr double MIN_DIMENSION = 0.01;
inline constexpr int32 MAX_TRIANGLES_PER_DYNAMIC_MESH = 500000;
inline constexpr int32 MAX_SUBDIVIDE_ITERATIONS = 6;
inline constexpr float MEMORY_PRESSURE_CRITICAL = 0.90f;

FTransform ReadTransformFromPayload(const TSharedPtr<FJsonObject>& Payload);
AActor* SpawnDynamicMeshActorWithMesh(const FTransform& Transform, const FString& Name, UDynamicMesh* DynMesh, FString& OutError);
// Spawn DynMesh as a DynamicMeshActor; on failure frees DynMesh, sends SPAWN_FAILED
// and returns nullptr. On success OutResult carries {name, class}.
AActor* SpawnPrimitiveOrReply(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const FTransform& Transform, const FString& Name, UDynamicMesh* DynMesh, TSharedPtr<FJsonObject>& OutResult);
// Replies with the error and returns false when memory is short or the op would exceed the triangle cap.
bool GuardMeshBudget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, int64 EstimatedTriangles, const TCHAR* OpName);
// Moves every vertex to Move(position) in one mesh edit; returns how many moved.
int32 DeformVertices(UDynamicMesh* Mesh, TFunctionRef<FVector(const FVector&)> Move);
void RecomputeMeshNormals(UDynamicMesh* Mesh, const FGeometryScriptCalculateNormalsOptions& Options = FGeometryScriptCalculateNormalsOptions());
// Optional triangleIndices -> mesh selection (dogfood #137); false after replying INVALID_SELECTION on bad ids.
bool ReadTriangleSelection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload, FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection);
// Face operators take distance, or amount as the documented spelling.
double FaceOpDistance(const TSharedPtr<FJsonObject>& Payload, double Default);
int32 ClampSegments(int32 Value, int32 Default = 1);
// Where a baked static mesh goes: outputPath (a folder gets DefaultName appended),
// else /Game/GeneratedMeshes/DefaultName. False, with OutError, for an unsafe path.
bool ResolveConversionAssetPath(const TSharedPtr<FJsonObject>& Payload, const FString& DefaultName,
                                FString& OutAssetPath, FString& OutError);
// create_primitive declares numSides/radialSegments/numRings/heightSegments;
// the shape handlers read older undeclared names (segments, subdivisions,
// radialSteps...) that the gateway rejects, so every rounded primitive came
// out at its default tessellation. First name present wins: declared first.
int32 DeclaredSegments(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> Names, int32 Default);
double ClampDimension(double Value, double Default = 100.0);
// Editor-world actor of class TActor by label or object name.
template <class TActor>
TActor* FindGeometryActor(const FString& ActorName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<TActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName) return *It;
	}
	return nullptr;
}
// The named actor's spline; null after replying with the error when it has none.
USplineComponent* ResolveGeometrySpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& SplineActorName);
struct FMcpGeometryTarget
{
	ADynamicMeshActor* Actor = nullptr;
	UDynamicMeshComponent* Component = nullptr;
	UDynamicMesh* Mesh = nullptr;
};
// The named actor's dynamic mesh; unset after replying with the error when it cannot be resolved.
TOptional<FMcpGeometryTarget> ResolveGeometryTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& ActorName, TSharedPtr<FMcpBridgeWebSocket> Socket);
UTexture2D* ResolveGeometryTexture(const FString& TexturePath, FString& OutResolvedPath);
bool SampleTextureLuminance(UTexture2D* Texture, double U, double V, double& OutLuminance);
// The payload `axis` (X/Y/Z) as a unit vector or 0/1/2 index; Default when absent or unknown.
FVector AxisVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FVector& Default = FVector::UpVector);
int32 AxisIndexFromPayload(const TSharedPtr<FJsonObject>& Payload, int32 Default = 2);
// Scales a mesh about the origin (per-vertex fallback before UE 5.4).
void ScaleMesh(UDynamicMesh* Mesh, const FVector& Scale);
bool HandleBooleanOperation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, EGeometryScriptBooleanOperation BoolOp, const FString& OpName);
bool HandleInsetOutset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bInset);
bool HandleCreateBox(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSphere(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateCylinder(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateCone(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateCapsule(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateTorus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bArch = false);
bool HandleCreatePlane(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateDisc(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bRing = false);
bool HandleBooleanUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBooleanSubtract(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBooleanIntersection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetMeshInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRecalculateNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleFlipNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSimplifyMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSubdivide(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bNanite = false);
bool HandleCreateStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSpiralStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleExtrude(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBevel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleOffsetFaces(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleShell(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBend(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleTwist(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleTaper(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleNoiseDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSmooth(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleWeldVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bMerge = false);
bool HandleFillHoles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveDegenerates(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemeshUniform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bVoxel = false);
bool HandleGenerateCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bComplex = false);
bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleArrayRadial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreatePipe(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateRamp(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRelax(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleProjectUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRecomputeTangents(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRevolve(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleStretch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSpherify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCylindrify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleLatticeDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleDisplaceByTexture(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleTransformUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBooleanTrim(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSelfUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleBridge(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSegmentedSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleDuplicateAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleLoopCut(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSplitNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateProceduralMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAppendTriangle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetVertexColor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAppendVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleTranslateMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandlePackUVIslands(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleEdgeSplit(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSimplifyCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGenerateLODsGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetLODSettings(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetLODScreenSizes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpGeometryHandlers

