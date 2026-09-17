import { describe, expect, it } from 'vitest';
import { bridgeNotConnectedMessage, describeBridgeFailure } from '../../../src/automation/bridge-config.js';

function errorWithCode(code: string, message = code): Error & { code: string } {
    return Object.assign(new Error(message), { code });
}

describe('describeBridgeFailure', () => {
    it('maps refusal, timeout and resolution failures to their closed-set reason', () => {
        expect(describeBridgeFailure(errorWithCode('ECONNREFUSED', 'connect ECONNREFUSED 127.0.0.1:8090'))).toBe('connection refused');
        expect(describeBridgeFailure(errorWithCode('ETIMEDOUT'))).toBe('timed out');
        expect(describeBridgeFailure(errorWithCode('UND_ERR_CONNECT_TIMEOUT'))).toBe('timed out');
        expect(describeBridgeFailure(new Error('Lazy connection timeout'))).toBe('timed out');
        expect(describeBridgeFailure(new Error('Handshake timeout'))).toBe('timed out');
        expect(describeBridgeFailure(errorWithCode('ENOTFOUND', 'getaddrinfo ENOTFOUND unreal.internal'))).toBe('host unreachable');
        expect(describeBridgeFailure(errorWithCode('EHOSTUNREACH'))).toBe('host unreachable');
    });

    it('maps TLS and socket failures to their closed-set reason', () => {
        expect(describeBridgeFailure(errorWithCode('ERR_TLS_CERT_ALTNAME_INVALID'))).toBe('tls failure');
        expect(describeBridgeFailure(new Error('self-signed certificate in certificate chain'))).toBe('tls failure');
        expect(describeBridgeFailure(errorWithCode('ECONNRESET'))).toBe('connection lost');
        expect(describeBridgeFailure(new Error('socket hang up'))).toBe('connection lost');
        expect(describeBridgeFailure(new Error('Socket closed during handshake'))).toBe('connection lost');
    });

    it('maps handshake rejections without echoing what the peer sent', () => {
        const peerControlled = 'Handshake expected bridge_ack, got {"type":"<script>alert(1)</script>"}';
        expect(describeBridgeFailure(new Error(peerControlled))).toBe('handshake rejected');
        expect(describeBridgeFailure(new Error('INVALID_CAPABILITY_TOKEN'))).toBe('handshake rejected');
    });

    it('falls back to a closed-set unknown instead of the raw message', () => {
        const raw = 'C:\\Projects\\Secret\\save.uasset exploded at 0x7ff';
        const reason = describeBridgeFailure(new Error(raw));

        expect(reason).toBe('unknown failure');
        expect(reason).not.toContain('Secret');
    });

    it('handles non-Error causes without stringifying them into the reason', () => {
        expect(describeBridgeFailure('connect ECONNREFUSED ::1:8090')).toBe('connection refused');
        expect(describeBridgeFailure(undefined)).toBe('unknown failure');
        expect(describeBridgeFailure({ code: 42 })).toBe('unknown failure');
    });
});

describe('bridgeNotConnectedMessage', () => {
    it('keeps the target and the closed-set reason', () => {
        expect(bridgeNotConnectedMessage('ws://127.0.0.1:8090', 'connection refused')).toBe(
            'Automation bridge not connected at ws://127.0.0.1:8090: connection refused. Ensure the Unreal Editor is running with the automation bridge listening.',
        );
    });

    it('still carries the transport marker telemetry matches on', () => {
        expect(bridgeNotConnectedMessage('ws://127.0.0.1:8090', 'timed out')).toContain('not connected');
    });

    it('omits the reason when none is known', () => {
        expect(bridgeNotConnectedMessage('ws://127.0.0.1:8090')).toBe(
            'Automation bridge not connected at ws://127.0.0.1:8090. Ensure the Unreal Editor is running with the automation bridge listening.',
        );
    });
});
