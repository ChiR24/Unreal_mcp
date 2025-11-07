import { spawn } from 'child_process';
import { exec } from 'child_process';

const server = spawn('node', ['dist/cli.js'], {
  stdio: ['pipe', 'pipe', 'inherit'],
  cwd: process.cwd()
});

let initialized = false;
let gotResponse = false;

server.stdout.on('data', (data) => {
  const text = data.toString();
  console.log('SERVER OUTPUT:', text);

  try {
    const parsed = JSON.parse(text.trim());
    if (parsed.id === 1 && parsed.result && !initialized) {
      initialized = true;
      console.log('Server initialized successfully');

      // Send initialized message
      const initializedMsg = JSON.stringify({
        jsonrpc: '2.0',
        method: 'initialized',
        params: {}
      }) + '\n';
      console.log('Sending initialized message...');
      server.stdin.write(initializedMsg);
    }

    if (parsed.id === 2 && parsed.result) {
      gotResponse = true;
      console.log('Got health response, checking port...');

      exec('netstat -ano | findstr :8090', (err, stdout) => {
        if (stdout && stdout.trim()) {
          console.log('✅ Port 8090 is listening!');
          console.log('Port details:', stdout.trim());
        } else {
          console.log('❌ Port 8090 is not listening');
        }
        server.kill();
        process.exit(0);
      });
    }
  } catch (e) {
    // Not JSON, ignore
  }
});

server.on('exit', (code) => {
  console.log('Server exited with code:', code);
  if (!gotResponse) {
    process.exit(1);
  }
});

server.on('error', (err) => {
  console.error('Failed to start server:', err);
  process.exit(1);
});

// Send initialize message
const initMsg = JSON.stringify({
  jsonrpc: '2.0',
  id: 1,
  method: 'initialize',
  params: {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'test', version: '1.0.0' }
  }
}) + '\n';

console.log('Sending initialize message...');
server.stdin.write(initMsg);

// Send health check after a delay
setTimeout(() => {
  if (initialized && !gotResponse) {
    console.log('Sending health check...');
    const healthMsg = JSON.stringify({
      jsonrpc: '2.0',
      id: 2,
      method: 'resources/read',
      params: { uri: 'ue://health' }
    }) + '\n';
    server.stdin.write(healthMsg);
  }
}, 1000);

// Timeout
setTimeout(() => {
  if (!gotResponse) {
    console.log('Timeout: Server did not respond');
    server.kill();
    process.exit(1);
  }
}, 5000);