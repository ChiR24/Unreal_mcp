import type { Logger } from '../utils/logging/logger.js';
import type { QueuedRequestItem } from './types.js';

interface CancelDelivery {
    readonly sendFrame: (autoId: string) => void;
    readonly rejectUnderlying: (autoId: string) => void;
    readonly log: Logger;
}

/**
 * Which automation requests (and queued items) each MCP request owns, so a
 * cancellation can reach them. One MCP request may fan out to several
 * automation requests; each automation request belongs to exactly one MCP
 * request. Entries are dropped on settle and on cancel, so nothing is retained.
 */
export class RequestCorrelation {
    private readonly autosByMcp = new Map<string, Set<string>>();
    private readonly mcpByAuto = new Map<string, string>();
    private readonly queuedByMcp = new Map<string, QueuedRequestItem[]>();

    public register(mcpRequestId: string | undefined, autoId: string): void {
        if (!mcpRequestId) return;
        this.mcpByAuto.set(autoId, mcpRequestId);
        const autos = this.autosByMcp.get(mcpRequestId) ?? new Set<string>();
        autos.add(autoId);
        this.autosByMcp.set(mcpRequestId, autos);
    }

    /** The MCP request that owns an automation request, for progress routing. */
    public mcpRequestIdsForAuto(autoId: string): string[] {
        const mcpRequestId = this.mcpByAuto.get(autoId);
        return mcpRequestId ? [mcpRequestId] : [];
    }

    public registerQueued(mcpRequestId: string | undefined, item: QueuedRequestItem): void {
        if (!mcpRequestId) return;
        const owned = this.queuedByMcp.get(mcpRequestId) ?? [];
        owned.push(item);
        this.queuedByMcp.set(mcpRequestId, owned);
    }

    public detachQueued(item: QueuedRequestItem): void {
        if (!item.mcpRequestId) return;
        const owned = this.queuedByMcp.get(item.mcpRequestId);
        if (!owned) return;
        const index = owned.indexOf(item);
        if (index >= 0) owned.splice(index, 1);
        if (owned.length === 0) this.queuedByMcp.delete(item.mcpRequestId);
    }

    /**
     * Remove and return the queued items owned by an MCP request id. Does not
     * reject; the caller owns the rejection so it can also remove from the queue.
     */
    public takeQueued(mcpRequestId: string): QueuedRequestItem[] {
        const owned = this.queuedByMcp.get(mcpRequestId);
        if (!owned) return [];
        this.queuedByMcp.delete(mcpRequestId);
        return owned;
    }

    /** The automation request settled (reply, timeout or transport failure). */
    public settle(autoId: string): void {
        const mcpRequestId = this.mcpByAuto.get(autoId);
        if (mcpRequestId === undefined) return;
        this.mcpByAuto.delete(autoId);
        const autos = this.autosByMcp.get(mcpRequestId);
        autos?.delete(autoId);
        if (autos?.size === 0) this.autosByMcp.delete(mcpRequestId);
    }

    /**
     * Send `cancel_request` for, and reject, every automation request an MCP
     * request owns. Non-throwing: a frame delivery failure is logged without
     * tokens and the remaining requests are still cancelled.
     */
    public cancel(mcpRequestId: string, delivery: CancelDelivery): void {
        const autos = this.autosByMcp.get(mcpRequestId);
        if (!autos) return;
        this.autosByMcp.delete(mcpRequestId);
        for (const autoId of autos) {
            this.mcpByAuto.delete(autoId);
            try {
                delivery.sendFrame(autoId);
            } catch {
                delivery.log.warn('Failed to deliver cancel_request frame to Unreal', { autoId });
            }
            try {
                delivery.rejectUnderlying(autoId);
            } catch {
                // Underlying tracker rejection is best-effort.
            }
        }
    }

    public clear(): void {
        this.autosByMcp.clear();
        this.mcpByAuto.clear();
        this.queuedByMcp.clear();
    }
}
