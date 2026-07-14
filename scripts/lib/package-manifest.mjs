import { createHash } from 'node:crypto';
import { createReadStream, writeFileSync } from 'node:fs';
import { basename, resolve } from 'node:path';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';

const SHA256_PATTERN = /^[a-f0-9]{64}$/;
const REPRODUCIBILITY_NOTE =
  'Archive bytes and SHA-256 are reproducible for identical package inputs; generatedAt records manifest creation time.';

/** @typedef {{ readonly filename: string, readonly sha256: string }} ArchiveEntry */
/**
 * @typedef {object} ManifestOptions
 * @property {readonly ArchiveEntry[]} archives
 * @property {string} engineTarget
 * @property {string} generatedAt
 * @property {string} pluginName
 * @property {string} ueRoot
 * @property {string} version
 */

/**
 * Build a manifest with recursively stable key and archive ordering.
 * @param {ManifestOptions} options
 */
export function buildManifest(options) {
  const archives = [...options.archives]
    .map(({ filename, sha256 }) => {
      if (!SHA256_PATTERN.test(sha256)) {
        throw new TypeError(`Invalid SHA-256 for archive ${filename}`);
      }
      return { filename, sha256 };
    })
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
export function serializeManifest(manifest) {
  return `${JSON.stringify(manifest, null, 2)}\n`;
}

async function main() {
  const [outputPath, pluginName, version, engineTarget, ueRoot, ...archivePaths] = process.argv.slice(2);
  if (!outputPath || !pluginName || !version || !engineTarget || !ueRoot || archivePaths.length === 0) {
    console.error(
      'Usage: node package-manifest.mjs <output> <plugin> <version> <engine-target> <ue-root> <archive...>',
    );
    process.exitCode = 1;
    return;
  }

  const archives = await Promise.all(
    archivePaths.map(async (archivePath) => ({
      filename: basename(archivePath),
      sha256: await sha256File(archivePath),
    })),
  );
  const manifest = buildManifest({
    archives,
    engineTarget,
    generatedAt: new Date().toISOString(),
    pluginName,
    ueRoot,
    version,
  });
  writeFileSync(outputPath, serializeManifest(manifest), 'utf8');
  console.log(`Package manifest: ${outputPath}`);
}

const entryPath = process.argv[1];
if (entryPath && resolve(entryPath) === fileURLToPath(import.meta.url)) {
  await main();
}
