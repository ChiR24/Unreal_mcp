export interface Env {
  UE_HOST: string;
  UE_RC_WS_PORT: number;
  UE_RC_HTTP_PORT: number;
  UE_PROJECT_PATH?: string;
}

export function loadEnv(): Env {
  const host = process.env.UE_HOST || '127.0.0.1';
  const wsPort = Number(process.env.UE_REMOTE_CONTROL_PORT || process.env.UE_RC_WS_PORT || 30010);
  const httpPort = Number(process.env.UE_REMOTE_CONTROL_HTTP_PORT || process.env.UE_RC_HTTP_PORT || 30020);
  const projectPath = process.env.UE_PROJECT_PATH;

  return {
    UE_HOST: host,
    UE_RC_WS_PORT: wsPort,
    UE_RC_HTTP_PORT: httpPort,
    UE_PROJECT_PATH: projectPath,
  };
}
