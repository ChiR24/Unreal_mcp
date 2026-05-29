import { WebSocket } from 'ws';
import { Logger } from '../utils/logger.js';
import { AutomationBridgeMessage } from './types.js';
import { bridgeAckSchema } from './message-schema.js';
import { EventEmitter } from 'node:events';

export class HandshakeHandler extends EventEmitter {
    private log = new Logger('HandshakeHandler');
    private readonly DEFAULT_HANDSHAKE_TIMEOUT_MS = 5000;

    constructor(
        private capabilityToken?: string
    ) {
        super();
    }

    public initiateHandshake(socket: WebSocket, timeoutMs: number = this.DEFAULT_HANDSHAKE_TIMEOUT_MS): Promise<Record<string, unknown>> {
        return new Promise((resolve, reject) => {
            let settled = false;
            let helloTimer: NodeJS.Timeout | undefined;
            let timeout: NodeJS.Timeout;

            const cleanup = () => {
                clearTimeout(timeout);
                if (helloTimer) {
                    clearTimeout(helloTimer);
                    helloTimer = undefined;
                }
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
                resolve(metadata);
            };

            timeout = setTimeout(() => {
                if (!settled) {
                    this.log.warn('Automation bridge client handshake timed out');
                    rejectHandshake(new Error('Handshake timeout'), 4002, 'Handshake timeout');
                }
            }, timeoutMs);

            const onMessage = (data: Buffer | string) => {
                let parsed: AutomationBridgeMessage;
                const text = typeof data === 'string' ? data : data.toString('utf8');
                try {
                    parsed = JSON.parse(text) as AutomationBridgeMessage;
                } catch (error) {
                    this.log.error('Received non-JSON automation message during handshake', error);
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

            // Send bridge_hello with a slight delay to ensure the server has registered its handlers
            helloTimer = setTimeout(() => {
                if (!settled && socket.readyState === WebSocket.OPEN) {
                    const helloPayload: AutomationBridgeMessage = {
                        type: 'bridge_hello',
                        capabilityToken: this.capabilityToken || undefined
                    };
                    this.log.debug('Sending bridge_hello (delayed)');
                    socket.send(JSON.stringify(helloPayload));
                } else {
                    this.log.warn('Socket closed before bridge_hello could be sent');
                }
            }, 500);
        });
    }

    private sanitizeHandshakeMetadata(payload: Record<string, unknown>): Record<string, unknown> {
        const sanitized: Record<string, unknown> = { ...payload };
        delete sanitized.type;
        if ('capabilityToken' in sanitized) {
            sanitized.capabilityToken = 'REDACTED';
        }
        return sanitized;
    }
}
