import { randomUUID } from 'node:crypto';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';
import { DebugArtifactRegistry } from './artifact-registry.js';
import { BoundedEventStore, type DebugEventQuery } from './bounded-event-store.js';
import { DebugHostClient, DebugHostUnavailableError } from './debug-host-client.js';
import { expressionRequiresUnsafePermission, unsafePermissionGranted } from './expression-safety.js';
import { DebugJobManager } from './job-manager.js';
import { RuntimeProbeServer } from './runtime-probe-server.js';
import type {
  DebugCorrelationContext,
  DebugDiagnostic,
  DebugMode,
  DebugSessionRecord,
  DebugToolResult
} from './types.js';

const TERMINAL_STATES = new Set(['disconnected', 'terminated', 'error']);

function asRecord(value: unknown): Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
    ? value as Record<string, unknown>
    : {};
}

function stringArg(args: Record<string, unknown>, key: string): string | undefined {
  const value = args[key];
  return typeof value === 'string' && value.trim() ? value.trim() : undefined;
}

export class DebugService {
  readonly events = new BoundedEventStore(Number(process.env.UE_MCP_DEBUG_EVENT_CAPACITY) || 10_000);
  readonly jobs = new DebugJobManager();
  readonly artifacts: DebugArtifactRegistry;
  readonly host: DebugHostClient;
  readonly probes: RuntimeProbeServer;
  private readonly sessions = new Map<string, DebugSessionRecord>();
  private recordingStart = new Map<string, number>();
  private readonly abnormalBundles = new Set<string>();

  constructor(private readonly automationBridge: AutomationRequestBridge, projectPath = process.env.UE_PROJECT_PATH) {
    this.artifacts = new DebugArtifactRegistry(projectPath);
    this.host = new DebugHostClient(projectPath);
    this.probes = new RuntimeProbeServer((snapshot, context) => {
      const stored = this.events.append({ event: 'probe_snapshot', context, payload: snapshot });
      if (context.debugSessionId) this.touchSessionCursor(context.debugSessionId, stored.sequence);
    });
    this.host.on('event', (message: unknown) => this.ingestHostEvent(asRecord(message)));
    this.host.on('disconnected', () => this.markHostSessionsDisconnected());
  }

  ingestAutomationEvent(event: Record<string, unknown>): void {
    const rawContext = asRecord(event.context);
    const context = this.context({
      requestId: event.requestId,
      traceId: rawContext.traceId,
      sessionId: rawContext.debugSessionId,
      targetPid: rawContext.targetPid,
      frame: rawContext.frame,
      thread: rawContext.thread,
      worldInstance: rawContext.worldInstance
    });
    const eventName = stringArg(event, 'event') ?? 'unreal_event';
    const stored = this.events.append({
      event: eventName,
      context,
      payload: event.payload ?? event.result,
      message: stringArg(event, 'message'),
      timestamp: stringArg(event, 'timestamp')
    });
    if (context.debugSessionId) this.touchSessionCursor(context.debugSessionId, stored.sequence);
  }

  async session(action: string, args: Record<string, unknown>): Promise<DebugToolResult> {
    const context = this.context(args);
    try {
      if (action === 'list_targets') {
        let hostTargets: unknown[] = [];
        try {
          const result = asRecord(await this.host.request('listTargets'));
          hostTargets = Array.isArray(result.targets) ? result.targets : [];
        } catch (error) {
          if (!(error instanceof DebugHostUnavailableError)) throw error;
        }
        return this.ok(context, {
          targets: [{ id: 'pie', mode: 'pie_observe', available: this.automationBridge.isConnected() }, ...hostTargets],
          debugHost: this.host.status()
        });
      }
      if (action === 'start') return await this.startSession(args, context);

      const session = this.requireSession(args);
      context.debugSessionId = session.sessionId;
      if (action === 'status') {
        if (session.mode !== 'pie_observe' && !TERMINAL_STATES.has(session.state)) {
          const status = asRecord(await this.host.request('status', { sessionId: session.sessionId }));
          this.applyHostStatus(session, status);
        }
        return this.ok(context, { session: { ...session }, debugHost: this.host.status() });
      }
      if (action === 'stop' && args.terminate === true && !unsafePermissionGranted(args.unsafe)) {
        return this.fail(context, 'UNSAFE_PERMISSION_REQUIRED', 'Target termination requires UE_MCP_DEBUG_ALLOW_UNSAFE=true and unsafe:true.', false);
      }
      if (session.mode === 'pie_observe') {
        if (action === 'stop') {
          session.state = 'terminated';
          session.updatedAt = new Date().toISOString();
          return this.ok(context, { session: { ...session } });
        }
        return this.fail(context, 'DEBUG_SIDECAR_REQUIRED', `${action} requires a standalone native debug session.`, false);
      }
      const command = this.sessionCommand(action);
      const result = await this.host.request(command, { ...args, sessionId: session.sessionId }, 30_000);
      this.updateSessionForAction(session, action, asRecord(result));
      return this.ok(context, { session: { ...session }, result });
    } catch (error) {
      return this.fromError(context, error, 'debug_session');
    }
  }

