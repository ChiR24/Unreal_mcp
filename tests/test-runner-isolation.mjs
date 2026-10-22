import path from 'node:path';
import { resolveResponsePath } from './test-runner-response-utils.mjs';

/** Fail before fixtures or suite mutations when the live editor identity cannot be proven. */
export function assertIsolatedProject(response, expected) {
  if (!expected || typeof expected.projectName !== 'string' || !expected.projectName.trim() ||
      typeof expected.projectDir !== 'string' || !expected.projectDir.trim()) {
    throw new Error('Isolated tests require an explicit projectName and projectDir; set UNREAL_MCP_TEST_PROJECT_NAME and UE_PROJECT_PATH.');
  }
  if (response?.isError || response?.structuredContent?.success === false) throw new Error('Cannot verify isolated editor identity.');
  const actualName = resolveResponsePath(response, 'structuredContent.result.projectName');
  const actualDir = resolveResponsePath(response, 'structuredContent.result.projectDir');
  const engineVersion = resolveResponsePath(response, 'structuredContent.result.engineVersion');
  if (actualName !== expected.projectName || typeof actualDir !== 'string' || /redacted/i.test(actualDir)) {
    throw new Error('Connected editor is not the explicitly selected isolated project; no fixture operations were performed.');
  }
  const paths = /^[A-Za-z]:[\\/]/.test(expected.projectDir) ? path.win32 : path.posix;
  let expectedRoot = expected.projectDir;
  if (/\.uproject$/i.test(expectedRoot)) expectedRoot = paths.dirname(expectedRoot);
  if (!paths.isAbsolute(expectedRoot)) throw new Error('Isolated projectDir must be an absolute directory or .uproject path.');
  if (!paths.isAbsolute(actualDir) && (typeof expected.editorBaseDir !== 'string' || !paths.isAbsolute(expected.editorBaseDir))) {
    throw new Error('A relative editor projectDir requires an explicit absolute UE_EDITOR_BASE_DIR for identity verification.');
  }
  const normalize = value => {
    const result = paths.resolve(value).replace(/[\\/]+$/, '');
    return paths === path.win32 ? result.toLowerCase() : result;
  };
  const actualRoot = paths.isAbsolute(actualDir) ? actualDir : paths.resolve(expected.editorBaseDir, actualDir);
  if (normalize(actualRoot) !== normalize(expectedRoot)) throw new Error('Connected editor project directory differs from the selected isolated project.');
  if (expected.engineVersionPrefix && (typeof engineVersion !== 'string' || !engineVersion.startsWith(expected.engineVersionPrefix))) {
    throw new Error('Connected editor engine version differs from the selected validation engine.');
  }
}
