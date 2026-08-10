import type { AutomationBridge } from './automation/index.js';
import { DEFAULT_AUTOMATION_HOST, DEFAULT_AUTOMATION_PORT } from './constants.js';
import { ErrorHandler } from './utils/responses/error-handler.js';
import { errorMessage } from './unreal-bridge-response.js';
import type { ConnectionEventInfo, UnrealBridgeLogger } from './unreal-bridge-types.js';

export interface AutomationBridgeListeners {
  readonly connected: (info: ConnectionEventInfo) => void;
  readonly disconnected: (info: ConnectionEventInfo) => void;
  readonly handshakeFailed: (info: ConnectionEventInfo) => void;
}

interface ConfigureAutomationBridgeParams {
  readonly currentBridge?: AutomationBridge;
  readonly listeners?: AutomationBridgeListeners;
  readonly nextBridge?: AutomationBridge;
  readonly log: UnrealBridgeLogger;
  readonly setConnected: (connected: boolean) => void;
}

interface TryConnectContext {
  readonly log: UnrealBridgeLogger;
  readonly getConnected: () => boolean;
  readonly setConnected: (connected: boolean) => void;
  readonly getAutomationBridge: () => AutomationBridge | undefined;
  readonly getConnectPromise: () => Promise<void> | undefined;
  readonly setConnectPromise: (promise: Promise<void> | undefined) => void;
  readonly connect: (timeoutMs: number) => Promise<void>;
}

interface ConnectContext {
  readonly getAutomationBridge: () => AutomationBridge | undefined;
  readonly setConnected: (connected: boolean) => void;
  readonly log: UnrealBridgeLogger;
}

export function parsePositiveIntegerEnv(raw: string | undefined): number | undefined {
  if (raw === undefined) return undefined;
  const trimmed = raw.trim();
  if (!/^\d+$/.test(trimmed)) return undefined;

  const parsed = Number(trimmed);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : undefined;
}

export function configureAutomationBridge(params: ConfigureAutomationBridgeParams): AutomationBridgeListeners | undefined {
  if (params.currentBridge && params.listeners) {
    params.currentBridge.off('connected', params.listeners.connected);
    params.currentBridge.off('disconnected', params.listeners.disconnected);
    params.currentBridge.off('handshakeFailed', params.listeners.handshakeFailed);
  }

  const automationBridge = params.nextBridge;
  if (!automationBridge) {
    params.setConnected(false);
    return undefined;
  }

  const onConnected = (info: ConnectionEventInfo) => {
    params.setConnected(true);
    params.log.debug('Automation bridge connected', info);
  };

  const onDisconnected = (info: ConnectionEventInfo) => {
    params.setConnected(false);
    params.log.debug('Automation bridge disconnected', info);
  };

  const onHandshakeFailed = (info: ConnectionEventInfo) => {
    params.setConnected(false);
    params.log.warn('Automation bridge handshake failed', info);
  };

  automationBridge.on('connected', onConnected);
  automationBridge.on('disconnected', onDisconnected);
  automationBridge.on('handshakeFailed', onHandshakeFailed);

  params.setConnected(automationBridge.isConnected());

  return {
    connected: onConnected,
    disconnected: onDisconnected,
    handshakeFailed: onHandshakeFailed
  };
}

