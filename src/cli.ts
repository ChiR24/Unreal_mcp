#!/usr/bin/env node
import { runDoctor } from './proxy/doctor.js';
import { runInit } from './proxy/init.js';
import { startNativeProxy } from './proxy/native-proxy.js';

// The stdio MCP server: the restart-proof front for the plugin's native /mcp endpoint. A `proxy` argument, which
// older configs pass, changes nothing. `doctor` and `init` are one-shot commands for a person at a terminal.
const [command, ...rest] = process.argv.slice(2);
if (command === 'doctor') {
  process.exitCode = await runDoctor();
} else if (command === 'init') {
  const result = runInit(rest);
  process.stdout.write(`${result.content ? `${result.file}\n\n${result.content}\n` : ''}${result.message}\n`);
  process.exitCode = result.ok ? 0 : 1;
} else {
  startNativeProxy();
  process.stdin.once('end', () => process.exit(0));
}
