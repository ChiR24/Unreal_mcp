import type { AutomationBridge } from './automation/index.js';

export interface UnrealBridgeLogger {
  debug(message: string, ...details: unknown[]): void;
  info(message: string, ...details: unknown[]): void;
  warn(message: string, ...details: unknown[]): void;
  error(message: string, ...details: unknown[]): void;
}

export interface ConnectionEventInfo {
  readonly host?: string;
  readonly port?: number;
  readonly reason?: string;
  readonly error?: string;
  readonly [key: string]: unknown;
}

export interface AutomationRequestResponse {
  readonly success?: boolean;
  readonly message?: string;
  readonly error?: string;
  readonly requestId?: string;
  readonly result?: unknown;
}

export interface BridgeConsoleResponse extends AutomationRequestResponse {
  readonly transport?: string;
}

export interface ObjectPropertyReadParams {
  readonly objectPath: string;
  readonly propertyName: string;
  readonly timeoutMs?: number;
}

export interface ObjectPropertyWriteParams {
  readonly objectPath: string;
  readonly propertyName: string;
  readonly value: unknown;
  readonly markDirty?: boolean;
  readonly timeoutMs?: number;
}

export interface BatchConsoleCommand {
  readonly command?: string;
}

export interface BatchConsoleOptions {
  readonly continueOnError?: boolean;
  readonly delayMs?: number;
}

export interface BatchConsoleResult {
  readonly success: boolean;
  readonly totalCommands: number;
  readonly executedCount: number;
  readonly failedCount: number;
  readonly results: ReadonlyArray<{
    readonly command: string;
    readonly success: boolean;
    readonly error?: string;
  }>;
}

export interface PartialBatchConsoleResult {
  readonly success?: boolean;
  readonly totalCommands?: number;
  readonly executedCount?: number;
  readonly failedCount?: number;
  readonly results?: BatchConsoleResult['results'];
}

export interface EngineVersionInfo {
  readonly version: string;
  readonly major: number;
  readonly minor: number;
  readonly patch: number;
  readonly isUE56OrAbove: boolean;
}

export interface FeatureFlagsInfo {
  readonly subsystems: {
    readonly unrealEditor: boolean;
    readonly levelEditor: boolean;
    readonly editorActor: boolean;
  };
}

export interface ConsoleCommandContext {
  readonly log: UnrealBridgeLogger;
  readonly getAutomationBridge: () => AutomationBridge | undefined;
  readonly runThrottled: <T>(command: () => Promise<T>, priority: number) => Promise<T>;
}

export interface StandardEditorOptions {
  readonly timeoutMs?: number;
}

