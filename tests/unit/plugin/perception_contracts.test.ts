// inspect analyze_view judges the level viewport's picture: saliency, palette, light and shader cost
// (Domains/Perception). Wiring contracts only: the k-means known answers are the in-editor KMeans test, the verdicts the
// integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

// inspect audit_materials: the materials in view checked as a technical artist would (Domains/Perception, Pbr and Texel).
describe('audit_materials', () => {
  const pbr = code('Domains', 'Perception', 'McpPerceptionPbr.cpp');
  const texel = code('Domains', 'Perception', 'McpPerceptionTexel.cpp');

  it('is routed by inspect and reads the frame analyze_view reads', () => {
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toContain(
      'return McpPerception::HandleAuditMaterials(*this, RequestId, Payload, RequestingSocket);',
    );
    expect(code('Domains', 'Perception', 'McpPerceptionAudit.cpp')).toContain(
      'McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("audit_materials"), true, View, Error, Code)',
    );
  });

  // A check that left the viewport in a buffer view would change what the user sees.
  it('puts the view mode and the buffer back after reading the three buffers', () => {
    expect(pbr).toMatch(/const EViewModeIndex Was = Client\.GetViewMode\(\);\s*const FName WasBuffer = Client\.CurrentBufferVisualizationMode;/u);
    expect(pbr).toMatch(/Client\.CurrentBufferVisualizationMode = WasBuffer;\s*Client\.SetViewMode\(Was\);\s*if \(bSwitch\)\s*\{\s*Client\.SetGameView\(false\);/u);
  });

  // An unlit material writes black base colour and a translucent one none: every sign and window read as too dark.
  it('skips actors whose materials the G-buffer does not show', () => {
    expect(pbr).toContain('Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit)');
    expect(pbr).toContain('Material->GetBlendMode() != BLEND_Opaque && Material->GetBlendMode() != BLEND_Masked');
  });

  // From 5.7 the five-argument GetUsedTextures is deprecated with an empty body: it compiles and finds nothing.
  it('asks for a material\'s textures through the overload each engine version fills', () => {
    expect(texel).toMatch(/#if ENGINE_MAJOR_VERSION > 5 \|\| ENGINE_MINOR_VERSION >= 7\s*Material->GetUsedTextures\(Textures\);/u);
  });
});

describe('analyze_view', () => {
  const handler = code('Domains', 'Perception', 'McpPerceptionAnalyze.cpp');

  it('is routed by inspect and reads the viewport through describe_view\'s own capture', () => {
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toContain(
      'return McpPerception::HandleAnalyzeView(*this, RequestId, Payload, RequestingSocket);',
    );
    expect(handler).toContain('McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("analyze_view"), true, View, Error, Code)');
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspectViewContents.cpp')).toContain(
      'McpCaptureView(CameraPayload, TEXT("describe_view"), bDiff, View, CaptureError, CaptureCode)',
    );
  });

  it('credits each cell to the actor the hit proxy names, as describe_view does', () => {
    expect(handler).toContain('Proxy = &Proxies.Add(Key, McpViewProxyOf(Id));');
    expect(handler).toContain('Frame.Actor.Add(Proxy->Kind == EMcpViewKind::Actor ? Proxy->Actor : nullptr);');
  });

  // A reviewer's tool that left the viewport in shader complexity would change what the user sees.
  it('puts the viewport\'s own view mode back after the shader cost frame', () => {
    const cost = code('Domains', 'Perception', 'McpPerceptionCost.cpp');
    expect(cost).toMatch(/const EViewModeIndex Was = Client\.GetViewMode\(\);\s*const bool bSwitch = !Client\.IsInGameView\(\);/u);
    expect(cost).toMatch(/Viewport->ReadPixels\(Pixels, FReadSurfaceDataFlags\(RCM_UNorm\)\);[\s\S]*?Client\.SetViewMode\(Was\);\s*if \(bSwitch\)\s*\{\s*Client\.SetGameView\(false\);/u);
    expect(cost.indexOf('Client.SetViewMode(Was);')).toBeLessThan(cost.indexOf('if (Pixels.Num() != Size.X * Size.Y)'));
  });

  // The level editor draws its axis widget, icons and gizmos into the viewport's pixels: a small enclosed shape at the
  // corner took the whole frame's saliency.
  it('judges the frame as the game draws it, then puts the viewport\'s own view back', () => {
    const capture = code('Domains', 'Environment', 'Inspection', 'McpEnvInspectViewPoints.cpp');
    expect(handler).toContain('McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("analyze_view"), true, View, Error, Code)');
    expect(capture).toMatch(/if \(bSwitch\)\s*\{\s*Out\.Client->SetGameView\(true\);/u);
    expect(capture).toMatch(/Out\.Ids = Viewport->GetRawHitProxyData\([^;]*\);\s*if \(bSwitch\)\s*\{\s*Out\.Client->SetGameView\(false\);/u);
  });

  // The view mode draws its legend into the frame: its red end over a floor read as the floor costing high.
  it('leaves the shader complexity legend and crosshair out of the cost', () => {
    const cost = code('Domains', 'Perception', 'McpPerceptionCost.cpp');
    expect(cost).toContain('const bool bLegend = X >= 46 && X < Size.X - 46 && Y >= Size.Y - 88 && Y < Size.Y - 34;');
    expect(cost).toMatch(/if \(IsOverlay\(PixelX, PixelY, Size\)\)\s*\{\s*continue;/u);
  });

  // A stack of translucent cubes first read as cheap as the floor: their shaders were still compiling.
  it('waits a few seconds for the complexity view\'s shaders, and warns when shaders were still compiling', () => {
    const cost = code('Domains', 'Perception', 'McpPerceptionCost.cpp');
    expect(cost).toMatch(/GShaderCompilingManager->IsCompiling\(\) && FPlatformTime::Seconds\(\) < Until;\)\s*\{\s*GShaderCompilingManager->ProcessAsyncResults\(false, false\);/u);
    expect(cost.indexOf('GShaderCompilingManager->ProcessAsyncResults')).toBeLessThan(cost.indexOf('Viewport->ReadPixels'));
    expect(handler).toMatch(/if \(const int32 Jobs = McpShaderJobsRemaining\(\)\)\s*\{\s*TArray<TSharedPtr<FJsonValue>> Warnings;/u);
  });

  it('scores harmony only: nothing is recoloured', () => {
    const color = code('Domains', 'Perception', 'McpPerceptionColor.cpp');
    expect(color).toContain('McpKMeans::Cluster(Frame.Lab, nullptr, PaletteSize, 12, 5)');
    expect(color).not.toMatch(/SetVectorParameterValue|SetMaterial|ColorSaturation|ColorGain/u);
  });

  it('reads the normal pass for key and fill only when a subject and a sun exist', () => {
    expect(handler).toMatch(/if \(Subject && FindSun\(View\.Client->GetWorld\(\), ToSun\)\)\s*\{\s*for \(const FLinearColor& Normal : McpEnvironmentHandlers::McpCapturePass\(/u);
  });

  // An unlit cube read 78% "toward the sun" while the camera saw its shadowed face: the normal pass holds no faces of it.
  it('gives an unlit subject a note, never a key to fill ratio', () => {
    const light = code('Domains', 'Perception', 'McpPerceptionLight.cpp');
    expect(light).toContain('!Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit)');
    expect(light).toMatch(/if \(DrawsUnlit\(\*Subject\)\)\s*\{\s*Row->SetStringField\(TEXT\("keyFillNote"\)[^;]*;\s*return Out;/u);
    expect(light.indexOf('if (DrawsUnlit(*Subject))')).toBeLessThan(light.indexOf('FVector3f::DotProduct(Normals[Index], ToSun)'));
  });
});

// describe_view saveAs and compareWith: what changed in the picture since a saved frame (Domains/Perception, Diff).
describe('describe_view saveAs and compareWith', () => {
  const view = code('Domains', 'Environment', 'Inspection', 'McpEnvInspectViewContents.cpp');
  const saved = code('Domains', 'Perception', 'McpPerceptionDiff.cpp');
  const frames = code('Domains', 'Perception', 'McpPerceptionDiffFrames.cpp');

  // A selection outline or the gizmo moving with the selection would read as a change.
  it('compares from the saved camera, on the frame as the game draws it', () => {
    expect(view).toMatch(/!McpPerception::PrepareViewDiff\(Payload, CameraPayload, CaptureError, CaptureCode\) \|\|\s*!McpCaptureView\(CameraPayload, TEXT\("describe_view"\), bDiff,/u);
    expect(view).toContain('const bool bDiff = !SaveAs.IsEmpty() || !GetJsonStringField(Payload, TEXT("compareWith")).IsEmpty();');
    expect(saved).toContain('OutCamera->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Saved->Location));');
  });

  // Temporal anti-aliasing never draws the same frame twice: an unchanged scene must read no change.
  it('learns the renderer\'s own noise from a second capture and counts a block only beyond it', () => {
    expect(saved).toMatch(/McpCaptureView\(MakeShared<FJsonObject>\(\), TEXT\("describe_view"\), true, Again, OutError, OutCode\)[\s\S]*?Diff::BlockChange\(Now, Diff::MeasureFrame\(Again\), Entry\.NoiseLoss, Entry\.NoiseColour\);/u);
    expect(frames).toContain('Loss[Block] / (LossFloor + NoiseFactor * Saved.NoiseLoss[Block])');
    expect(frames).toContain('Colour[Block] / (ColourFloor + NoiseFactor * Saved.NoiseColour[Block])');
  });

  it('refuses a frame of another size or field of view instead of comparing it', () => {
    expect(saved).toContain('OutCode = TEXT("VIEW_SIZE_CHANGED");');
    expect(saved).toContain('if (!FMath::IsNearlyEqual(Saved->Fov, View.Client->ViewFOV, 0.01f))');
  });
});

// inspect contact_sheet: one image of an actor from several sides (Domains/Perception, Sheet).
describe('contact_sheet', () => {
  const sheet = code('Domains', 'Perception', 'McpPerceptionSheet.cpp');
  const draw = code('Domains', 'Perception', 'McpPerceptionSheetDraw.cpp');
  const passes = code('Domains', 'Environment', 'Inspection', 'McpEnvInspectCapturePasses.cpp');

  it('is routed by inspect', () => {
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toContain(
      'return McpPerception::HandleContactSheet(*this, RequestId, Payload, RequestingSocket);',
    );
  });

  // A wall between a camera and the actor would fill the tile; the floor it is sunk into must stay.
  it('draws only the actor and what comes within reach of it, never moving the level viewport', () => {
    expect(passes).toMatch(/if \(ShowOnly\)\s*\{\s*Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;/u);
    expect(sheet).toContain('if (Box.IsValid && Box.Intersect(Reach))');
    // A subsystem's debug-draw actor, whose box spans the level, was listed beside a cube.
    expect(sheet).toContain('const bool bSkip = *It == Subject || It->IsHidden() || McpPlacement::IsBoundsOnlyActor(*It);');
    expect(sheet).not.toMatch(/SetViewLocation|SetViewRotation/u);
  });

  // From inside the floor slab a cube was sunk into, none of the slab's faces showed, and the cube read as resting.
  it('takes each elevation from outside everything it draws', () => {
    expect(sheet).toContain('const double Distance = bOrtho ? Beyond + 100.0');
    expect(sheet).toMatch(/for \(const FBox& Box : ShownBoxes\)\s*\{\s*Beyond = FMath::Max\(Beyond,/u);
  });

  it('measures the elevations in the actor\'s own turned frame, orthographic', () => {
    expect(sheet).toContain('const FTransform Frame(FRotator(0.0, Subject->GetActorRotation().Yaw, 0.0), Subject->GetActorLocation());');
    expect(sheet).toContain('const FBox Local = McpPlacement::DrawnBoundsIn(*Subject, Frame);');
    expect(passes).toContain('Capture->ProjectionType = OrthoWidth > 0.f ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;');
  });

  // In a level lit by one sun with no sky light, the faces turned from it read black; a cube sunk in a floor of its own
  // grey could not be told from the floor.
  it('shades from the geometry, the actor apart, its hidden parts shown through what covers them', () => {
    expect(sheet).toContain('Normals = Pass(SCS_Normal, Shown), Depth = Pass(SCS_SceneDepth, Shown), SubjectDepth = Pass(SCS_SceneDepth, Alone);');
    expect(sheet).not.toMatch(/SCS_SceneColor|SCS_FinalColor/u);
    expect(draw).toContain('FLinearColor Colour = (bOwn ? SubjectAlbedo : OtherAlbedo) * Lit;');
    expect(draw).toMatch(/if \(!bOwn && Own < Empty && Own > Near\)\s*\{\s*Colour = FMath::Lerp\(Colour, SubjectAlbedo \* 0\.8f, XRay\);/u);
  });

  // An orthographic view puts the far plane at a finite depth and leaves a stale normal where nothing is drawn: empty
  // space read as the actor, orange to the tile's edges.
  it('reads empty space off the far plane, not the normal', () => {
    expect(draw).toMatch(/for \(const FLinearColor& Pixel : Alone\)\s*\{\s*Far = FMath::Max\(Far, Pixel\.R\);/u);
    expect(draw).toMatch(/if \(Near >= Empty\)\s*\{\s*Out\[Index\] = TileBackground;/u);
    expect(draw).not.toContain('Normal.SizeSquared()');
  });
});
