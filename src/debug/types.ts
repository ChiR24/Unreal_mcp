export type DebugMode = 'pie_observe' | 'standalone_debug' | 'attach';
export type DebugSessionState =
  | 'starting'
  | 'running'
  | 'stopped'
  | 'disconnected'
  | 'terminated'
  | 'error';

export type DebugJobState =
  | 'queued'
  | 'running'
  | 'passed'
  | 'failed'
  | 'cancelled'
  | 'timed_out';

export interface DebugCorrelationContext {
  requestId?: string;
  traceId: string;
  debugSessionId?: string;
  targetPid?: number;
  worldInstance?: string;
  frame?: number;
  thread?: number;
  timestamp: string;
  eventCursor?: number;
}

export interface DebugDiagnostic {
  code: string;
  severity: 'info' | 'warning' | 'error' | 'fatal';
  component: 'sidecar' | 'debug_host' | 'unreal_bridge' | 'runtime_probe';
  phase: string;
  retriable: boolean;
  message: string;
  causes?: string[];
  recoveryHints?: string[];
  artifactIds?: string[];
}

export interface DebugEvent {
  type: 'automation_event';
  event: string;
  sequence: number;
  timestamp: string;
  context: DebugCorrelationContext;
  payload?: unknown;
  message?: string;
}

export interface DebugSessionRecord {
  sessionId: string;
  mode: DebugMode;
  state: DebugSessionState;
  createdAt: string;
  updatedAt: string;
  projectPath?: string;
  map?: string;
  targetPid?: number;
  stoppedReason?: string;
  lastEventCursor: number;
  error?: DebugDiagnostic;
}

export interface DebugJobRecord {
  jobId: string;
  kind: string;
  state: DebugJobState;
  createdAt: string;
  updatedAt: string;
  context: DebugCorrelationContext;
  result?: unknown;
  diagnostic?: DebugDiagnostic;
}

export interface DebugArtifactRecord {
  artifactId: string;
  kind: string;
  absolutePath: string;
  size: number;
  sha256: string;
  createdAt: string;
  sessionId?: string;
  metadata?: Record<string, unknown>;
}

export interface DebugToolResult extends Record<string, unknown> {
  success: boolean;
  context: DebugCorrelationContext;
  error?: string;
  diagnostic?: DebugDiagnostic;
}
