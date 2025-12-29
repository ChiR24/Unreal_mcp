import WebSocket from 'ws';

async function doHandshake(port = 8090, capabilityToken) {
  return new Promise((resolve, reject) => {
    const url = `ws://127.0.0.1:${port}`;
    console.log(`Connecting to ${url} with protocol 'mcp-automation'...`);
    
    // Correctly pass subprotocol as 2nd arg, options as 3rd
    const socket = new WebSocket(url, 'mcp-automation', { 
      headers: capabilityToken ? { 'X-MCP-Capability': capabilityToken } : undefined 
    });

    const timeout = setTimeout(() => {
      try { socket.terminate(); } catch {};
      reject(new Error('Handshake timed out (5000ms)'));
    }, 5000);

    socket.on('open', () => {
      console.log('Socket open. Sending bridge_hello...');
      // Send hello
      socket.send(JSON.stringify({ type: 'bridge_hello', capabilityToken: capabilityToken }));
    });

    socket.on('message', (data) => {
      // clearTimeout(timeout); // Don't clear yet, wait for ACK
      try {
        const msgStr = typeof data === 'string' ? data : data.toString('utf8');
        // Sanitize log output to prevent log injection (CWE-117)
        const safeMsg = msgStr.substring(0, 200).replace(/[\r\n]/g, '');
        console.log('Received:', safeMsg);
        const parsed = JSON.parse(msgStr);
        
        if (parsed.type === 'bridge_ack') {
          clearTimeout(timeout);
          socket.close();
          resolve({ success: true, ack: parsed });
        } else if (parsed.type === 'bridge_error') {
          clearTimeout(timeout);
          socket.close();
          resolve({ success: false, error: parsed.error });
        } else {
          // Might be other messages? Ignore or log.
          // Sanitize parsed.type to prevent log injection (CWE-117)
          const safeType = typeof parsed.type === 'string' 
            ? parsed.type.replace(/[\r\n]/g, '') 
            : '[invalid]';
          console.log('Ignoring non-ack message:', safeType);
        }
      } catch (err) {
        clearTimeout(timeout);
        socket.close();
        reject(err);
      }
    });

    socket.on('error', (err) => {
      clearTimeout(timeout);
      console.error('Socket error:', err.message);
      reject(err);
    });
    
    socket.on('close', (code, reason) => {
        console.log(`Socket closed: ${code} - ${reason}`);
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

// Execute the test
run();
