import { cp, mkdir, readFile, readdir, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

const { values } = parseArgs({ options: {
  engine: { type: 'string' }, project: { type: 'string' }, fixtures: { type: 'boolean', default: false },
  'pcg-disabled': { type: 'boolean', default: false }, 'pcg-editor-disabled': { type: 'boolean', default: false },
  'ws-ports': { type: 'string', default: '18090,18091' }, 'native-port': { type: 'string', default: '13000' },
} });
if (!values.engine || !values.project) {
  throw new Error('Usage: node scripts/pcg-validation-project.mjs --engine <UE root> --project <new project directory>');
}
if ((values.fixtures && values['pcg-disabled']) || (values['pcg-disabled'] && values['pcg-editor-disabled'])) {
  throw new Error('Choose one optional-module variant; PCG fixtures require PCG.');
}
const ports = [...values['ws-ports'].split(','), values['native-port']];
if (ports.some(port => !/^\d+$/.test(port) || Number(port) < 1024 || Number(port) > 65535) || new Set(ports).size !== ports.length) {
  throw new Error('Validation ports must be distinct integers in 1024-65535.');
}
const engine = path.resolve(values.engine);
const project = path.resolve(values.project);
const source = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../plugins/EngineWire');
const version = JSON.parse(await readFile(path.join(engine, 'Engine/Build/Build.version'), 'utf8'));
await stat(path.join(engine, 'Engine/Binaries/Win64/UnrealEditor.exe'));
if (values.fixtures && (version.MajorVersion !== 5 || version.MinorVersion < 7)) {
  throw new Error('The optional context-handle/Blueprint fixtures currently require UE 5.7 or later.');
}
try {
  await stat(project);
  throw new Error('The project directory already exists; refusing to overwrite it.');
} catch (error) {
  if (error.code !== 'ENOENT') throw error;
}
const available = new Set();
async function collectPlugins(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const descriptors = entries.filter(entry => entry.isFile() && entry.name.endsWith('.uplugin'));
  for (const descriptor of descriptors) available.add(descriptor.name.slice(0, -8));
  if (descriptors.length) return;
  for (const entry of entries) {
    if (entry.isDirectory() && !['Binaries', 'Intermediate', 'Content', 'Resources', 'Source'].includes(entry.name)) {
      await collectPlugins(path.join(directory, entry.name));
    }
  }
}
await collectPlugins(path.join(engine, 'Engine/Plugins'));
const manifest = JSON.parse(await readFile(path.join(source, 'EngineWire.uplugin'), 'utf8'));
const plugins = [{ Name: 'EngineWire', Enabled: true }];
for (const dependency of manifest.Plugins) {
  if (available.has(dependency.Name)) {
    plugins.push({ Name: dependency.Name, Enabled: !['Fab', 'Bridge'].includes(dependency.Name) && !(values['pcg-disabled'] && dependency.Name === 'PCG') });
  }
}
if (!values['pcg-disabled'] && !plugins.some(plugin => plugin.Name === 'PCG' && plugin.Enabled)) {
  throw new Error('The engine must provide PCG for this validation project.');
}
await mkdir(project);
for (const directory of ['Config', 'Content', 'Source/PCGValidation', 'Plugins', 'Saved/PCGValidation']) {
  await mkdir(path.join(project, directory), { recursive: true });
}
await cp(source, path.join(project, 'Plugins/EngineWire'), {
  recursive: true,
  filter: file => !['Binaries', 'Intermediate', 'Saved', '.git'].includes(path.basename(file)),
});
if (values['pcg-editor-disabled']) {
  const rulesPath = path.join(project, 'Plugins/EngineWire/Source/EngineWire/EngineWire.Build.cs');
  const rules = await readFile(rulesPath, 'utf8');
  const marker = 'bool bHasPcgEditorApi = bHasPCG &&';
  if (rules.split(marker).length !== 2) throw new Error('PCGEditor detection changed; refusing an unverified conditional-build patch.');
  await writeFile(rulesPath, rules.replace(marker, 'bool bHasPcgEditorApi = false && bHasPCG &&'));
}
const files = {
  'PCGValidation.uproject': JSON.stringify({ FileVersion: 3,
    EngineAssociation: `${version.MajorVersion}.${version.MinorVersion}`,
    Description: 'Isolated EngineWire PCG acceptance project. Not production content.',
    Modules: [{ Name: 'PCGValidation', Type: 'Runtime', LoadingPhase: 'Default' }], Plugins: plugins,
  }, null, 2) + '\n',
  'Source/PCGValidation/PCGValidation.Build.cs': `using UnrealBuildTool;
public class PCGValidation : ModuleRules
{
    public PCGValidation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine"${values['pcg-disabled'] ? '' : ', "PCG"'} });
    }
}
`,
  'Source/PCGValidation/PCGValidation.cpp': `#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, PCGValidation, "PCGValidation");
`,
  'Source/PCGValidationEditor.Target.cs': `using UnrealBuildTool;
public class PCGValidationEditorTarget : TargetRules
{
    public PCGValidationEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("PCGValidation");
    }
}
`,
  'Source/PCGValidation.Target.cs': `using UnrealBuildTool;
public class PCGValidationTarget : TargetRules
{
    public PCGValidationTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("PCGValidation");
    }
}
`,
  'Config/DefaultGame.ini': `[/Script/EngineSettings.GeneralProjectSettings]
ProjectName=PCGValidation

[/Script/EngineWire.EngineWireSettings]
bAlwaysListen=True
ListenHost=127.0.0.1
ListenPorts=${values['ws-ports']}
bRequireCapabilityToken=True
bAllowNonLoopback=False
bEnableNativeMCP=True
NativeMCPPort=${values['native-port']}
`,
  'Config/DefaultEngine.ini': `[/Script/Engine.RendererSettings]
r.RayTracing=False
r.DynamicGlobalIlluminationMethod=0
r.ReflectionMethod=0

[/Script/Engine.Engine]
bUseFixedFrameRate=False
`,
};
for (const [name, content] of Object.entries(files)) {
  await writeFile(path.join(project, name), content, { flag: 'wx' });
}
if (values.fixtures) {
  const fixtures = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../tests/fixtures/pcg');
  for (const name of ['PCGValidationElements.h', 'PCGValidationElements.cpp']) {
    await cp(path.join(fixtures, name), path.join(project, 'Source/PCGValidation', name), { errorOnExist: true, force: false });
  }
}
console.info(`Created isolated PCGValidation project for UE ${version.MajorVersion}.${version.MinorVersion}.${version.PatchVersion}.`);
console.info('Only the new project was written; the installed engine was not changed.');
if (values['pcg-editor-disabled']) console.info('PCGEditor API branch is disabled in this test copy; this is NOT proof of a genuinely missing engine module.');
