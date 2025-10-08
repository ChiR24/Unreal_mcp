export interface Env {
  UE_HOST: string;
  UE_PROJECT_PATH?: string;
  UE_EDITOR_EXE?: string;
  UE_SCREENSHOT_DIR?: string;
}

export function loadEnv(): Env {
  const host = process.env.UE_HOST || '127.0.0.1';
  const projectPath = process.env.UE_PROJECT_PATH;
  const editorExe = process.env.UE_EDITOR_EXE;
  const screenshotDir = process.env.UE_SCREENSHOT_DIR;

  return {
    UE_HOST: host,
    UE_PROJECT_PATH: projectPath,
    UE_EDITOR_EXE: editorExe,
    UE_SCREENSHOT_DIR: screenshotDir,
  };
}
