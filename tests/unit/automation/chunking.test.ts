import { describe, it, expect, vi, beforeEach } from 'vitest';
import { MessageHandler } from '../../../src/automation/message-handler.js';
import { RequestTracker } from '../../../src/automation/request-tracker.js';
import { AutomationBridgeResponseChunk, AutomationBridgeResponseMessage, PendingRequest } from '../../../src/automation/types.js';

describe('MessageHandler - Chunking', () => {
    let requestTracker: RequestTracker;
    let messageHandler: MessageHandler;

    beforeEach(() => {
        // Mock RequestTracker
        requestTracker = {
            getPendingRequest: vi.fn(),
            resolveRequest: vi.fn(),
            rejectRequest: vi.fn(),
        } as unknown as RequestTracker;

        messageHandler = new MessageHandler(requestTracker);
    });

    it('should reassemble chunked responses', () => {
        const requestId = 'req-123';
        const originalResponse: AutomationBridgeResponseMessage = {
            type: 'automation_response',
            requestId: requestId,
            success: true,
            message: 'Done',
            result: { data: 'Long data' }
        };
        const fullJson = JSON.stringify(originalResponse);
        
        // Split into 3 chunks
        const part1 = fullJson.slice(0, 20);
        const part2 = fullJson.slice(20, 40);
        const part3 = fullJson.slice(40);
        
        // Setup pending request mock to accept the response
        vi.mocked(requestTracker.getPendingRequest).mockReturnValue({
            action: 'test_action',
            resolve: vi.fn(),
            reject: vi.fn(),
            timeout: setTimeout(() => {}, 0),
            requestedAt: new Date(),
            payload: {}
        } as PendingRequest);

        // Send chunks
        messageHandler.handleMessage({
            type: 'automation_response_chunk',
            requestId,
            chunkIndex: 0,
            totalChunks: 3,
            chunkData: part1
        } as AutomationBridgeResponseChunk);

        messageHandler.handleMessage({
            type: 'automation_response_chunk',
            requestId,
            chunkIndex: 1,
            totalChunks: 3,
            chunkData: part2
        } as AutomationBridgeResponseChunk);

        messageHandler.handleMessage({
            type: 'automation_response_chunk',
            requestId,
            chunkIndex: 2,
            totalChunks: 3,
            chunkData: part3
        } as AutomationBridgeResponseChunk);

        // Verify resolution
        expect(requestTracker.resolveRequest).toHaveBeenCalledWith(requestId, expect.objectContaining({
            requestId: requestId,
            success: true,
            result: { data: 'Long data' }
        }));
    });

    it('should handle out-of-order chunks', () => {
         const requestId = 'req-456';
         const originalResponse = { type: 'automation_response', requestId, success: true };
         const fullJson = JSON.stringify(originalResponse);
         
         const part1 = fullJson.slice(0, 10);
         const part2 = fullJson.slice(10);
         
         vi.mocked(requestTracker.getPendingRequest).mockReturnValue({
             action: 'test',
             resolve: vi.fn(),
             reject: vi.fn(),
             timeout: setTimeout(() => {}, 0),
             requestedAt: new Date(),
             payload: {}
         } as PendingRequest);

         // Send chunk 1 (index 1) first
         messageHandler.handleMessage({
             type: 'automation_response_chunk',
             requestId,
             chunkIndex: 1,
             totalChunks: 2,
             chunkData: part2
         } as AutomationBridgeResponseChunk);
         
         // Should not resolve yet
         expect(requestTracker.resolveRequest).not.toHaveBeenCalled();
         
         // Send chunk 0
         messageHandler.handleMessage({
             type: 'automation_response_chunk',
             requestId,
             chunkIndex: 0,
             totalChunks: 2,
             chunkData: part1
         } as AutomationBridgeResponseChunk);
         
         // Should resolve now
         expect(requestTracker.resolveRequest).toHaveBeenCalledWith(requestId, expect.objectContaining({ success: true }));
    });
});
