import { describe, expect, it, vi } from 'vitest';

import { Logger } from '../utils/logging/logger.js';
import { RequestCorrelation } from './request-correlation.js';
import type { QueuedRequestItem } from './types.js';

function delivery() {
    return {
        sendFrame: vi.fn<(autoId: string) => void>(),
        rejectUnderlying: vi.fn<(autoId: string) => void>(),
        log: new Logger('request-correlation-test', 'error')
    };
}

describe('RequestCorrelation', () => {
    it('maps an automation request back to its owning MCP request', () => {
        const correlation = new RequestCorrelation();
        correlation.register('str:a', 'auto-1');
        expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual(['str:a']);
        expect(correlation.mcpRequestIdsForAuto('auto-2')).toEqual([]);
    });

    it('ignores a request with no MCP id', () => {
        const correlation = new RequestCorrelation();
        correlation.register(undefined, 'auto-1');
        expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual([]);
    });

    it('cancel sends one frame and rejects each owned automation request exactly once', () => {
        const correlation = new RequestCorrelation();
        correlation.register('str:a', 'auto-1');
        correlation.register('str:a', 'auto-2');
        const d = delivery();

        correlation.cancel('str:a', d);
        correlation.cancel('str:a', d);

        expect(d.sendFrame.mock.calls.map(([id]) => id)).toEqual(['auto-1', 'auto-2']);
        expect(d.rejectUnderlying.mock.calls.map(([id]) => id)).toEqual(['auto-1', 'auto-2']);
    });

    it('a settled request is no longer cancellable', () => {
        const correlation = new RequestCorrelation();
        correlation.register('str:a', 'auto-1');
        correlation.settle('auto-1');
        const d = delivery();

        correlation.cancel('str:a', d);

        expect(d.sendFrame).not.toHaveBeenCalled();
        expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual([]);
    });

    it('keeps numeric and string MCP ids with the same text apart', () => {
        const correlation = new RequestCorrelation();
        correlation.register('num:1', 'auto-num');
        correlation.register('str:1', 'auto-str');
        const d = delivery();

        correlation.cancel('num:1', d);

        expect(d.sendFrame.mock.calls).toEqual([['auto-num']]);
        expect(correlation.mcpRequestIdsForAuto('auto-str')).toEqual(['str:1']);
    });

    it('still cancels the rest when a frame delivery throws', () => {
        const correlation = new RequestCorrelation();
        correlation.register('str:a', 'auto-1');
        correlation.register('str:a', 'auto-2');
        const d = delivery();
        d.sendFrame.mockImplementationOnce(() => { throw new Error('socket gone'); });

        expect(() => correlation.cancel('str:a', d)).not.toThrow();
        expect(d.rejectUnderlying.mock.calls.map(([id]) => id)).toEqual(['auto-1', 'auto-2']);
    });

    it('hands back and forgets the queued items an MCP request owns', () => {
        const correlation = new RequestCorrelation();
        const first = { mcpRequestId: 'str:a' } as QueuedRequestItem;
        const second = { mcpRequestId: 'str:a' } as QueuedRequestItem;
        correlation.registerQueued('str:a', first);
        correlation.registerQueued('str:a', second);
        correlation.detachQueued(first);

        expect(correlation.takeQueued('str:a')).toEqual([second]);
        expect(correlation.takeQueued('str:a')).toEqual([]);
    });
});