  async breakpoint(action: string, args: Record<string, unknown>): Promise<DebugToolResult> {
    const context = this.context(args);
    try {
      const session = this.requireSession(args);
      context.debugSessionId = session.sessionId;
      if (session.mode === 'pie_observe') {
        return this.fail(context, 'DEBUG_SIDECAR_REQUIRED', 'Native breakpoints require standalone_debug or attach mode.', false);
      }
      const result = await this.host.request('breakpoint', { ...args, action, sessionId: session.sessionId });
      return this.ok(context, { result });
    } catch (error) {
      return this.fromError(context, error, 'debug_breakpoint');
    }
  }

  async inspect(action: string, args: Record<string, unknown>): Promise<DebugToolResult> {
    const context = this.context(args);
    try {
      const session = this.requireSession(args);
      context.debugSessionId = session.sessionId;
      if (session.mode === 'pie_observe') {
        return this.fail(context, 'DEBUG_SIDECAR_REQUIRED', 'Frozen native inspection requires standalone_debug or attach mode.', false);
      }
      if (action === 'evaluate') {
        const expression = stringArg(args, 'expression');
        if (!expression) return this.fail(context, 'INVALID_ARGUMENT', 'expression is required.', false);
        if (expressionRequiresUnsafePermission(expression) && !unsafePermissionGranted(args.unsafe)) {
          return this.fail(context, 'UNSAFE_EXPRESSION_REJECTED', 'Assignments, function calls, allocation and mutation require UE_MCP_DEBUG_ALLOW_UNSAFE=true and unsafe:true.', false);
        }
      }
      const result = await this.host.request('inspect', { ...args, action, sessionId: session.sessionId }, 30_000);
      return this.ok(context, { result });
    } catch (error) {
      return this.fromError(context, error, 'debug_inspect');
    }
  }

  async observe(action: string, args: Record<string, unknown>): Promise<DebugToolResult> {
    const context = this.context(args);
    try {
      if (action === 'query_events') {
        const query = this.eventQuery(args);
        return this.ok(context, this.events.query(query));
      }
      if (action === 'blueprint_diagnostics') {
        const query = this.eventQuery(args);
        const results = ['blueprint_exception', 'blueprint_compile_diagnostic']
          .flatMap((event) => this.events.query({ ...query, event, limit: query.limit ?? 100 }).events)
          .sort((left, right) => left.sequence - right.sequence);
        return this.ok(context, { events: results, nextCursor: results.at(-1)?.sequence ?? query.after ?? 0 });
      }
      if (action === 'probe_snapshot') {
        const snapshot = this.events.latest({ ...this.eventQuery(args), event: 'probe_snapshot' });
        return this.ok(context, { snapshot: snapshot ?? null, stale: snapshot === undefined });
      }
      if (action === 'start_recording') {
        const sessionId = stringArg(args, 'sessionId') ?? 'global';
        this.recordingStart.set(sessionId, this.events.getCursor());
        return this.ok(context, { sessionId, cursor: this.events.getCursor(), recording: true });
      }
      if (action === 'stop_recording') return await this.stopRecording(args, context);
      if (action === 'run_tests') return this.startAutomationJob('unreal_tests', 'manage_tests', args, context);
      if (action === 'test_status' || action === 'trace_status') return this.jobStatus(args, context);
      if (action === 'cancel_test') return await this.cancelJob(args, context);
      if (action === 'start_trace') return this.startAutomationJob('unreal_trace', 'manage_insights', { ...args, subAction: 'start_session' }, context);
      if (action === 'stop_trace') {
        return this.startAutomationJob(
          'unreal_trace_stop',
          'manage_insights',
          { ...args, subAction: 'stop_session' },
          context,
          async (response) => {
            const tracePath = this.findStringField(response, ['traceFilePath', 'tracePath', 'traceFile', 'outputPath']);
            if (!tracePath) throw new Error('Trace stop completed without a generated .utrace path');
            const artifact = await this.artifacts.registerFile(tracePath, 'unreal_trace', stringArg(args, 'sessionId') ?? 'global');
            return { ...asRecord(response), artifact };
          }
        );
      }
      if (action === 'create_bundle') return await this.createBundle(args, context);
      return this.fail(context, 'INVALID_ACTION', `Unsupported debug_observe action '${action}'.`, false);
    } catch (error) {
      return this.fromError(context, error, 'debug_observe');
    }
  }