export async function tryConnectWithBackoff(
  context: TryConnectContext,
  maxAttempts: number = 3,
  timeoutMs: number = 15000,
  retryDelayMs: number = 3000
): Promise<boolean> {
  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    context.log.info('🔌 MOCK MODE: Simulating active connection');
    context.setConnected(true);
    return true;
  }

  const automationBridge = context.getAutomationBridge();
  if (context.getConnected() && automationBridge?.isConnected()) {
    return true;
  }

  if (!automationBridge) {
    context.log.warn('Automation bridge is not configured; cannot establish connection.');
    return false;
  }

  if (automationBridge.isConnected()) {
    context.setConnected(true);
    return true;
  }

  const existingConnect = context.getConnectPromise();
  if (existingConnect) {
    try {
      await existingConnect;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      context.log.debug('Existing connect promise rejected', message);
    }
    return context.getConnected();
  }

  const connectPromise = ErrorHandler.retryWithBackoff(
    () => {
      const rawEnvTimeout = process.env.UNREAL_CONNECTION_TIMEOUT;
      const envTimeout = parsePositiveIntegerEnv(rawEnvTimeout);
      const actualTimeout = envTimeout ?? (rawEnvTimeout === undefined || rawEnvTimeout === '' ? 30000 : timeoutMs);
      return context.connect(actualTimeout);
    },
    {
      maxRetries: Math.max(0, maxAttempts - 1),
      initialDelay: retryDelayMs,
      maxDelay: 10000,
      backoffMultiplier: 1.5,
      shouldRetry: (error: unknown) => {
        const message = errorMessage(error).toLowerCase();
        return message.includes('timeout') || message.includes('connect') || message.includes('automation');
      }
    }
  ).catch((error: unknown) => {
    context.log.warn(`Automation bridge connection failed after ${maxAttempts} attempts:`, errorMessage(error));
    context.log.warn('⚠️  Ensure Unreal Editor is running with MCP Automation Bridge plugin enabled');
    context.log.warn(`⚠️  Plugin should listen on ws://${DEFAULT_AUTOMATION_HOST}:${DEFAULT_AUTOMATION_PORT} for MCP server connections`);
  });

  context.setConnectPromise(connectPromise);

  try {
    await connectPromise;
  } finally {
    context.setConnectPromise(undefined);
  }

  context.setConnected(context.getAutomationBridge()?.isConnected() ?? false);
  return context.getConnected();
}

export async function connectAutomationBridge(context: ConnectContext, timeoutMs: number = 15000): Promise<void> {
  const automationBridge = context.getAutomationBridge();
  if (!automationBridge) {
    throw new Error('Automation bridge not configured');
  }

  if (automationBridge.isConnected()) {
    context.setConnected(true);
    return;
  }

  automationBridge.start();

  const success = await waitForAutomationConnection(automationBridge, context.log, timeoutMs);
  if (!success) {
    throw new Error('Automation bridge connection timeout');
  }

  context.setConnected(true);
}

async function waitForAutomationConnection(
  automationBridge: AutomationBridge,
  log: UnrealBridgeLogger,
  timeoutMs: number
): Promise<boolean> {
  if (automationBridge.isConnected()) {
    return true;
  }

  return new Promise<boolean>((resolve) => {
    let settled = false;

    const cleanup = () => {
      if (settled) {
        return;
      }
      settled = true;
      automationBridge.off('connected', onConnected);
      automationBridge.off('handshakeFailed', onHandshakeFailed);
      automationBridge.off('error', onError);
      automationBridge.off('disconnected', onDisconnected);
      clearTimeout(timer);
    };

    const onConnected = (info: ConnectionEventInfo) => {
      cleanup();
      log.debug('Automation bridge connected while waiting', info);
      resolve(true);
    };

    const onHandshakeFailed = (info: ConnectionEventInfo) => {
      log.warn('Automation bridge handshake failed while waiting', info);
      cleanup();
      resolve(false);
    };

    const onError = (error: unknown) => {
      log.warn('Automation bridge error while waiting', error);
      cleanup();
      resolve(false);
    };

    const onDisconnected = (info: ConnectionEventInfo) => {
      log.warn('Automation bridge disconnected while waiting', info);
      cleanup();
      resolve(false);
    };

    const timer = setTimeout(() => {
      cleanup();
      resolve(false);
    }, Math.max(0, timeoutMs));

    automationBridge.on('connected', onConnected);
    automationBridge.on('handshakeFailed', onHandshakeFailed);
    automationBridge.on('error', onError);
    automationBridge.on('disconnected', onDisconnected);
  });
}
