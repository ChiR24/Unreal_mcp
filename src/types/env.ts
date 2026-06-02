export interface Env {
  UE_PROJECT_PATH?: string;
  UE_EDITOR_EXE?: string;
  UE_SCREENSHOT_DIR?: string;
}

function readOptionalEnv(name: keyof NodeJS.ProcessEnv): string | undefined {
  const value = process.env[name];
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

export function loadEnv(): Env {
  return {
    UE_PROJECT_PATH: readOptionalEnv('UE_PROJECT_PATH'),
    UE_EDITOR_EXE: readOptionalEnv('UE_EDITOR_EXE'),
    UE_SCREENSHOT_DIR: readOptionalEnv('UE_SCREENSHOT_DIR'),
  };
}
