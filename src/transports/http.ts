/**
 * StreamableHTTP Transport (Experimental)
 * 
 * This module provides experimental support for StreamableHTTP transport.
 * The standard transport is stdio (default). HTTP transport is for future
 * MCP SDK versions that support StreamableHTTPServerTransport.
 * 
 * @module transports/http
 */

import { Logger } from '../utils/logger.js';

const log = new Logger('HTTPTransport');

/**
 * Configuration options for HTTP transport
 */
export interface HTTPTransportOptions {
  /** Port to listen on */
  port: number;
  /** Host to bind to (default: 127.0.0.1) */
  host?: string;
  /** Enable CORS (default: false) */
  cors?: boolean;
}

/**
 * Check if HTTP transport mode is enabled via environment variable
 */
export const HTTP_TRANSPORT_ENABLED = process.env.TRANSPORT_MODE === 'http';

/**
 * Default HTTP transport options
 */
export const DEFAULT_HTTP_OPTIONS: HTTPTransportOptions = {
  port: parseInt(process.env.HTTP_TRANSPORT_PORT ?? '3000', 10),
  host: process.env.HTTP_TRANSPORT_HOST ?? '127.0.0.1',
  cors: process.env.HTTP_TRANSPORT_CORS === 'true'
};

/**
 * Log experimental warning when HTTP transport is enabled
 */
export function logExperimentalWarning(): void {
  log.warn('========================================');
  log.warn('EXPERIMENTAL: StreamableHTTP transport');
  log.warn('This transport mode is experimental and');
  log.warn('may not be fully supported by the MCP SDK.');
  log.warn('Use stdio transport for production.');
  log.warn('========================================');
}

/**
 * Placeholder for future StreamableHTTP implementation.
 * 
 * When the MCP SDK adds StreamableHTTPServerTransport support,
 * this function will create and return the transport instance.
 * 
 * @param _options - HTTP transport configuration
 * @returns null (placeholder - actual implementation pending SDK support)
 */
export function createHTTPTransport(_options: HTTPTransportOptions = DEFAULT_HTTP_OPTIONS): null {
  // Note: Full implementation requires MCP SDK support for StreamableHTTPServerTransport
  // This is a placeholder for future SDK versions.
  // 
  // Future implementation would look like:
  // import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamablehttp.js';
  // return new StreamableHTTPServerTransport({ port: options.port, host: options.host });
  
  log.info('HTTP transport requested but not yet implemented in MCP SDK');
  log.info('Falling back to stdio transport');
  
  return null;
}
