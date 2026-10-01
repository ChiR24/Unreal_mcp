// Foliage replies that hid or skipped what they did. configure_foliage_mesh set the mesh and answered
// configuredPropertyCount 0 and an empty list, because only the reflected `settings` entries were ever named;
// the placed instances of the type were never mentioned. Wiring contracts only: behaviour needs an editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

const DOMAINS = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...segments: readonly string[]): string =>
  readFileSync(join(DOMAINS, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('configure_foliage names what it wrote and what became of the placed instances', () => {
  const configure = (): string =>
    sliceBetween(read('Environment', 'Runtime', 'McpAutomationBridge_EnvironmentHandlersFoliageAssets.cpp'), 'bool McpConfigureFoliageType(', 'bool McpCreateLandscapeLayerInfo(');

  it('names every setting it wrote by its property name, the handled ones as well as the reflected ones', () => {
    const source = configure();

    for (const name of ['Mesh', 'Density', 'AlignToNormal', 'RandomYaw', 'BodyInstance', 'CullDistance']) {
      expect(source, name).toContain(`Applied.Add(TEXT("${name}"));`);
    }
    expect(source).toMatch(/for \(const TCHAR \*Written : \{TEXT\("Scaling"\), TEXT\("ScaleX"\), TEXT\("ScaleY"\), TEXT\("ScaleZ"\)\}\)\s*\{\s*Applied\.Add\(Written\);\s*\}/u);
    expect(source.indexOf('TArray<FString> Applied;'), 'declared before the first setting is written').toBeLessThan(source.indexOf('Applied.Add(TEXT("Mesh"));'));
    expect(source).toContain('McpApplyPayloadSettings(FoliageType, ReflectPayload, Applied, Failed);');
    expect(source).toContain('Resp->SetNumberField(TEXT("configuredPropertyCount"), Applied.Num());');
    expect(source, 'the count no longer covers only the reflected entries').not.toContain('ReflectedCount');
  });

  it('says the mesh it replaced and how many placed instances now draw the new one, read back after the type refreshed them', () => {
    const source = configure();

    expect(source).toMatch(/const UStaticMesh \*Previous = Instanced->GetStaticMesh\(\);\s*Resp->SetStringField\(TEXT\("previousMesh"\), Previous \? Previous->GetPathName\(\) : FString\(\)\);\s*Instanced->SetStaticMesh\(Mesh\);\s*NewMesh = Mesh;\s*Applied\.Add\(TEXT\("Mesh"\)\);/u);
    expect(source).toMatch(/Placed = Info->Instances\.Num\(\);\s*const UHierarchicalInstancedStaticMeshComponent \*Component = Info->GetComponent\(\);\s*Showing = Component && Component->GetStaticMesh\(\) == NewMesh \? Placed : 0;/u);
    expect(source).toContain('Resp->SetNumberField(TEXT("placedInstances"), Placed);');
    expect(source).toContain('Resp->SetNumberField(TEXT("instancesShowingNewMesh"), Showing);');
    expect(source.indexOf('FoliageType->PostEditChange();'), 'the instances are counted after the type told them').toBeLessThan(source.indexOf('Showing = Component'));
    expect(source, 'only a mesh change reports instances').toMatch(/if \(NewMesh\)\s*\{\s*int32 Placed = 0;/u);
  });

  it('the record declares what the reply carries', () => {
    const properties = capabilityIndex().byId.get('build_environment.configure_foliage')?.schemas.output.properties;
    const declared = isRecord(properties) ? Object.keys(properties) : [];

    for (const name of ['configuredProperties', 'configuredPropertyCount', 'previousMesh', 'placedInstances', 'instancesShowingNewMesh']) {
      expect(declared, name).toContain(name);
    }
  });
});

// get_foliage_instances answered one `scale` number per instance, the X axis alone, so a tall thin tree
// (2, 1, 0.5) read as uniform. It now answers the whole scale.
describe('get_foliage_instances reports the whole scale of an instance', () => {
  it('writes scale as {x, y, z}, and no longer as the X axis alone', () => {
    const source = read('Foliage', 'McpAutomationBridge_FoliageHandlersGetInstances.cpp');

    expect(source).toMatch(/TSharedPtr<FJsonObject> Scale = MakeShared<FJsonObject>\(\);\s*Scale->SetNumberField\(TEXT\("x"\), Inst\.DrawScale3D\.X\);\s*Scale->SetNumberField\(TEXT\("y"\), Inst\.DrawScale3D\.Y\);\s*Scale->SetNumberField\(TEXT\("z"\), Inst\.DrawScale3D\.Z\);\s*InstObj->SetObjectField\(TEXT\("scale"\), Scale\);/u);
    expect(source).not.toContain('SetNumberField(TEXT("scale")');
  });

  it('the record says what the list carries for one type', () => {
    const summary = capabilityIndex().byId.get('build_environment.get_foliage_instances')?.discovery.summary ?? '';

    expect(summary).toMatch(/rotation and scale \(\{x, y, z\}: the whole scale/u);
  });
});