  listSessions(): DebugSessionRecord[] {
    return Array.from(this.sessions.values(), (session) => ({ ...session }));
  }

  readResource(uri: string): unknown {
    const parsed = new URL(uri);
    const segments = parsed.pathname.split('/').filter(Boolean);
    if (parsed.hostname !== 'debug') throw new Error(`Unknown debug resource: ${uri}`);
    if (segments[0] === 'sessions' && segments.length === 1) return { sessions: this.listSessions() };
    if (segments[0] === 'session' && segments[1]) return this.sessions.get(segments[1]) ?? null;
    if (segments[0] === 'events' && segments[1]) {
      return this.events.query({
        sessionId: segments[1],
        after: Number(parsed.searchParams.get('after') ?? 0),
        limit: Number(parsed.searchParams.get('limit') ?? 100)
      });
    }
    if (segments[0] === 'jobs' && segments[1]) return this.jobs.get(segments[1]) ?? null;
    if (segments[0] === 'artifacts' && segments[1]) return this.artifacts.get(segments[1]) ?? null;
    if (segments[0] === 'health') return this.health();
    throw new Error(`Unknown debug resource: ${uri}`);
  }

  health(): Record<string, unknown> {
    return {
      protocolVersions: [2, 1],
      capabilities: ['structured_diagnostics', 'correlated_events', 'async_jobs', 'blueprint_diagnostics', 'runtime_probes'],
      debugHost: this.host.status(),
      sessions: { total: this.sessions.size, active: this.listSessions().filter((session) => !TERMINAL_STATES.has(session.state)).length },
      events: { cursor: this.events.getCursor(), dropped: this.events.getDroppedCount() },
      jobs: this.jobs.list(),
      artifacts: this.artifacts.health(),
      runtimeProbes: this.probes.health(),
      unsafeEnabled: process.env.UE_MCP_DEBUG_ALLOW_UNSAFE === 'true'
    };
  }

  private async startSession(args: Record<string, unknown>, context: DebugCorrelationContext): Promise<DebugToolResult> {
    const mode = (stringArg(args, 'mode') ?? 'standalone_debug') as DebugMode;
    if (!['pie_observe', 'standalone_debug', 'attach'].includes(mode)) {
      return this.fail(context, 'INVALID_ARGUMENT', `Unknown debug mode '${mode}'.`, false);
    }
    const sessionId = randomUUID();
    const now = new Date().toISOString();
    const session: DebugSessionRecord = {
      sessionId,
      mode,
      state: 'starting',
      createdAt: now,
      updatedAt: now,
      projectPath: stringArg(args, 'projectPath') ?? process.env.UE_PROJECT_PATH,
      map: stringArg(args, 'map'),
      lastEventCursor: this.events.getCursor()
    };
    this.sessions.set(sessionId, session);
    context.debugSessionId = sessionId;
    if (mode === 'pie_observe') {
      session.state = 'running';
      session.updatedAt = new Date().toISOString();
      return this.ok(context, { session: { ...session } });
    }
    const runtime = await this.probes.prepareLaunch(sessionId);
    const result = asRecord(await this.host.request('start', { ...args, ...runtime, mode, sessionId }, 60_000));
    this.applyHostStatus(session, result);
    if (session.state === 'starting') session.state = 'running';
    session.updatedAt = new Date().toISOString();
    return this.ok(context, { session: { ...session }, result });
  }

