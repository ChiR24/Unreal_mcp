import { WASMIntegration } from '../../src/wasm/index.js';
import { performance } from 'perf_hooks';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function runBenchmark() {
    console.log('--- WASM Math Benchmark ---');

    // 1. Setup Data
    const vertexCount = 100000; // 100k vertices
    const vertices = new Float32Array(vertexCount * 3);
    for (let i = 0; i < vertices.length; i++) {
        vertices[i] = Math.random() * 1000 - 500;
    }

    // 2. Setup Integrations
    // We need to point to the built WASM pkg
    // Assuming running from root: src/wasm/pkg/unreal_mcp_wasm.js
    // From tests/benchmarks/: ../../src/wasm/pkg/unreal_mcp_wasm.js
    const wasmPath = path.resolve(__dirname, '../../src/wasm/pkg/unreal_mcp_wasm.js');
    
    console.log(`Loading WASM from: ${wasmPath}`);

    const wasm = new WASMIntegration({ enabled: true, wasmPath });
    const ts = new WASMIntegration({ enabled: false });

    await wasm.initialize();
    await ts.initialize();

    console.log(`WASM Ready: ${wasm.isReady()}`);
    // TS instance won't be "ready" in terms of module loaded, but works for fallback

    // 3. Measure Bounds Calculation
    const iterations = 100;
    
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
