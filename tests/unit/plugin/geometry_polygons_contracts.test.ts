// Wiring contracts of edit_dynamic_mesh append_polygons that no unit test can reach by running it (it needs an editor):
// everything is validated before the mesh is touched, and the cage is appended under one edit. Behaviour itself belongs
// to the integration cases in tests/mcp-tools/world/manage-geometry-polygons.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'EngineWire', 'Source', 'EngineWire');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('append_polygons validates everything before it touches the mesh', () => {
  const input = (): string => read('Modeling', 'McpGeometryPolygonInput.cpp');
  const handler = (): string => read('Modeling', 'McpGeometryAppendPolygons.cpp');

  it('caps the call, checks indices and finite coordinates, and names the face at fault', () => {
    const source = input();
    expect(source).toContain('static constexpr int32 MaxAppendVertices = 20000;');
    expect(source).toContain('static constexpr int32 MaxAppendFaces = 20000;');
    expect(source).toContain('!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)');
    expect(source).toContain('faces[%d] uses vertex index %.0f, but vertices has %d points');
    expect(source).toContain('faces[%d] uses vertex %.0f twice');
    expect(source).toContain('Corners->Num() < 3 || Corners->Num() > MaxAppendFaceCorners');
  });

  it('ear clips faces of more than three corners in the winding they were given', () => {
    // The block comment naming bOrientAsHoleFill is stripped before this reads, leaving only `false`: same winding, not a hole fill.
    expect(input()).toMatch(/PolygonTriangulation::TriangulateSimplePolygon\(Polygon, Local,\s+false\);/u);
    expect(input()).toContain('Local.Num() != Face.Num() - 2');
  });

  // Unreal's triangles face (v2 - v0) x (v1 - v0); appended in the given order, an outward-wound cage came out inside out.
  it('appends each triangle in Unreal winding, so (v1 - v0) x (v2 - v0) is the side it faces', () => {
    expect(handler()).toContain('EditMesh.AppendTriangle(VertexIds[Triangle.A], VertexIds[Triangle.C], VertexIds[Triangle.B], GroupOfFace[Face]);');
    expect(read('Modeling', 'McpGeometryProceduralMesh.cpp')).toContain('EditMesh.AppendTriangle(Idx0, Idx2, Idx1, GroupID);');
  });

  // Appended onto a mesh that had normals, the new faces had none and rendered as streaks until a split_normals.
  it('gives every triangle the normal overlay left unset an element per corner before the recompute', () => {
    const source = handler();
    expect(source).toContain('Normals->IsSetTriangle(TriangleId)');
    expect(source).toContain('Normals->SetTriangle(TriangleId, Elements);');
    expect(source.indexOf('Normals->SetTriangle(TriangleId, Elements);')).toBeLessThan(source.indexOf('RecomputeMeshNormals(Mesh);'));
  });

  it('refuses inconsistent winding and edges shared by more than two faces', () => {
    const source = input();
    expect(source).toContain('both run from vertex %u to vertex %u');
    expect(source).toContain('if (++Count > 2)');
  });

  it('appends under one edit, one unique polygroup per face unless faceGroups is given, and restores on a refusal', () => {
    const source = handler();
    expect(source).toContain('Input.FaceGroups.Num() > 0 ? Input.FaceGroups[FaceIndex] : EditMesh.AllocateTriangleGroup()');
    expect(source).toContain('MaterialIds->SetValue(TriangleId, Input.FaceMaterials[Face]);');
    expect(source).toContain('Mesh->SetMesh(MoveTemp(Backup));');
    expect(source).toContain('GuardMeshBudget(Self, RequestId, Socket,');
    for (const field of ['verticesAdded', 'facesAdded', 'trianglesAdded', 'vertexCount', 'triangleCount', 'groupCount']) {
      expect(source, field).toContain(`TEXT("${field}")`);
    }
  });

  it('keeps polygroup ids small enough for FGroupTopology to size its arrays by them', () => {
    expect(input()).toContain('static constexpr int32 MaxAppendGroupId = 1000000;');
    expect(input()).toContain('ParseAppendFaceIntegers(Payload, TEXT("faceGroups"), OutInput.Faces.Num(), MaxAppendGroupId');
  });

  it('leaves out a point no face uses, so no isolated vertex stretches the mesh bounds', () => {
    const source = read('Modeling', 'McpGeometryAppendPolygons.cpp');
    expect(source).toContain('Used[Triangle.A] = Used[Triangle.B] = Used[Triangle.C] = true;');
    expect(source).toContain('if (Used[VertexIndex])');
    expect(source).toContain('Result->SetNumberField(TEXT("verticesAdded"), VerticesAdded);');
  });

  it('the dispatcher routes append_polygons, append_cuboids and append_loft to HandleAppendPolygons', () => {
    expect(read('McpGeometryHandlers.cpp')).toMatch(
      /SubAction == TEXT\("append_polygons"\) \|\| SubAction == TEXT\("append_cuboids"\) \|\| SubAction == TEXT\("append_loft"\)\)\s*return HandleAppendPolygons\(this, RequestId, Payload, RequestingSocket, SubAction\);/u);
  });
});

