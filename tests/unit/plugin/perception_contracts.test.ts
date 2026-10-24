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

describe('analyze_view', () => {
  const handler = code('Domains', 'Perception', 'McpPerceptionAnalyze.cpp');

  it('is routed by inspect and reads the viewport through describe_view\'s own capture', () => {
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toContain(
      'return McpPerception::HandleAnalyzeView(*this, RequestId, Payload, RequestingSocket);',
    );
    expect(handler).toContain('McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("analyze_view"), true, View, Error, Code)');
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspectViewContents.cpp')).toContain(
      'McpCaptureView(Payload, TEXT("describe_view"), false, View, CaptureError, CaptureCode)',
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