  private startAutomationJob(
    kind: string,
    bridgeAction: string,
    args: Record<string, unknown>,
    context: DebugCorrelationContext,
    onComplete?: (response: unknown) => Promise<unknown>
  ): DebugToolResult {
    const job = this.jobs.start(kind, context, async (signal) => {
      if (signal.aborted) throw new Error('Job cancelled');
      if (!this.automationBridge.isConnected()) throw new Error('Unreal automation bridge is not connected');
      const response = await this.automationBridge.sendAutomationRequest(bridgeAction, args, {
        timeoutMs: Number(args.timeoutMs) || 300_000,
        waitForEvent: bridgeAction === 'manage_tests',
        waitForEventTimeoutMs: Number(args.timeoutMs) || 300_000
      });
      return onComplete ? onComplete(response) : response;
    });
    return this.ok(context, { job });
  }

  private jobStatus(args: Record<string, unknown>, context: DebugCorrelationContext): DebugToolResult {
    const jobId = stringArg(args, 'jobId');
    if (!jobId) return this.fail(context, 'INVALID_ARGUMENT', 'jobId is required.', false);
    const job = this.jobs.get(jobId);
    return job ? this.ok(context, { job }) : this.fail(context, 'JOB_NOT_FOUND', `Job '${jobId}' was not found.`, false);
  }

  private async cancelJob(args: Record<string, unknown>, context: DebugCorrelationContext): Promise<DebugToolResult> {
    const jobId = stringArg(args, 'jobId');
    if (!jobId) return this.fail(context, 'INVALID_ARGUMENT', 'jobId is required.', false);
    const existing = this.jobs.get(jobId);
    if (!existing) return this.fail(context, 'JOB_NOT_FOUND', `Job '${jobId}' was not found.`, false);
    const job = this.jobs.cancel(jobId);
    let bridgeCancellation: unknown;
    if (existing.kind === 'unreal_tests' && ['queued', 'running'].includes(existing.state) && this.automationBridge.isConnected()) {
      try {
        bridgeCancellation = await this.automationBridge.sendAutomationRequest('manage_tests', { action: 'cancel_tests' }, { timeoutMs: 10_000 });
      } catch (error) {
        bridgeCancellation = { success: false, error: error instanceof Error ? error.message : String(error) };
      }
    }
    return this.ok(context, { job, bridgeCancellation });
  }

  private async stopRecording(args: Record<string, unknown>, context: DebugCorrelationContext): Promise<DebugToolResult> {
    const sessionId = stringArg(args, 'sessionId') ?? 'global';
    const after = this.recordingStart.get(sessionId);
    if (after === undefined) return this.fail(context, 'RECORDING_NOT_FOUND', `No event recording is active for '${sessionId}'.`, false);
    const recorded = this.events.query({ after, sessionId: sessionId === 'global' ? undefined : sessionId, limit: 1_000 });
    this.recordingStart.delete(sessionId);
    const artifact = await this.artifacts.createJson(sessionId, 'event_recording', 'events.json', recorded);
    return this.ok(context, { recording: false, artifact });
  }

  private async createBundle(args: Record<string, unknown>, context: DebugCorrelationContext): Promise<DebugToolResult> {
    const sessionId = stringArg(args, 'sessionId');
    if (!sessionId) return this.fail(context, 'INVALID_ARGUMENT', 'sessionId is required.', false);
    const session = this.sessions.get(sessionId);
    if (!session) return this.fail(context, 'DEBUG_SESSION_NOT_FOUND', `Session '${sessionId}' was not found.`, false);
    const bundle = {
      schemaVersion: 1,
      createdAt: new Date().toISOString(),
      session,
      events: this.events.query({ sessionId, limit: 1_000 }),
      jobs: this.jobs.list().filter((job) => job.context.debugSessionId === sessionId),
      artifacts: this.artifacts.list(sessionId),
      health: this.health()
    };
    const artifact = await this.artifacts.createJson(sessionId, 'debug_bundle_manifest', 'bundle-manifest.json', bundle);
    return this.ok(context, { artifact });
  }