// A cage of boxes or a loft took hundreds of hand-wound faces in one payload; both now build the input append_polygons
// validates, wound outward for the caller.
describe('append_cuboids and append_loft build a validated cage', () => {
  const source = read('Modeling', 'McpGeometryAppendShapes.cpp');

  it('lay every box face outward and repeat on three axes, then validate like append_polygons', () => {
    expect(source).toContain('{TEXT("-z"), {0, 2, 3, 1}}, {TEXT("+z"), {4, 5, 7, 6}}, {TEXT("-y"), {0, 1, 5, 4}}');
    expect(source).toContain('const FVector3d Shift = Grid.At(Copy);');
    expect(source.match(/return FinishAppendPolygonsInput\(OutInput, OutError\);/gu)).toHaveLength(2);
  });

  it('count copies along count, then rows, then layers', () => {
    expect(read('McpGeometryHandlers.h')).toContain(
      'return Offset * (Copy % Count) + RowOffset * ((Copy / Count) % Rows) + LayerOffset * (Copy / (Count * Rows));');
  });

  it('turn a loft outward from its first section against the way the sections run', () => {
    expect(source).toContain('const bool bAlong = Normal.Dot(Last - First) > 0.0;');
    expect(source).toContain('TEXT("startPoint and endPoint close a tube\'s ends, so they need closed true")');
  });
});

// A model assembled from detailed parts took one actor per part; append_mesh copies an asset in, and delete_triangles
// takes a picked region back out.
describe('append_mesh copies a static mesh in and delete_triangles removes a selection', () => {
  const copy = read('Modeling', 'McpGeometryAppendMesh.cpp');
  const ids = read('Modeling', 'McpGeometryMatIds.cpp');

  it('reads the asset once, caps the copies before the mesh is touched, and maps each slot to its ids in turn', () => {
    expect(copy).toContain('UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(Source, Part,');
    expect(copy.indexOf('if (Copies > MaxMeshCopies)')).toBeLessThan(copy.indexOf('Mesh->EditMesh('));
    expect(copy).toContain('Placement.SlotIds[Slot][Copy % Placement.SlotIds[Slot].Num()]');
    expect(copy).toContain('if (bMirrored) EditMesh.ReverseTriOrientation(Added);');
  });

  it('never deletes the whole mesh: no selection is a refusal', () => {
    expect(ids).toMatch(/if \(!bHasSelection\)\s*\{\s*Self->SendAutomationError\([\s\S]*?TEXT\("INVALID_SELECTION"\)\);/u);
    expect(ids).toContain('EditMesh.RemoveTriangle(TriangleId, true, false) == UE::Geometry::EMeshResult::Ok');
  });

  it('the dispatcher routes both edits', () => {
    const dispatcher = read('McpGeometryHandlers.cpp');
    expect(dispatcher).toContain('if (SubAction == TEXT("append_mesh")) return HandleAppendMesh(this, RequestId, Payload, RequestingSocket);');
    expect(dispatcher).toContain('if (SubAction == TEXT("delete_triangles")) return HandleDeleteTriangles(this, RequestId, Payload, RequestingSocket);');
  });
});

// An existing static mesh asset could not be edited: a dynamic mesh only ever started empty.
describe('create starts a dynamic mesh from a static mesh asset when meshPath names one', () => {
  it('copies the asset before any actor is spawned, and refuses a path that is no static mesh', () => {
    const source = read('Modeling', 'McpGeometryProceduralMesh.cpp');
    expect(source).toContain('UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(Source, DynMesh,');
    expect(source.indexOf('CopyMeshFromStaticMesh(')).toBeLessThan(source.indexOf('SpawnDynamicMeshActorWithMesh('));
    expect(source).toContain('TEXT("ASSET_NOT_FOUND")');
    expect(source).toContain('Result->SetNumberField(TEXT("triangleCount"), CopiedTriangles);');
  });
});

// A hip roof over a footprint: its faces go through append_polygons' own checks and append, each face its own polygroup
// with UVs laid from its eave, and the actor spawned as one undo step.
describe('create_primitive roof shares the polygon append and is one undo step', () => {
  const roof = read('Roof', 'McpGeometryRoof.cpp');

  it('builds its faces through FinishAppendPolygonsInput and AppendPolygonFaces, the path append_polygons runs', () => {
    expect(roof).toContain('bool bBuilt = FinishAppendPolygonsInput(Input, Error);');
    expect(roof).toContain('bBuilt = bBuilt && AppendPolygonFaces(EditMesh, Input, VerticesAdded, Error);');
    expect(read('Modeling', 'McpGeometryAppendPolygons.cpp')).toContain('bAppended = AppendPolygonFaces(EditMesh, Input, VerticesAdded, Error);');
  });

  // AppendPolygonFaces shares one normal element per vertex, as a cage wants: on a roof that shaded every ridge and eave round.
  it('gives each face its own polygroup, normal and UV elements, and spawns inside one transaction', () => {
    expect(roof).toContain('Input.FaceGroups.Add(Face + 1);');
    expect(roof).toContain('RoofFaceElements(EditMesh, Roof.Outline, Pitch);');
    expect(roof).toContain('const int64 Key = (static_cast<int64>(Face) << 32) | Corners[K];');
    expect(roof).toContain('Normals->SetTriangle(Triangle, Normal);');
    expect(roof.indexOf('RoofFaceElements(EditMesh')).toBeLessThan(roof.indexOf('RecomputeMeshNormals(DynMesh);'));
    expect(roof.indexOf('FMcpScopedEditorTransaction Transaction(')).toBeLessThan(roof.indexOf('SpawnPrimitiveOrReply('));
    expect(roof).toContain('Transaction.DescribeInto(Result);');
  });

  it('refuses an outline the skeleton cannot carry before any mesh is made', () => {
    expect(roof.indexOf('No roof: %s. Nothing was made.')).toBeLessThan(roof.indexOf('NewObject<UDynamicMesh>'));
  });

  it('the dispatcher routes create_roof', () => {
    expect(read('McpGeometryHandlers.cpp')).toContain('if (SubAction == TEXT("create_roof")) return HandleCreateRoof(this, RequestId, Payload, RequestingSocket);');
  });
});
