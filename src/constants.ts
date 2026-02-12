// Shared runtime defaults and protocol constants
export const DEFAULT_AUTOMATION_HOST = '127.0.0.1';
export const DEFAULT_AUTOMATION_PORT = 8090;
export const DEFAULT_AUTOMATION_PORTS = [8090, 8091];
export const DEFAULT_NEGOTIATED_PROTOCOLS = ['mcp-automation'];
export const DEFAULT_HEARTBEAT_INTERVAL_MS = 10000;
export const DEFAULT_HANDSHAKE_TIMEOUT_MS = 5000;
export const DEFAULT_MAX_PENDING_REQUESTS = 25;
export const DEFAULT_MAX_QUEUED_REQUESTS = 100;
export const DEFAULT_MAX_INBOUND_MESSAGES_PER_MINUTE = 600;
export const DEFAULT_MAX_INBOUND_AUTOMATION_REQUESTS_PER_MINUTE = 120;
export const DEFAULT_TIME_OF_DAY = 9;
export const DEFAULT_SUN_INTENSITY = 10000;
export const DEFAULT_SKYLIGHT_INTENSITY = 1;

export const DEFAULT_SCREENSHOT_RESOLUTION = '1920x1080';

// Operation Timeouts (reduced for faster idle detection)
export const DEFAULT_OPERATION_TIMEOUT_MS = 30000;  // 30s base timeout
export const DEFAULT_ASSET_OP_TIMEOUT_MS = 60000;
export const EXTENDED_ASSET_OP_TIMEOUT_MS = 120000;
export const LONG_RUNNING_OP_TIMEOUT_MS = 300000;

// Command-specific timeouts
export const CONSOLE_COMMAND_TIMEOUT_MS = 30000;
export const ENGINE_QUERY_TIMEOUT_MS = 15000;
export const CONNECTION_TIMEOUT_MS = 15000;

// Progress/Heartbeat Timeout Extension
// When UE sends progress updates, timeout is extended to keep long operations alive
export const PROGRESS_EXTENSION_MS = 30000;          // Extend by 30s on each progress update
export const MAX_PROGRESS_EXTENSIONS = 10;           // Max times timeout can be extended (prevents deadlock)
export const PROGRESS_STALE_THRESHOLD = 3;           // Stale updates before timeout (percent unchanged)
export const ABSOLUTE_MAX_TIMEOUT_MS = 300000;       // 5 minute hard cap (even with extensions)

// Message size limits
export const MAX_WS_MESSAGE_SIZE_BYTES = 5 * 1024 * 1024;

