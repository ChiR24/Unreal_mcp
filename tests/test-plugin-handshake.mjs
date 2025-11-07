import WebSocket from 'ws';
import { promisify } from 'node:util';

async function doHandshake(port = 8090, capabilityToken) {
  return new Promise((resolve, reject) => {
    const url = `ws://127.0.0.1:${port}`;
    const socket = new WebSocket(url, { headers: capabilityToken ? { 'X-MCP-Capability': capabilityToken } : undefined });

    const timeout = setTimeout(() => {
      try { socket.terminate(); } catch {};
      reject(new Error('Handshake timed out'));
    }, 5000);

    socket.on('open', () => {
      // Send hello
      socket.send(JSON.stringify({ type: 'bridge_hello', capabilityToken: capabilityToken }));
    });

    socket.on('message', (data) => {
      clearTimeout(timeout);
      try {
        const parsed = JSON.parse(typeof data === 'string' ? data : data.toString('utf8'));
        if (parsed.type === 'bridge_ack') {
          socket.close();
          resolve({ success: true, ack: parsed });
        } else if (parsed.type === 'bridge_error') {
          socket.close();
          resolve({ success: false, error: parsed.error });
        } else {
          // Unexpected
          socket.close();
          resolve({ success: false, error: 'unexpected-response', payload: parsed });
        }
      } catch (err) {
        socket.close();
        reject(err);
      }
    });

    socket.on('error', (err) => {
      clearTimeout(timeout);
      reject(err);
    });
  });
}

async function run() {
  console.log('Test: plugin handshake on default port 8090');
  try {
    const resp = await doHandshake(8090);
    if (resp && resp.success) {
      console.log('✅ Handshake succeeded; plugin listening and returned ack.');
      console.log('ack:', JSON.stringify(resp.ack).substring(0, 400));
      process.exit(0);
    } else {
      console.error('❌ Handshake did not succeed:', resp);
      process.exit(2);
    }
  } catch (err) {
    console.error('❌ Handshake test failed:', err);
    process.exit(1);
  }
}

// When run directly as a script, execute the test
if (import.meta.url === `file://${process.argv[1]}` || import.meta.url === `file://${process.argv[1].replace(/\\/g, '/')}`) {
  run();
}
