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
    /** Set by cleanup() so a synchronous settle is visible to the code that publishes the handle. */
    private connectionAttemptSettled = false;

    constructor(private readonly deps: ConnectionLifecycleDependencies) { }

    public async ensureConnected(): Promise<void> {
        if (!this.deps.enabled) {
            throw new Error('Automation bridge disabled');
        }

        this.deps.log.info('Automation bridge not connected, attempting lazy connection...');
        let attempt = this.connectionPromise;
        if (!attempt && !this.connectionLock) {
            this.connectionLock = true;
            this.connectionAttemptSettled = false;
            attempt = this.createConnectionPromise();
            // `cleanup()` can run SYNCHRONOUSLY inside the executor: the error
            // listener is registered before `startClient()`, and a WebSocket
            // constructor throw (malformed subprotocol, a token with a newline
            // in a header) is re-emitted synchronously. cleanup() clears
            // `connectionPromise`, so publishing the handle unconditionally here
            // resurrected an already-rejected promise and pinned it for the
            // process lifetime - every later ensureConnected() re-awaited the
            // same dead rejection and never called startClient() again.
            // Only publish a handle that is still in flight; the local `attempt`
            // still carries the rejection to this caller either way.
            if (!this.connectionAttemptSettled) this.connectionPromise = attempt;
        }

        try {
            await this.waitForConnection(attempt);
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
                this.connectionAttemptSettled = true;
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

    private async waitForConnection(attempt: Promise<void> | undefined): Promise<void> {
        // Awaiting the attempt handed in by the caller, not `this.connectionPromise`:
        // the field can be cleared by a synchronous cleanup() before we get here,
        // and `Promise.race([undefined, timeout])` resolves IMMEDIATELY - which
        // would report a successful connection that never happened.
        if (!attempt) {
            throw new Error('No connection attempt in flight');
        }

        let timeoutId: ReturnType<typeof setTimeout> | undefined;
        const timeoutPromise = new Promise<never>((_, reject) => {
            timeoutId = setTimeout(() => reject(new Error('Lazy connection timeout')), this.deps.connectionTimeoutMs);
        });

        try {
            await Promise.race([attempt, timeoutPromise]);
        } finally {
            if (timeoutId) clearTimeout(timeoutId);
        }
    }
}

function getErrorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}
