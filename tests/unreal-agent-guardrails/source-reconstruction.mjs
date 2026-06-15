import assert from 'node:assert/strict';
import { mkdir, readFile, symlink, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import ts from 'typescript';

const root = process.cwd();
const studioKitDir = join(
  root,
  'plugins/UnrealAgent/Source/UnrealAgent/Private/Acp/StudioKit',
);

function extractFunctionBody(source, functionName) {
  const signature = `FString ${functionName}()`;
  const signatureIndex = source.indexOf(signature);
  assert.notEqual(signatureIndex, -1, `Missing ${functionName}`);
  const bodyStart = source.indexOf('{', signatureIndex + signature.length);
  assert.notEqual(bodyStart, -1, `Missing body for ${functionName}`);

  let depth = 0;
  let quote = '';
  for (let index = bodyStart; index < source.length; index += 1) {
    const character = source[index];
    if (quote) {
      if (character === '\\') {
        index += 1;
        continue;
      }
      if (character === quote) quote = '';
      continue;
    }
    if (character === '"' || character === "'") {
      quote = character;
      continue;
    }
    if (character === '{') depth += 1;
    if (character !== '}') continue;
    depth -= 1;
    if (depth === 0) return source.slice(bodyStart + 1, index);
  }
  throw new Error(`Unterminated body for ${functionName}`);
}

function expandGeneratedFunction(source, functionName, sections = {}) {
  const body = extractFunctionBody(source, functionName);
  const tokenPattern =
    /TEXT\("((?:\\.|[^"\\])*)"\)|(MakeGuardrails[A-Za-z]+Section)\(\)/gsu;
  let result = '';
  for (const match of body.matchAll(tokenPattern)) {
    if (match[1] !== undefined) {
      result += JSON.parse(`"${match[1]}"`);
      continue;
    }
    const section = sections[match[2]];
    assert.equal(typeof section, 'string', `Missing generated section ${match[2]}`);
    result += section;
  }
  return result;
}

async function generateGuardrailsSource() {
  const sourceNames = [
    'CommandSafety',
    'PreflightState',
    'MutationAdmission',
    'LocalPath',
    'LocalShell',
    'LocalMutation',
    '',
    'LocalTools',
    'Plugin',
  ];
  const sources = await Promise.all(
    sourceNames.map((name) =>
      readFile(
        join(studioKitDir, `UnrealAgentStudioKitGuardrails${name}.cpp`),
        'utf8',
      ),
    ),
  );
  const [
    commandSafetySource,
    preflightStateSource,
    mutationAdmissionSource,
    localPathSource,
    localShellSource,
    localMutationSource,
    coreSource,
    localSource,
    pluginSource,
  ] = sources;
  const commandSafety = expandGeneratedFunction(
    commandSafetySource,
    'MakeGuardrailsCommandSafetySection',
  );
  const preflightState = expandGeneratedFunction(
    preflightStateSource,
    'MakeGuardrailsPreflightStateSection',
  );
  const mutationAdmission = expandGeneratedFunction(
    mutationAdmissionSource,
    'MakeGuardrailsMutationAdmissionSection',
  );
  const localPath = expandGeneratedFunction(
    localPathSource,
    'MakeGuardrailsLocalPathSection',
  );
  const localShell = expandGeneratedFunction(
    localShellSource,
    'MakeGuardrailsLocalShellSection',
  );
  const localMutation = expandGeneratedFunction(
    localMutationSource,
    'MakeGuardrailsLocalMutationSection',
  );
  const core = expandGeneratedFunction(coreSource, 'MakeGuardrailsCoreSection', {
    MakeGuardrailsPreflightStateSection: preflightState,
    MakeGuardrailsMutationAdmissionSection: mutationAdmission,
  });
  const local = expandGeneratedFunction(
    localSource,
    'MakeGuardrailsLocalToolSection',
    {
      MakeGuardrailsCommandSafetySection: commandSafety,
      MakeGuardrailsLocalPathSection: localPath,
      MakeGuardrailsLocalShellSection: localShell,
      MakeGuardrailsLocalMutationSection: localMutation,
    },
  );
  return expandGeneratedFunction(pluginSource, 'MakeGuardrailsPlugin', {
    MakeGuardrailsCoreSection: core,
    MakeGuardrailsLocalToolSection: local,
  });
}

function transpileGuardrails(source) {
  const result = ts.transpileModule(source, {
    compilerOptions: {
      target: ts.ScriptTarget.ES2022,
      module: ts.ModuleKind.ESNext,
      strict: true,
    },
    reportDiagnostics: true,
  });
  const errors = (result.diagnostics ?? []).filter(
    (diagnostic) => diagnostic.category === ts.DiagnosticCategory.Error,
  );
  assert.deepEqual(
    errors.map((diagnostic) =>
      ts.flattenDiagnosticMessageText(diagnostic.messageText, '\n'),
    ),
    [],
    'Generated guardrails TypeScript must parse',
  );
  return result.outputText;
}

function collectTypeErrors(sourcePath) {
  const program = ts.createProgram([sourcePath], {
    target: ts.ScriptTarget.ES2022,
    module: ts.ModuleKind.NodeNext,
    moduleResolution: ts.ModuleResolutionKind.NodeNext,
    strict: true,
    noEmit: true,
    skipLibCheck: true,
    types: ['node'],
    typeRoots: [join(root, 'node_modules/@types')],
  });
  return ts
    .getPreEmitDiagnostics(program)
    .filter((diagnostic) => diagnostic.category === ts.DiagnosticCategory.Error)
    .map((diagnostic) => {
      const message = ts.flattenDiagnosticMessageText(
        diagnostic.messageText,
        '\n',
      );
      return diagnostic.file && diagnostic.start !== undefined
        ? `${diagnostic.file.fileName}:${diagnostic.file.getLineAndCharacterOfPosition(diagnostic.start).line + 1}: ${message}`
        : message;
    });
}

async function assertPluginTypes(source, temporaryDirectory) {
  const sourcePath = join(temporaryDirectory, 'unreal-agent-guardrails.ts');
  const packageScopeDirectory = join(
    temporaryDirectory,
    'node_modules/@opencode-ai',
  );
  const packageDirectory = join(packageScopeDirectory, 'plugin');
  await mkdir(packageScopeDirectory, { recursive: true });
  await symlink(
    join(root, 'node_modules/@opencode-ai/plugin'),
    packageDirectory,
    process.platform === 'win32' ? 'junction' : 'dir',
  );
  await writeFile(sourcePath, source, 'utf8');
  assert.deepEqual(
    collectTypeErrors(sourcePath),
    [],
    'Generated guardrails must match the pinned OpenCode plugin hook types',
  );
}

export async function loadGuardrailsModule(temporaryDirectory) {
  const source = await generateGuardrailsSource();
  const testableSource = source.replace(
    'export default UnrealAgentGuardrails',
    'export { UNREAL_BINARY_ASSET_PATTERN, containsDestructiveGitCommand, containsResolvedBinaryAssetPath, containsUnrealBinaryAssetGlob, normalizeShellForSafety, tokenizeShellCommand }; export default UnrealAgentGuardrails',
  );
  const compiled = transpileGuardrails(testableSource);
  await assertPluginTypes(source, temporaryDirectory);
  const modulePath = join(temporaryDirectory, 'unreal-agent-guardrails.mjs');
  await writeFile(modulePath, compiled, 'utf8');
  return import(`${pathToFileURL(modulePath).href}?v=${Date.now()}`);
}
