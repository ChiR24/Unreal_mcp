export interface Env {
  UE_HOST: string;
  UE_RC_WS_PORT: number;
  UE_RC_HTTP_PORT: number;
  UE_PROJECT_PATH?: string;
  UE_EDITOR_EXE?: string;
  UE_SCREENSHOT_DIR?: string;
}

export function loadEnv(): Env {
  const host = process.env.UE_HOST || '127.0.0.1';
  // Note: UE5 default is HTTP on 30010, WebSocket on 30020
  const wsPort = Number(process.env.UE_RC_WS_PORT || process.env.UE_REMOTE_CONTROL_WS_PORT || 30020);
  const httpPort = Number(process.env.UE_RC_HTTP_PORT || process.env.UE_REMOTE_CONTROL_HTTP_PORT || 30010);
  const projectPath = process.env.UE_PROJECT_PATH;
  const editorExe = process.env.UE_EDITOR_EXE;
  const screenshotDir = process.env.UE_SCREENSHOT_DIR;

  return {
    UE_HOST: host,
    UE_RC_WS_PORT: wsPort,
    UE_RC_HTTP_PORT: httpPort,
    UE_PROJECT_PATH: projectPath,
    UE_EDITOR_EXE: editorExe,
    UE_SCREENSHOT_DIR: screenshotDir,
  };
}
