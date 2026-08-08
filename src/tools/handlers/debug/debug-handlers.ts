import type { ITools } from '../../../types/tools/tool-interfaces.js';

function requireDebugService(tools: ITools) {
  if (!tools.debugService) throw new Error('Debug service is unavailable');
  return tools.debugService;
}

export async function handleDebugSession(action: string, args: Record<string, unknown>, tools: ITools): Promise<unknown> {
  return requireDebugService(tools).session(action, args);
}

export async function handleDebugBreakpoint(action: string, args: Record<string, unknown>, tools: ITools): Promise<unknown> {
  return requireDebugService(tools).breakpoint(action, args);
}

export async function handleDebugInspect(action: string, args: Record<string, unknown>, tools: ITools): Promise<unknown> {
  return requireDebugService(tools).inspect(action, args);
}

export async function handleDebugObserve(action: string, args: Record<string, unknown>, tools: ITools): Promise<unknown> {
  return requireDebugService(tools).observe(action, args);
}
