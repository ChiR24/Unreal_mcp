import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import net from 'node:net';
import { performance } from 'node:perf_hooks';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');

export class BenchmarkRunner {
  constructor(suiteName) {
    this.suiteName = suiteName || 'Benchmark Suite';
    this.benchmarks = [];
  }

  addBenchmark(name, fn, iterations = 10) {
    this.benchmarks.push({ name, fn, iterations });
  }

  async run() {
    console.log('\n' + '='.repeat(60));
    console.log(`🚀 ${this.suiteName}`);
    console.log('='.repeat(60));
    console.log(`Total benchmarks: ${this.benchmarks.length}`);
    console.log('');

    let transport;
    let client;
    const results = {};

    try {
      const bridgeHost = process.env.MCP_AUTOMATION_WS_HOST ?? '127.0.0.1';
      const envPorts = process.env.MCP_AUTOMATION_WS_PORTS
        ? process.env.MCP_AUTOMATION_WS_PORTS.split(',').map((p) => parseInt(p.trim(), 10)).filter(Boolean)
        : [8090, 8091];
      const waitMs = parseInt(process.env.UNREAL_MCP_WAIT_PORT_MS ?? '5000', 10);

      async function waitForAnyPort(host, ports, timeoutMs = 10000) {
        const start = Date.now();
        while (Date.now() - start < timeoutMs) {
          for (const port of ports) {
            try {
              await new Promise((resolve, reject) => {
                const sock = new net.Socket();
                let settled = false;
                sock.setTimeout(1000);
                sock.once('connect', () => { settled = true; sock.destroy(); resolve(true); });
                sock.once('timeout', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('timeout')); } });
                sock.once('error', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('error')); } });
                sock.connect(port, host);
              });
              console.log(`✅ Automation bridge appears to be listening on ${host}:${port}`);
              return port;
            } catch { }
          }
          await new Promise((r) => setImmediate(r));
        }
        throw new Error(`Timed out waiting for automation bridge on ports: ${ports.join(',')}`);
      }

      try {
        await waitForAnyPort(bridgeHost, envPorts, waitMs);
      } catch (err) {
        console.warn('Automation bridge did not become available:', err.message);
      }

      let serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
      let serverArgs = process.env.UNREAL_MCP_SERVER_ARGS ? process.env.UNREAL_MCP_SERVER_ARGS.split(',') : [path.join(repoRoot, 'dist', 'cli.js')];
      const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;
      const serverEnv = Object.assign({}, process.env);

      const distPath = path.join(repoRoot, 'dist', 'cli.js');
      let useDist = false;
      try {
        await fs.access(distPath);
        useDist = true;
      } catch {
        useDist = false;
      }

      if (!useDist) {
        serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'npx';
        serverArgs = ['ts-node-esm', path.join(repoRoot, 'src', 'cli.ts')];
      }

      transport = new StdioClientTransport({
        command: serverCommand,
        args: serverArgs,
        cwd: serverCwd,
        stderr: 'inherit',
        env: serverEnv
      });

      client = new Client({
        name: 'unreal-mcp-benchmark-runner',
        version: '1.0.0'
      });

      await client.connect(transport);
      console.log('✅ Connected to Unreal MCP Server\n');

      const tools = {
        async executeTool(toolName, args) {
          return await client.callTool({ name: toolName, arguments: args });
        }
      };

      for (const bench of this.benchmarks) {
        console.log(`Running: ${bench.name} (${bench.iterations} iterations)`);
        const timings = [];
        let errors = 0;

        try {
          await bench.fn(tools);
        } catch (e) {
          console.warn(`  Warmup failed: ${e.message}`);
        }

        for (let i = 0; i < bench.iterations; i++) {
          const start = performance.now();
          try {
            await bench.fn(tools);
            const end = performance.now();
            timings.push(end - start);
            process.stdout.write('.');
          } catch (e) {
            process.stdout.write('x');
            errors++;
          }
        }
        console.log('');

        if (timings.length > 0) {
          const min = Math.min(...timings);
          const max = Math.max(...timings);
          const avg = timings.reduce((a, b) => a + b, 0) / timings.length;
          const sorted = [...timings].sort((a, b) => a - b);
          const p95 = sorted[Math.floor(sorted.length * 0.95)];

          results[bench.name] = {
            min: min.toFixed(2),
            max: max.toFixed(2),
            avg: avg.toFixed(2),
            p95: p95.toFixed(2),
            errors
          };

          console.log(`  Min: ${min.toFixed(2)}ms | Max: ${max.toFixed(2)}ms | Avg: ${avg.toFixed(2)}ms | P95: ${p95.toFixed(2)}ms | Errors: ${errors}`);
        } else {
          console.log('  No successful iterations.');
        }
      }

      return results;

    } catch (error) {
      console.error('Benchmark runner failed:', error);
      process.exit(1);
    } finally {
      if (client) await client.close();
      if (transport) await transport.close();
    }
  }
}
