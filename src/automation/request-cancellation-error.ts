/**
 * Typed error used to reject locally-pending automation bridge promises when an
 * MCP request is cancelled. It is distinct from a timeout or transport failure
 * so callers (and the gateway error path) can attribute the rejection to an
 * explicit client cancellation.
 */
export class McpRequestCancelledError extends Error {
    /** Stable machine-readable code for cancellation. */
    public readonly code = 'MCP_REQUEST_CANCELLED';
    /** Marks this error as a cancellation so it cannot be mistaken for a failure. */
    public readonly cancelled = true;

    constructor(
        message: string,
        public readonly reason?: string,
    ) {
        super(message);
        this.name = 'McpRequestCancelledError';
    }
}
