import type { Logger } from '../utils/logging/logger.js';
import type {
    AutomationBridgeEvents
} from './types.js';

export interface ConnectionLifecycleDependencies {
    readonly enabled: boolean;
    readonly connectionTimeoutMs: number;
    readonly log: Logger;
    readonly startClient: () => void;
    readonly abortPendingConnection: (reason: Error) => void;
    readonly once: <K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ) => void;
    readonly off: <K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ) => void;
}

/**
 * Owns the lazy-connection promise, lock, and abort bookkeeping for the bridge
 * request dispatcher. The dispatcher delegates connection orchestration here so
 * its own surface stays focused on request dispatch and cancellation.
 */
export class ConnectionLifecycle {
    private connectionPromise?: Promise<void>;
    private connectionLock = false;
    private connectionAttemptCleanup?: () => void;
    private connectionAttemptReject?: (reason: Error) => void;

    constructor(private readonly deps: ConnectionLifecycleDependencies) { }

    public async ensureConnected(): Promise<void> {
        if (!this.deps.enabled) {
            throw new Error('Automation bridge disabled');
        }

        this.deps.log.info('Automation bridge not connected, attempting lazy connection...');
        if (!this.connectionPromise && !this.connectionLock) {
            this.connectionLock = true;
            this.connectionPromise = this.createConnectionPromise();
        }

        try {
            await this.waitForConnection();
        } catch (error) {
            const message = getErrorMessage(error);
            if (message === 'Lazy connection timeout') {
                this.abort(new Error('Lazy connection timeout'));
            }
            this.deps.log.error('Lazy connection failed', error);
            throw new Error(`Failed to establish connection to Unreal Engine: ${message}`);
        }
    }

    public abort(reason: Error): void {
        const rejectConnectionAttempt = this.connectionAttemptReject;
        const cleanup = this.connectionAttemptCleanup;
        if (cleanup) {
            cleanup();
        } else {
            this.connectionLock = false;
            this.connectionPromise = undefined;
            this.connectionAttemptReject = undefined;
        }

        if (rejectConnectionAttempt) {
            rejectConnectionAttempt(reason);
        }
        this.deps.abortPendingConnection(reason);
    }

    private createConnectionPromise(): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            const onConnect = () => {
                cleanup();
                resolve();
            };
            const onError = (error: Error) => {
                cleanup();
                reject(error);
            };
            const onHandshakeFail = (info: { reason: string }) => {
                cleanup();
                reject(new Error(`Handshake failed: ${String(info.reason)}`));
            };
            const cleanup = () => {
                this.deps.off('connected', onConnect);
                this.deps.off('error', onError);
                this.deps.off('handshakeFailed', onHandshakeFail);
                this.connectionLock = false;
                this.connectionPromise = undefined;
                if (this.connectionAttemptCleanup === cleanup) {
                    this.connectionAttemptCleanup = undefined;
                    this.connectionAttemptReject = undefined;
                }
            };

            this.connectionAttemptCleanup = cleanup;
            this.connectionAttemptReject = reject;
            this.deps.once('connected', onConnect);
            this.deps.once('error', onError);
            this.deps.once('handshakeFailed', onHandshakeFail);

            try {
                this.deps.startClient();
            } catch (error) {
                onError(error instanceof Error ? error : new Error(String(error)));
            }
        });
    }

    private async waitForConnection(): Promise<void> {
        let timeoutId: ReturnType<typeof setTimeout> | undefined;
        const timeoutPromise = new Promise<never>((_, reject) => {
            timeoutId = setTimeout(() => reject(new Error('Lazy connection timeout')), this.deps.connectionTimeoutMs);
        });

        try {
            await Promise.race([this.connectionPromise, timeoutPromise]);
        } finally {
            if (timeoutId) clearTimeout(timeoutId);
        }
    }
}

function getErrorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}
