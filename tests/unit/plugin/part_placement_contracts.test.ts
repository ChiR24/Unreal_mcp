/**
 * Source contracts for parts that sink inside one actor, and for SDF shape copies. A rider's legs sat 12 cm inside his
 * mount while every Blueprint edit answered a plain success: both parts belong to one actor, so the actor placement
 * check never compared them. And the only way to give an SDF both eyes or a row of stitches was to list every copy,
 * which pushed authoring out of the editor. The C++ cannot run here, so these pin the rules.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const bridge = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (file: string): string =>
  readFileSync(resolve(bridge, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('Blueprint parts that sink are named', () => {
  const audit = code('Domains/ControlActor/Placement/McpAutomationBridge_PartPlacement.cpp');

  it('measures a preview instance on the source mesh, not bounds or the Nanite fallback', () => {
    expect(audit).toContain('FPreviewScene Scene;');
    expect(audit).toContain('GetMeshDescription(0)');
    expect(audit).toContain('TFastWindingTree<FDynamicMesh3>');
    expect(audit).toContain('FindNearestPoint(Local)');
    expect(audit).toContain('GetScaledCapsuleHalfHeight()');
  });

  it('leaves out a part tagged as a deliberate embed', () => {
    expect(audit).toContain('const FName AcceptedTag(TEXT("mcp.placement.ok"));');
    expect(audit).toContain('ComponentHasTag(AcceptedTag)');
  });

  it('reports from every SCS edit path, for the components it touched', () => {
    const finalize = code('Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersModifyScsFinalize.cpp');
    expect(finalize).toContain('McpPartPlacement::AuditBlueprintParts(LocalBP, Touched)');
    expect(finalize).toContain('ResultPayload->SetArrayField(TEXT("partWarnings"), PartWarnings);');
    const add = code('Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsAddComponent.cpp');
    expect(add).toContain('McpPartPlacement::AppendPartWarnings(BlueprintPath, {ComponentName}, Result);');
    const wrappers = code('Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsWrappers.cpp');
    expect(wrappers.match(/McpPartPlacement::AppendPartWarnings\(BPPath, \{CompName\}, Result\);/gu)).toHaveLength(3);
  });

  it('names what a mesh replaced in place did to the loaded Blueprints that draw it, without loading any', () => {
    const convert = code('Domains/Geometry/Support/McpAutomationBridge_GeometryAssetConversion.cpp');
    expect(convert).toContain('McpPartPlacement::AppendMeshUserWarnings(CreatedMesh, Result);');
    const reply = code('Domains/ControlActor/Placement/McpAutomationBridge_PartPlacementReply.cpp');
    expect(reply).toContain('GetReferencers(');
    expect(reply).toContain('FindObject<UBlueprint>(');
    expect(reply).not.toContain('LoadObject<UBlueprint>(');
    expect(audit).toContain('Component->GetStaticMesh() == Mesh');
  });

  it('answers audit_placement with blueprintPath before sweeping the level', () => {
    const handler = code('Domains/ControlActor/McpAutomationBridge_ControlActorPlacementAudit.cpp');
    const branch = handler.indexOf('McpPartPlacement::BuildBlueprintAuditReply(');
    expect(branch).toBeGreaterThan(-1);
    expect(branch).toBeLessThan(handler.indexOf('for (TActorIterator<AActor> It(World); It; ++It)'));
  });
});

describe('an SDF shape can stand for its copies', () => {
  const read = code('Domains/Geometry/Primitives/McpAutomationBridge_GeometryPrimitivesImplicit.cpp');
  const copies = code('Domains/Geometry/Primitives/McpAutomationBridge_GeometrySdfCopies.h');
  const field = code('Domains/Geometry/Primitives/McpAutomationBridge_GeometrySdfField.h');

  it('expands repeat, then mirror, keeping the authoring index on every copy', () => {
    expect(read).toContain('McpGeometrySdf::ExpandRepeat(*Obj, S, Copies, CopyError) || !McpGeometrySdf::ExpandMirror(*Obj, Copies, CopyError)');
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
