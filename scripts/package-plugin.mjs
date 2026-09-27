// Package McpAutomationBridge as pre-built binaries for Blueprint-only projects.
//
//   node scripts/package-plugin.mjs <UnrealEngineDir> [OutputDir] [extra RunUAT args...]
//
// Stages the plugin source, builds it with RunUAT BuildPlugin, marks the output
// Installed, strips debug symbols, zips it and writes a SHA-256 manifest beside
// the archive.
//
// Windows: BuildPlugin nests HostProject\Plugins\<plugin>\Intermediate\Build\Win64\...
// under the staging dir and UBT refuses paths over 260 characters, so the staging
// root must stay shallow. Point MCP_PACKAGE_STAGING_ROOT at one (e.g. X:\t) when
// the temp dir is itself too deep.

import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cpSync, createReadStream, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { basename, dirname, join, resolve } from 'node:path';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';

const PLUGIN = 'McpAutomationBridge';
const SYMBOL_FILE = /\.(pdb|debug|sym)$/i;
// NOT "reproducible": the zip stores each entry's build-time mtime, so the same
// sources packaged twice produce different bytes and a different digest.
const REPRODUCIBILITY_NOTE =
  'SHA-256 identifies these exact archive bytes. Archives are NOT bit-reproducible: '
  + 'zip entries embed build-time modification stamps, so repackaging identical sources '
  + 'yields a different digest. generatedAt records manifest creation time.';

/**
 * Manifest with stable key and archive ordering.
 * @param {{ archives: readonly { filename: string, sha256: string }[], engineTarget: string, generatedAt: string, pluginName: string, ueRoot: string, version: string }} options
 */
export function buildManifest(options) {
  const archives = options.archives
    .map(({ filename, sha256 }) => ({ filename, sha256 }))
    .sort((left, right) => (left.filename < right.filename ? -1 : left.filename > right.filename ? 1 : 0));
  return {
    archives,
    engineTarget: options.engineTarget,
    generatedAt: new Date(options.generatedAt).toISOString(),
    pluginName: options.pluginName,
    reproducibility: REPRODUCIBILITY_NOTE,
    ueRoot: options.ueRoot,
    version: options.version,
  };
}

/** @param {string} filePath */
export async function sha256File(filePath) {
  const hash = createHash('sha256');
  await pipeline(createReadStream(filePath), hash);
  return hash.digest('hex');
}

/** @param {ReturnType<typeof buildManifest>} manifest */
export const serializeManifest = (manifest) => `${JSON.stringify(manifest, null, 2)}\n`;

