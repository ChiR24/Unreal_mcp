export type DebugSessionState =
  | 'starting'
  | 'running'
  | 'stopped'
  | 'disconnected'
  | 'terminated'
  | 'error';

export interface MutableDapSessionState {
  state: DebugSessionState;
  targetPid?: number;
  stoppedReason?: string;
}

export function applyDapEvent(
  record: MutableDapSessionState,
  event: string,
  body: Record<string, unknown> = {}
): void {
  if (event === 'stopped') {
    record.state = 'stopped';
    record.stoppedReason = typeof body.reason === 'string' ? body.reason : undefined;
  } else if (event === 'continued') {
    record.state = 'running';
    record.stoppedReason = undefined;
  } else if (event === 'terminated' || event === 'exited') {
    record.state = 'terminated';
  }

  if (event === 'process' && typeof body.systemProcessId === 'number') {
    record.targetPid = body.systemProcessId;
  }
}

export function applyAdapterExit(
  record: MutableDapSessionState,
  code: number | undefined,
  signal: string | undefined
): void {
  if ((code !== undefined && code !== 0) || signal) record.state = 'error';
}
