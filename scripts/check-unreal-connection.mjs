#!/usr/bin/env node
import net from 'node:net';

const HOST = process.env.MCP_AUTOMATION_WS_HOST || '127.0.0.1';
const PORT = process.env.MCP_AUTOMATION_WS_PORT ? parseInt(process.env.MCP_AUTOMATION_WS_PORT) : 8091;
console.log(`Checking connection to Unreal Engine on port ${PORT}...`);

function check(host, port, timeoutMs = 2000) {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    const timer = setTimeout(() => { try { socket.destroy(); } catch {} resolve(false); }, timeoutMs);
    socket.once('error', () => { clearTimeout(timer); resolve(false); });
    socket.connect(port, host, () => { clearTimeout(timer); try { socket.destroy(); } catch {} resolve(true); });
  });
}

const ok = await check(HOST, PORT);
console.log(ok ? `OK: ${HOST}:${PORT} accepting TCP` : `DOWN: ${HOST}:${PORT} not reachable`);
process.exit(ok ? 0 : 1);
