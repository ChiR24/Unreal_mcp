/**
 * Source contracts for parts that sink inside one actor, and for SDF shape copies. A seated character's legs sat 12 cm
 * inside the body under them while every Blueprint edit answered a plain success: both parts belong to one actor, so the actor placement
 * check never compared them. And the only way to give an SDF both eyes or a row of stitches was to list every copy,
 * which pushed authoring out of the editor. The C++ cannot run here, so these pin the rules.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const bridge = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private');
/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (file: string): string =>
  readFileSync(resolve(bridge, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('Blueprint parts that sink are named', () => {
  const audit = code('Domains/ControlActor/Placement/McpPartPlacement.cpp');

  it('measures a preview instance on the source mesh, not bounds or the Nanite fallback', () => {
    expect(audit).toContain('FPreviewScene Scene;');
    expect(audit).toContain('GetMeshDescription(0)');
    expect(audit).toContain('TFastWindingTree<FDynamicMesh3>');
    expect(audit).toContain('FindNearestPoint(Local)');
    expect(audit).toContain('GetScaledCapsuleHalfHeight()');
  });

  // A nozzle 3 cm into a cylinder scaled into a 19 m pipe read 253.5 cm deep: the nearest surface in the unscaled
  // space was the end cap.
  it('measures depth in world distance on a part with a non-uniform or mirrored scale', () => {
    expect(audit).toContain('Shape->Mesh.SetVertex(Vid, Shape->Mesh.GetVertex(Vid) * FVector3d(Scale));');
    expect(audit).toContain('if (Scale.X * Scale.Y * Scale.Z < 0.0) Shape->Mesh.ReverseOrientation();');
    expect(audit).toContain('Shapes.FindOrAdd(Mesh->GetPathName() + Scale.ToString())');
    expect(audit).toContain('Part.Transform.SetScale3D(FVector::OneVector);');
  });

  it('leaves out a part tagged as a deliberate embed', () => {
    expect(audit).toContain('const FName AcceptedTag(TEXT("mcp.placement.ok"));');
    expect(audit).toContain('ComponentHasTag(AcceptedTag)');
  });

  // A neck in its collar had to opt the whole head out, so a head sunk into anything else went unreported.
  it('accepts a pair embed for that pair only', () => {
    expect(audit).toContain('const FString AcceptedPairPrefix(TEXT("mcp.placement.ok:"));');
    expect(audit).toContain('Part.Accepted.Add(Tag.ToString().Mid(AcceptedPairPrefix.Len()));');
    expect(audit).toContain('A.Accepted.Contains(B.Name) || B.Accepted.Contains(A.Name)');
  });

  it('reports from every SCS edit path, for the components it touched', () => {
    const finalize = code('Domains/Blueprint/Components/McpBPModifyScsFinalize.cpp');
    expect(finalize).toContain('McpPartPlacement::AuditBlueprintParts(LocalBP, Touched)');
    expect(finalize).toContain('ResultPayload->SetArrayField(TEXT("partWarnings"), PartWarnings);');
    const add = code('Domains/Blueprint/Components/McpBPScsAddComponent.cpp');
    expect(add).toContain('McpPartPlacement::AppendPartWarnings(BlueprintPath, {ComponentName}, Result);');
    const wrappers = code('Domains/Blueprint/Components/McpBPScsWrappers.cpp');
    expect(wrappers.match(/McpPartPlacement::AppendPartWarnings\(BPPath, \{CompName\}, Result\);/gu)).toHaveLength(3);
  });

  it('names what a mesh replaced in place did to the loaded Blueprints that draw it, without loading any', () => {
    const convert = code('Domains/Geometry/Support/McpGeometryAssetConversion.cpp');
    expect(convert).toContain('McpPartPlacement::AppendMeshUserWarnings(CreatedMesh, Result);');
    const reply = code('Domains/ControlActor/Placement/McpPartPlacementReply.cpp');
    expect(reply).toContain('GetReferencers(');
    expect(reply).toContain('FindObject<UBlueprint>(');
    expect(reply).not.toContain('LoadObject<UBlueprint>(');
    expect(audit).toContain('Component->GetStaticMesh() == Mesh');
  });

  // A trigger box of extent 70x70x40 put under a base flattened to (1.6, 1.6, 0.1) was 112x112x4 in the world, and
  // neither the edit nor get_components said so.
  it('says when a part it touched takes an uneven scale from its parent, and get_components shows the world scale', () => {
    const reply = code('Domains/ControlActor/Placement/McpPartPlacementReply.cpp');
    expect(audit).toContain('NoteInheritedScale(Actor, Focus, Audit.Notes);');
    expect(reply).toContain('const FVector Inherited = Parent->GetComponentScale();');
    expect(reply).toContain('Inherited.GetAbsMax() <= Inherited.GetAbsMin() * 1.05');
    expect(reply).toContain('Component->IsUsingAbsoluteScale()');
    expect(reply).toContain('Warnings.Add(MakeShared<FJsonValueString>(Note));');
    expect(code('Domains/Blueprint/Components/McpBPModifyScsFinalize.cpp')).toContain('State.LocalWarnings.Append(Parts.Notes);');
    const components = code('Domains/ControlActor/McpActorComponents.cpp');
    expect(components).toContain('!Found->HasAnyFlags(RF_ClassDefaultObject) && !SceneComp->GetComponentScale().Equals(Scale, 1e-3)');
    expect(components).toContain('TEXT("worldScale")');
  });

  it('answers audit_placement with blueprintPath before sweeping the level', () => {
    const handler = code('Domains/ControlActor/McpActorPlacementAudit.cpp');
    const branch = handler.indexOf('McpPartPlacement::BuildBlueprintAuditReply(');
    expect(branch).toBeGreaterThan(-1);
    expect(branch).toBeLessThan(handler.indexOf('for (TActorIterator<AActor> It(World); It; ++It)'));
  });
});

describe('an SDF shape can stand for its copies', () => {
  const read = code('Domains/Geometry/Primitives/McpGeometryPrimitivesImplicit.cpp');
  const copies = code('Domains/Geometry/Primitives/McpGeometrySdfCopies.h');
  const field = code('Domains/Geometry/Primitives/McpGeometrySdfField.h');

  it('expands repeat, then mirror, keeping the authoring index on every copy', () => {
    expect(read).toContain('McpGeometrySdf::ExpandCopies(*Obj, S, Index, Authored, Out, CopyError)');
    expect(copies).toContain('!ExpandRepeat(Obj, S, Copies, Error) || !ExpandMirror(Obj, Copies, Error)');
    expect(read).toContain('S.Source = Index;');
    expect(read).toContain('Mesh.SetTriangleGroup(Tid, Owner.Source + 1);');
  });

  it('reflects into a proper rotation by also flipping the symmetric local X', () => {
    expect(copies).toContain('FScaleMatrix(FVector(-1.0, 1.0, 1.0)) * FQuatRotationMatrix(S.Frame.GetRotation())');
  });

  it('skips only shapes whose blend cannot reach the point, never an intersect', () => {
    expect(field).toContain('if (S.Op != EOp::Intersect)');
    expect(field).toContain('if (S.Op == EOp::Union ? Far >= D + K : Far >= K - D) continue;');
    expect(field).toContain('default: return S.Length * 0.5 + FMath::Max(S.Radius, S.TopRadius);');
  });
});
