import { mkdtemp, mkdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

async function createDirectoryLink(target, path) {
  await symlink(
    target,
    path,
    process.platform === 'win32' ? 'junction' : 'dir',
  );
}

async function tryCreateFileLink(target, path) {
  try {
    await symlink(target, path, 'file');
    return true;
  } catch (error) {
    if (
      process.platform === 'win32' &&
      error &&
      typeof error === 'object' &&
      'code' in error &&
      ['EPERM', 'EACCES', 'UNKNOWN'].includes(String(error.code))
    ) {
      return false;
    }
    throw error;
  }
}

export async function createProjectFixture() {
  const temporaryDirectory = await mkdtemp(
    join(tmpdir(), 'unreal-agent-guardrails-'),
  );
  const projectDirectory = join(temporaryDirectory, 'Project');
  await Promise.all([
    mkdir(join(projectDirectory, 'Content'), { recursive: true }),
    mkdir(join(projectDirectory, 'Content/Sub'), { recursive: true }),
    mkdir(join(projectDirectory, 'Config'), { recursive: true }),
    mkdir(join(projectDirectory, 'Docs'), { recursive: true }),
    mkdir(join(projectDirectory, 'Docs/Content'), { recursive: true }),
    mkdir(join(projectDirectory, 'Docs/Config'), { recursive: true }),
  ]);
  await writeFile(
    join(projectDirectory, 'Content/Danger.uasset'),
    'asset',
    'utf8',
  );
  await writeFile(
    join(projectDirectory, 'Docs/Notes.md'),
    'documentation',
    'utf8',
  );
  await writeFile(
    join(projectDirectory, 'Docs/guide.uasset.md'),
    'binary extension documentation',
    'utf8',
  );
  await createDirectoryLink(
    join(projectDirectory, 'Content'),
    join(projectDirectory, 'Assets'),
  );
  await createDirectoryLink(
    join(projectDirectory, 'Content/Sub'),
    join(projectDirectory, 'Alias'),
  );
  await createDirectoryLink(
    join(projectDirectory, 'Config'),
    join(projectDirectory, 'cfg'),
  );
  await createDirectoryLink(
    join(projectDirectory, 'Content'),
    join(projectDirectory, 'Docs/My Alias'),
  );
  const hasBinaryFileLink = await tryCreateFileLink(
    process.platform === 'win32'
      ? join(projectDirectory, 'Content/Danger.uasset')
      : '../Content/Danger.uasset',
    join(projectDirectory, 'Docs/cache.bin'),
  );
  const hasSafeFileLink = await tryCreateFileLink(
    process.platform === 'win32'
      ? join(projectDirectory, 'Docs/Notes.md')
      : 'Notes.md',
    join(projectDirectory, 'Docs/safe-link.md'),
  );

  return {
    createDirectoryLink,
    hasBinaryFileLink,
    hasSafeFileLink,
    projectDirectory,
    temporaryDirectory,
  };
}

export async function removeProjectFixture(temporaryDirectory) {
  await rm(temporaryDirectory, { recursive: true, force: true });
}