  private ingestHostEvent(message: Record<string, unknown>): void {
    const sessionId = stringArg(message, 'sessionId');
    const event = stringArg(message, 'event') ?? 'debug_host_event';
    const session = sessionId ? this.sessions.get(sessionId) : undefined;
    if (session) {
      if (event === 'stopped') session.state = 'stopped';
      else if (event === 'continued') session.state = 'running';
      else if (event === 'terminated' || event === 'exited') session.state = 'terminated';
      else if (event === 'adapter_error') session.state = 'error';
      session.updatedAt = new Date().toISOString();
    }
    const stored = this.events.append({
      event: `debug_${event}`,
      context: this.context({ sessionId }),
      payload: message.payload
    });
    if (session) session.lastEventCursor = stored.sequence;
    const exitPayload = asRecord(message.payload);
    const abnormalAdapterExit = event === 'adapter_exit' && (exitPayload.code !== 0 || Boolean(exitPayload.signal));
    if (sessionId && (event === 'adapter_error' || abnormalAdapterExit || (event === 'exited' && exitPayload.exitCode !== 0))) {
      void this.collectAbnormalExitBundle(sessionId, message).catch((error) => {
        this.events.append({
          event: 'debug_bundle_failed',
          context: this.context({ sessionId }),
          message: error instanceof Error ? error.message : String(error)
        });
      });
    }
  }

  private async collectAbnormalExitBundle(sessionId: string, exit: Record<string, unknown>): Promise<void> {
    if (this.abnormalBundles.has(sessionId)) return;
    this.abnormalBundles.add(sessionId);
    const session = this.sessions.get(sessionId);
    if (!session) return;
    const configuredProject = session.projectPath ?? process.env.UE_PROJECT_PATH ?? process.cwd();
    const projectRoot = path.extname(configuredProject).toLowerCase() === '.uproject'
      ? path.dirname(configuredProject) : configuredProject;
    const projectName = path.basename(configuredProject, path.extname(configuredProject)) || path.basename(projectRoot);
    let unrealLogTail: string | null = null;
    const logPath = path.join(projectRoot, 'Saved', 'Logs', `${projectName}.log`);
    try {
      const log = await fs.readFile(logPath);
      unrealLogTail = log.subarray(Math.max(0, log.length - 256 * 1024)).toString('utf8');
    } catch { /* a crash can occur before Unreal creates its log */ }
    const crashReferences: unknown[] = [];
    const visit = async (directory: string): Promise<void> => {
      let entries;
      try { entries = await fs.readdir(directory, { withFileTypes: true }); } catch { return; }
      for (const entry of entries) {
        const absolutePath = path.join(directory, entry.name);
        if (entry.isDirectory()) await visit(absolutePath);
        else if (entry.isFile() && (entry.name.endsWith('.dmp') || entry.name.includes('CrashContext'))) {
          const artifact = await this.artifacts.registerFile(
            absolutePath, entry.name.endsWith('.dmp') ? 'minidump' : 'crash_context', sessionId);
          crashReferences.push(artifact);
        }
      }
    };
    await visit(path.join(projectRoot, 'Saved', 'Crashes'));
    const manifest = {
      schemaVersion: 1,
      abnormalExit: exit,
      session,
      events: this.events.query({ sessionId, limit: 1_000 }),
      unrealLog: { absolutePath: logPath, tail: unrealLogTail },
      adapterEvents: this.events.query({ sessionId, regex: '^debug_', limit: 1_000 }).events,
      crashReferences
    };
    const artifact = await this.artifacts.createJson(
      sessionId, 'abnormal_exit_manifest', 'abnormal-exit-manifest.json', manifest);
    this.events.append({
      event: 'debug_bundle_created',
      context: this.context({ sessionId }),
      payload: { artifact }
    });
  }

  private markHostSessionsDisconnected(): void {
    for (const session of this.sessions.values()) {
      if (session.mode !== 'pie_observe' && !TERMINAL_STATES.has(session.state)) {
        session.state = 'disconnected';
        session.updatedAt = new Date().toISOString();
      }
    }
  }

  private requireSession(args: Record<string, unknown>): DebugSessionRecord {
    const sessionId = stringArg(args, 'sessionId');
    if (!sessionId) throw new Error('sessionId is required');
    const session = this.sessions.get(sessionId);
    if (!session) throw new Error(`Debug session '${sessionId}' was not found`);
    return session;
  }

  private applyHostStatus(session: DebugSessionRecord, status: Record<string, unknown>): void {
    const state = stringArg(status, 'state');
    if (state && ['starting', 'running', 'stopped', 'disconnected', 'terminated', 'error'].includes(state)) {
      session.state = state as DebugSessionRecord['state'];
    }
    if (typeof status.targetPid === 'number') session.targetPid = status.targetPid;
    session.stoppedReason = stringArg(status, 'stoppedReason') ?? session.stoppedReason;
    session.updatedAt = new Date().toISOString();
  }

