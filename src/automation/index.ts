export { AutomationBridge } from './bridge.js';
export { McpRequestCancelledError } from './request-cancellation-error.js';
export {
    runWithMcpRequestContext,
    getMcpRequestContext,
    canonicalizeMcpRequestId
} from './request-context.js';
export type { McpRequestContext } from './request-context.js';
export type {
    AutomationBridgeOptions,
    AutomationBridgeMessage,
    AutomationBridgeAutomationEvent,
    AutomationBridgeResponseMessage,
    CancelRequestMessage,
    PendingRequestDetail,
    AutomationBridgeConnectionInfo,
    AutomationBridgeConnectedEvent,
    AutomationBridgeDisconnectedEvent,
    AutomationBridgePortError,
    ProgressUpdateMessage,
    AutomationBridgeStatus,
    PendingRequest,
    QueuedRequestItem,
    SocketInfo,
    AutomationBridgeEvents
} from './types.js';
