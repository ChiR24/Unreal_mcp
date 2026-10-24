#!/usr/bin/env node
import { startNativeProxy } from './proxy/native-proxy.js';

// The stdio MCP server: the restart-proof front for the plugin's native /mcp endpoint. A `proxy` argument, which
// older configs pass, changes nothing.
startNativeProxy();
process.stdin.once('end', () => process.exit(0));
