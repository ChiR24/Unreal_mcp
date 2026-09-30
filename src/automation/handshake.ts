import { WebSocket } from 'ws';
import type { AutomationSocket } from './connection-manager.js';
import {
    AutomationLogger,
    REDACTED_AUTOMATION_CREDENTIAL,
    redactKnownAutomationCredentials
} from './log-redaction.js';
import { bridgeAckSchema } from './message-schema.js';
import type { AutomationBridgeMessage } from './types.js';

export class HandshakeHandler {
    private log = new AutomationLogger('HandshakeHandler');
    private readonly DEFAULT_HANDSHAKE_TIMEOUT_MS = 5000;

    constructor(
        private readonly capabilityToken?: string,
        private readonly resolveToken?: () => Promise<string | undefined>
    ) {
    }

    /**
     * Send bridge_hello and resolve with the bridge_ack metadata. `onAcknowledged` runs inside the
     * bridge_ack 'message' dispatch, right after the handshake listener is removed: ws emits every
     * frame of one read in the same tick, so a listener added there sees the frame that follows
     * bridge_ack, and one added after `await` can miss it. If it throws, the handshake rejects.
     */
    public async initiateHandshake(
        socket: AutomationSocket,
        timeoutMs: number = this.DEFAULT_HANDSHAKE_TIMEOUT_MS,
        onAcknowledged?: (metadata: Record<string, unknown>) => void
    ): Promise<Record<string, unknown>> {
        return new Promise((resolve, reject) => {
            let settled = false;
            const timeout = setTimeout(() => {
                if (!settled) {
                    this.log.warn('Automation bridge client handshake timed out');
                    rejectHandshake(new Error('Handshake timeout'), 4002, 'Handshake timeout');
                }
            }, timeoutMs);

            const cleanup = () => {
                clearTimeout(timeout);
                socket.off('message', onMessage);
                socket.off('error', onError);
                socket.off('close', onClose);
            };

            const rejectHandshake = (error: Error, closeCode?: number, closeReason?: string): void => {
                if (settled) return;
                settled = true;
                if (closeCode !== undefined) {
                    socket.close(closeCode, closeReason);
                }
                cleanup();
                reject(error);
            };

            const resolveHandshake = (metadata: Record<string, unknown>): void => {
                if (settled) return;
                settled = true;
                cleanup();
                try {
                    onAcknowledged?.(metadata);
                } catch (error) {
                    reject(error instanceof Error ? error : new Error(String(error)));
                    return;
                }
                resolve(metadata);
            };

            const onMessage = (data: Buffer | string) => {
                let parsed: AutomationBridgeMessage;
                const text = typeof data === 'string' ? data : data.toString('utf8');
                try {
                    parsed = JSON.parse(text) as AutomationBridgeMessage;
                } catch (error) {
                    this.log.error('Received non-JSON automation message during handshake', error instanceof Error ? error : String(error));
                    rejectHandshake(new Error('Invalid JSON payload'), 4003, 'Invalid JSON payload');
                    return;
                }

                const validation = bridgeAckSchema.safeParse(parsed);
                if (validation.success) {
                    const metadata = this.sanitizeHandshakeMetadata(validation.data as Record<string, unknown>);
                    resolveHandshake(metadata);
                    return;
                }

                const typeHint = typeof parsed.type === 'string' ? parsed.type : 'unknown';
                this.log.warn(`Expected bridge_ack handshake, received ${typeHint}`, validation.error.issues);
                rejectHandshake(new Error(`Handshake expected bridge_ack, got ${typeHint}`), 4004, 'Handshake expected bridge_ack');
            };

            const onError = (error: Error) => {
                rejectHandshake(error);
            };

            const onClose = () => {
                rejectHandshake(new Error('Socket closed during handshake'));
            };

            socket.on('message', onMessage);
            socket.on('error', onError);
            socket.on('close', onClose);

            void (async () => {
                if (settled || socket.readyState !== WebSocket.OPEN) {
                    this.log.warn('Socket closed before bridge_hello could be sent');
                    return;
                }
                let capabilityToken: string | undefined;
                try {
                    capabilityToken = this.resolveToken
                        ? await this.resolveToken()
                        : this.capabilityToken;
                } catch (error) {
                    this.log.error(
                        'Capability token resolution failed; sending bridge_hello without a token',
                        error instanceof Error ? error.message : String(error)
                    );
                    capabilityToken = this.capabilityToken || undefined;
                }
                if (settled || socket.readyState !== WebSocket.OPEN) {
                    return;
                }
                const helloPayload: AutomationBridgeMessage = {
                    type: 'bridge_hello',
                    capabilityToken
                };
                // The socket may close while the token was read: ws then throws from send, which
                // inside this void async block was an unhandled rejection, not a failed handshake.
                try {
                    socket.send(JSON.stringify(helloPayload));
                } catch (error) {
                    rejectHandshake(error instanceof Error ? error : new Error(String(error)));
                }
            })();
        });
    }

    private sanitizeHandshakeMetadata(payload: Record<string, unknown>): Record<string, unknown> {
        const sanitized: Record<string, unknown> = { ...payload };
        delete sanitized.type;
        const capabilityToken = typeof sanitized.capabilityToken === 'string'
            ? sanitized.capabilityToken
            : undefined;
        if ('capabilityToken' in sanitized) {
            sanitized.capabilityToken = REDACTED_AUTOMATION_CREDENTIAL;
        }
        if (capabilityToken) {
            for (const [key, value] of Object.entries(sanitized)) {
                if (key !== 'capabilityToken') {
                    sanitized[key] = redactKnownAutomationCredentials(value, [capabilityToken]);
                }
            }
        }
        return sanitized;
    }
}
