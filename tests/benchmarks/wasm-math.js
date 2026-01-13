import { WASMIntegration } from '../../dist/wasm/index.js';
import { performance } from 'perf_hooks';
import path from 'path';
import { fileURLToPath, pathToFileURL } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function runBenchmark() {
    console.log('--- WASM Math Benchmark (JS) ---');

    // 1. Setup Data
    const vertexCount = 100000; // 100k vertices
    const vertices = new Float32Array(vertexCount * 3);
    for (let i = 0; i < vertices.length; i++) {
        vertices[i] = Math.random() * 1000 - 500;
    }

    // 2. Setup Integrations
    // Point to the WASM pkg
    const absolutePath = path.resolve(__dirname, '../../src/wasm/pkg/unreal_mcp_wasm.js');
    const wasmPath = pathToFileURL(absolutePath).href;
    
    console.log(`Loading WASM from: ${wasmPath}`);

    const wasm = new WASMIntegration({ enabled: true, wasmPath });
    const ts = new WASMIntegration({ enabled: false });

    await wasm.initialize();
    await ts.initialize();

    console.log(`WASM Ready: ${wasm.isReady()}`);

    // 3. Measure Bounds Calculation
    const iterations = 50;
    
    // Warmup
    wasm.calculateMeshBounds(vertices);
    ts.calculateMeshBounds(vertices);

    // TS Run
    let start = performance.now();
    for (let i = 0; i < iterations; i++) {
        ts.calculateMeshBounds(vertices);
    }
    const tsTime = performance.now() - start;
    console.log(`TS Total: ${tsTime.toFixed(2)}ms, Avg: ${(tsTime/iterations).toFixed(4)}ms`);

    // WASM Run
    start = performance.now();
    for (let i = 0; i < iterations; i++) {
        wasm.calculateMeshBounds(vertices);
    }
    const wasmTime = performance.now() - start;
    console.log(`WASM Total: ${wasmTime.toFixed(2)}ms, Avg: ${(wasmTime/iterations).toFixed(4)}ms`);

    const speedup = tsTime / wasmTime;
    console.log(`Speedup: ${speedup.toFixed(2)}x`);
}

runBenchmark().catch(console.error);