const readJson = (path) => JSON.parse(readFileSync(path, 'utf8'));
const writeJson = (path, value) => writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`);

function fail(message) {
  console.error(`ERROR: ${message}`);
  process.exit(1);
}

function run(command, args, env) {
  // A .bat can only run through cmd; quote every argument so paths with spaces survive.
  const result = process.platform === 'win32'
    ? spawnSync('cmd.exe', ['/d', '/s', '/c', `"${[command, ...args].map((a) => `"${a}"`).join(' ')}"`],
      { stdio: 'inherit', env, windowsVerbatimArguments: true })
    : spawnSync(command, args, { stdio: 'inherit', env });
  if (result.status !== 0) fail(`${basename(command)} exited with ${result.status ?? result.signal}`);
}

function* walk(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    yield { path, isDir: entry.isDirectory() };
    if (entry.isDirectory()) yield* walk(path);
  }
}

function stripSymbols(pluginDir) {
  for (const { path, isDir } of [...walk(pluginDir)]) {
    if (!existsSync(path)) continue;
    if (isDir ? /\.dsym$/i.test(path) : SYMBOL_FILE.test(path)) rmSync(path, { recursive: true, force: true });
  }
  for (const dir of ['Intermediate', 'Saved', '.cache', 'DerivedDataCache']) rmSync(join(pluginDir, dir), { recursive: true, force: true });
  const left = [...walk(pluginDir)].filter(({ path, isDir }) => (isDir ? /\.dsym$/i.test(path) : SYMBOL_FILE.test(path)));
  if (left.length > 0) fail(`debug symbols survived stripping: ${left.map((entry) => entry.path).join(', ')}`);
}

async function main() {
  const [engineDir, ...rest] = process.argv.slice(2);
  if (!engineDir) fail('usage: node scripts/package-plugin.mjs <UnrealEngineDir> [OutputDir] [extra RunUAT args...]');
  const outputArgs = rest.filter((arg) => !arg.startsWith('-'));
  if (outputArgs.length > 1) fail(`unexpected extra output directory argument: ${outputArgs[1]}`);
  const extraArgs = rest.filter((arg) => arg.startsWith('-'));

  const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
  const pluginFile = join(repoRoot, 'plugins', PLUGIN, `${PLUGIN}.uplugin`);
  const outputDir = resolve(outputArgs[0] ?? join(repoRoot, 'build'));
  mkdirSync(outputDir, { recursive: true });

  const platform = { win32: 'Win64', darwin: 'Mac', linux: 'Linux' }[process.platform] ?? fail(`unsupported platform ${process.platform}`);
  const runUat = join(engineDir, 'Engine', 'Build', 'BatchFiles', process.platform === 'win32' ? 'RunUAT.bat' : 'RunUAT.sh');
  if (!existsSync(runUat)) fail(`RunUAT not found: ${runUat} (the first argument must be the UE installation root)`);

  const versionFile = join(engineDir, 'Engine', 'Build', 'Build.version');
  const ue = existsSync(versionFile) ? readJson(versionFile) : undefined;
  const ueVer = ue ? `${ue.MajorVersion}.${ue.MinorVersion}` : 'unknown';
  const pluginVer = readJson(pluginFile).VersionName;
  const zipPath = join(outputDir, `${PLUGIN}-v${pluginVer}-UE${ueVer}-${platform}.zip`);
  const manifestPath = zipPath.replace(/\.zip$/, '.manifest.json');

  console.log(`Packaging ${PLUGIN} ${pluginVer} for UE ${ueVer} ${platform}\n  engine: ${engineDir}\n  output: ${zipPath}`);
  rmSync(zipPath, { force: true });
  rmSync(manifestPath, { force: true });

  const staging = mkdtempSync(join(process.env.MCP_PACKAGE_STAGING_ROOT ?? tmpdir(), 'mcpab-'));
  try {
    const sourceDir = join(staging, 'source', PLUGIN);
    const packageDir = join(staging, PLUGIN);
    cpSync(join(repoRoot, 'plugins', PLUGIN), sourceDir, { recursive: true });
    const sourcePlugin = join(sourceDir, `${PLUGIN}.uplugin`);
    // PCG ships as an engine plugin from 5.2 on, so the packaged descriptor requires it outright.
    if (ue && (ue.MajorVersion > 5 || (ue.MajorVersion === 5 && ue.MinorVersion >= 2))) {
      const descriptor = readJson(sourcePlugin);
      for (const dependency of descriptor.Plugins ?? []) if (dependency.Name === 'PCG') delete dependency.Optional;
      writeJson(sourcePlugin, descriptor);
    }

    // Without this every staged (writable) source is ejected from the unity blobs.
    // UBT reads UnrealBuildTool_<Category>__<Field>, scoping it to this build only.
    run(runUat, ['BuildPlugin', `-Plugin=${sourcePlugin}`, `-Package=${packageDir}`, `-TargetPlatforms=${platform}`, '-Rocket', ...extraArgs],
      { ...process.env, UnrealBuildTool_BuildConfiguration__bUseAdaptiveUnityBuild: 'false' });

    const pluginDir = [packageDir, join(packageDir, 'HostProject', 'Plugins', PLUGIN)]
      .find((dir) => existsSync(join(dir, `${PLUGIN}.uplugin`))) ?? fail(`packaged plugin output not found under ${packageDir}`);
    const outputPlugin = join(pluginDir, `${PLUGIN}.uplugin`);
    writeJson(outputPlugin, { ...readJson(outputPlugin), Installed: true });
    stripSymbols(pluginDir);

    // bsdtar (Windows 10+, macOS) writes zip from the extension; Linux GNU tar cannot.
    const zipResult = process.platform === 'linux'
      ? spawnSync('zip', ['-qr', zipPath, basename(pluginDir)], { cwd: dirname(pluginDir), stdio: 'inherit' })
      : spawnSync('tar', ['-a', '-c', '-f', zipPath, basename(pluginDir)], { cwd: dirname(pluginDir), stdio: 'inherit' });
    if (zipResult.status !== 0 || !existsSync(zipPath) || statSync(zipPath).size === 0) fail('failed to create the zip archive');

    const manifest = buildManifest({
      archives: [{ filename: basename(zipPath), sha256: await sha256File(zipPath) }],
      engineTarget: `UE${ueVer}-${platform}`,
      generatedAt: new Date().toISOString(),
      pluginName: PLUGIN,
      ueRoot: engineDir,
      version: pluginVer,
    });
    writeFileSync(manifestPath, serializeManifest(manifest), 'utf8');
  } finally {
    rmSync(staging, { recursive: true, force: true });
  }
  console.log(`Done.\n  archive:  ${zipPath}\n  manifest: ${manifestPath}\nTo install: unzip into YourProject/Plugins/`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main();