  private updateSessionForAction(session: DebugSessionRecord, action: string, result: Record<string, unknown>): void {
    if (action === 'pause' || action === 'next' || action === 'step_in' || action === 'step_out') session.state = 'stopped';
    if (action === 'continue') session.state = 'running';
    if (action === 'stop') session.state = 'terminated';
    if (action === 'stop') this.probes.revokeSession(session.sessionId);
    this.applyHostStatus(session, result);
  }

  private sessionCommand(action: string): string {
    const commands: Record<string, string> = {
      pause: 'pause',
      continue: 'continue',
      next: 'next',
      step_in: 'stepIn',
      step_out: 'stepOut',
      stop: 'stop'
    };
    const command = commands[action];
    if (!command) throw new Error(`Unsupported debug_session action '${action}'`);
    return command;
  }

  private eventQuery(args: Record<string, unknown>): DebugEventQuery {
    return {
      after: typeof args.after === 'number' ? args.after : undefined,
      limit: typeof args.limit === 'number' ? args.limit : undefined,
      event: stringArg(args, 'event'),
      sessionId: stringArg(args, 'sessionId'),
      requestId: stringArg(args, 'requestId'),
      severity: stringArg(args, 'severity'),
      category: stringArg(args, 'category'),
      regex: stringArg(args, 'regex'),
      since: stringArg(args, 'since'),
      until: stringArg(args, 'until')
    };
  }

  private findStringField(value: unknown, names: string[]): string | undefined {
    const record = asRecord(value);
    for (const name of names) {
      if (typeof record[name] === 'string' && record[name]) return record[name] as string;
    }
    for (const childName of ['result', 'payload', 'data']) {
      if (typeof record[childName] === 'object' && record[childName] !== null) {
        const nested = this.findStringField(record[childName], names);
        if (nested) return nested;
      }
    }
    return undefined;
  }

  private touchSessionCursor(sessionId: string, cursor: number): void {
    const session = this.sessions.get(sessionId);
    if (session) session.lastEventCursor = cursor;
  }

  private context(values: Record<string, unknown>): DebugCorrelationContext {
    const traceId = typeof values.traceId === 'string' && values.traceId ? values.traceId : randomUUID();
    return {
      traceId,
      timestamp: new Date().toISOString(),
      ...(typeof values.requestId === 'string' ? { requestId: values.requestId } : {}),
      ...(typeof values.sessionId === 'string' ? { debugSessionId: values.sessionId } : {}),
      ...(typeof values.targetPid === 'number' ? { targetPid: values.targetPid } : {}),
      ...(typeof values.frame === 'number' ? { frame: values.frame } : {}),
      ...(typeof values.thread === 'number' ? { thread: values.thread } : {}),
      ...(typeof values.worldInstance === 'string' ? { worldInstance: values.worldInstance } : {})
    };
  }

  private ok(context: DebugCorrelationContext, fields: Record<string, unknown>): DebugToolResult {
    return { success: true, context, ...fields };
  }

  private fail(context: DebugCorrelationContext, code: string, message: string, retriable: boolean): DebugToolResult {
    const diagnostic: DebugDiagnostic = {
      code,
      severity: 'error',
      component: code === 'DEBUG_HOST_UNAVAILABLE' ? 'debug_host' : 'sidecar',
      phase: 'request',
      retriable,
      message,
      recoveryHints: code === 'DEBUG_HOST_UNAVAILABLE'
        ? ['Open the Unreal project workspace in VS Code.', 'Install and enable unreal-mcp-debug-host.', 'Confirm Saved/McpDebug/debug-host.json exists.']
        : undefined
    };
    return { success: false, error: message, context, diagnostic };
  }

  private fromError(context: DebugCorrelationContext, error: unknown, phase: string): DebugToolResult {
    const message = error instanceof Error ? error.message : String(error);
    const code = error instanceof DebugHostUnavailableError
      ? error.code
      : message.includes('was not found') ? 'DEBUG_SESSION_NOT_FOUND' : 'DEBUG_OPERATION_FAILED';
    const result = this.fail(context, code, message, code === 'DEBUG_HOST_UNAVAILABLE');
    if (result.diagnostic) result.diagnostic.phase = phase;
    return result;
  }
}
