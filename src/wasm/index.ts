/**
 * WebAssembly Integration Module
 *
 * This module provides a high-level interface to WebAssembly-optimized
 * operations for performance-critical tasks in the MCP server.
 *
 * Features:
 * - Property parsing with fallback to TypeScript
 * - Transform calculations (vector/matrix math)
 * - Asset dependency resolution
 * - Performance monitoring
 */

import { Logger } from '../utils/logger.js';

// WASM module interface based on wasm-pack generated exports
interface WASMModule {
  default: () => Promise<void>;
  PropertyParser?: new () => {
    parse_properties: (json: string, maxDepth: number) => unknown;
  };
  TransformCalculator?: new () => {
    composeTransform: (loc: Float32Array, rot: Float32Array, scale: Float32Array) => Float32Array;
    decomposeMatrix: (matrix: Float32Array) => Float32Array;
  };
  Vector?: new (x: number, y: number, z: number) => {
    x: number;
    y: number;
    z: number;
    add: (other: { x: number; y: number; z: number }) => { x: number; y: number; z: number };
  };
  DependencyResolver?: new () => {
    analyzeDependencies: (assetPath: string, dependencies: string, maxDepth: number) => unknown;
    calculateDepth: (assetPath: string, dependencies: string, maxDepth: number) => unknown;
    findCircularDependencies: (dependencies: string, maxDepth: number) => unknown;
    topologicalSort: (dependencies: string) => unknown;
  };
  Utils?: unknown;
}

// Type for globalThis with optional fetch/Request (Node may not have them)
type GlobalThisWithFetch = typeof globalThis & {
  fetch?: typeof fetch;
  Request?: typeof Request;
};

interface WASMConfig {
  enabled?: boolean;
  wasmPath?: string;
  fallbackEnabled?: boolean;
  performanceMonitoring?: boolean;
}

interface PerformanceMetrics {
  operation: string;
  duration: number;
  timestamp: number;
  useWASM: boolean;
}

const isNodeEnvironment =
  typeof process !== 'undefined' &&
  process.versions != null &&
  process.versions.node != null;

let nodeFileFetchPatched = false;

async function ensureNodeFileFetchForWasm(): Promise<void> {
  if (!isNodeEnvironment || nodeFileFetchPatched) {
    return;
  }

  const g = globalThis as GlobalThisWithFetch;
  const originalFetch = g.fetch;

  if (typeof originalFetch !== 'function') {
    return;
  }

  try {
    const fs = await import('node:fs/promises');
    const url = await import('node:url');
    
    const readFile = fs.readFile;
    const fileURLToPath = url.fileURLToPath;

    const toUrl = (input: unknown): URL | null => {
      try {
        if (input instanceof URL) {
          return input;
        }
        // Node fetch often receives a string URL
        if (typeof input === 'string') {
          return new URL(input);
        }
        // Handle Request objects if available
        const RequestCtor = g.Request;
        const inputObj = input as Record<string, unknown>;
        if (RequestCtor && input instanceof RequestCtor && typeof inputObj.url === 'string') {
          return new URL(inputObj.url);
        }
      } catch {
        // Ignore parse errors and fall through
      }
      return null;
    };

    // Custom fetch that handles file:// URLs for WASM loading.
    // Returns ArrayBuffer for file:// (non-standard) or delegates to original fetch.
    const patchedFetch = async (input: RequestInfo | URL, init?: RequestInit): Promise<Response | ArrayBuffer> => {
      try {
        const target = toUrl(input);
        if (target && target.protocol === 'file:') {
          const filePath = fileURLToPath(target);
          const data = await readFile(filePath);

          // Return a raw ArrayBuffer/Uint8Array so wasm-pack's loader can
          // pass it directly to WebAssembly.instantiate as a buffer source.
          const buffer = data.buffer.slice(
            data.byteOffset,
            data.byteOffset + data.byteLength
          );

          return buffer;
        }
      } catch {
        // Fall through to original fetch if anything goes wrong
      }

      return originalFetch(input, init);
    };

    // Assign patched fetch - type assertion needed as we return ArrayBuffer for file:// URLs
    // which is non-standard but required for wasm-pack's loader in Node.js
    g.fetch = patchedFetch as typeof fetch;

    nodeFileFetchPatched = true;
  } catch {
    // If the Node-specific modules are unavailable for some reason,
    // leave fetch as-is and let the normal error handling occur.
  }
}

export class WASMIntegration {
  private log = new Logger('WASMIntegration');
  private module: WASMModule | null = null;
  private config: Required<WASMConfig>;
  private metrics: PerformanceMetrics[] = [];
  private maxMetrics = 1000;
  private initialized = false;
  // Track whether we've already determined that the optional WASM bundle is
  // unavailable (for example, ERR_MODULE_NOT_FOUND). When set, future
  // initialize() calls become no-ops and rely on TypeScript fallbacks.
  private moduleUnavailable = false;

  constructor(config: WASMConfig = {}) {
    const envFlag = process.env.WASM_ENABLED;
    const envEnabled = envFlag === undefined ? true : envFlag === 'true';

    // Resolve a sensible default WASM bundle path that works both when this
    // module is executed from compiled dist/ (Node runs dist/wasm/index.js)
    // and when run directly from src/ (e.g. via ts-node). When running from
    // dist/, the generated bundle lives under src/wasm/pkg by default, so we
    // point back to the source tree. When running from src/, we expect the
    // pkg/ directory to sit alongside index.ts.
    const here = new URL('.', import.meta.url);
    const herePath = here.pathname.replace(/\\/g, '/');
    const defaultUrl = herePath.includes('/dist/')
      ? new URL('../../src/wasm/pkg/unreal_mcp_wasm.js', import.meta.url)
      : new URL('./pkg/unreal_mcp_wasm.js', import.meta.url);
    const defaultWasmPath = defaultUrl.href;

    this.config = {
      enabled: config.enabled ?? envEnabled,
      wasmPath: config.wasmPath ?? process.env.WASM_PATH ?? defaultWasmPath,
      fallbackEnabled: config.fallbackEnabled ?? true,
      performanceMonitoring: config.performanceMonitoring ?? true
    };
  }

  /**
   * Initialize the WebAssembly module
   */
  async initialize(): Promise<void> {
    if (this.initialized && this.module) {
      this.log.debug('WASM module already initialized');
      return;
    }

    // If a prior initialization attempt determined that the module is not
    // available (for example, ERR_MODULE_NOT_FOUND), do not keep trying to
    // load it on every call. The TypeScript fallbacks will remain active.
    if (this.moduleUnavailable) {
      return;
    }

    if (!this.config.enabled) {
      this.log.info('WASM integration is disabled');
      return;
    }

    try {
      this.log.info(`Loading WASM module from ${this.config.wasmPath}...`);

      // When running under Node, wasm-pack's web target will attempt to
      // fetch() the compiled .wasm via a file:// URL. Node's built-in
      // fetch does not currently support file://, so we install a narrow
      // polyfill that handles file URLs by reading from disk and delegates
      // all other requests to the original fetch implementation.
      await ensureNodeFileFetchForWasm();

      // Dynamic import of the WASM module
      const wasmModule = await import(this.config.wasmPath);
      await wasmModule.default();

      this.module = wasmModule;
      this.initialized = true;

      this.log.info('✅ WebAssembly module initialized successfully');

      // Log available functions for debugging
      this.log.debug('WASM module functions:', {
        hasPropertyParser: typeof wasmModule.PropertyParser === 'function',
        hasTransformCalculator: typeof wasmModule.TransformCalculator === 'function',
        hasVector: typeof wasmModule.Vector === 'function',
        hasUtils: typeof wasmModule.Utils === 'function'
      });
    } catch (error: unknown) {
      this.log.error('Failed to initialize WebAssembly module:', error);

      const errorObj = error as Record<string, unknown> | null;
      const code = errorObj?.code;
      if (code === 'ERR_MODULE_NOT_FOUND') {
        // The WASM bundle is an optional optimization. When it has not been
        // built, log a single concise warning and then rely on the
        // TypeScript fallbacks for the rest of the process without
        // repeatedly attempting to import the missing file.
        this.log.warn('WebAssembly module not found. To enable WASM, run "npm run build:wasm". To silence this warning, set WASM_ENABLED=false.');
        this.moduleUnavailable = true;
      }

      this.log.warn('Falling back to TypeScript implementations');

      if (!this.config.fallbackEnabled) {
        throw new Error('WASM initialization failed and fallbacks are disabled');
      }

      this.module = null;
      this.initialized = false;
    }
  }

  /**
   * Check if WASM is initialized and ready
   */
  isReady(): boolean {
    return this.initialized && this.module !== null;
  }

  /**
   * Check if WebAssembly is supported in the environment
   */
  static isSupported(): boolean {
    return typeof WebAssembly === 'object' &&
           typeof WebAssembly.instantiate === 'function';
  }

  /**
   * Parse properties with WASM optimization and TypeScript fallback
   */
  async parseProperties(jsonStr: string, options?: { maxDepth?: number }): Promise<unknown> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    // Try WASM first if available
    if (this.module && typeof this.module.PropertyParser === 'function') {
      try {
        const parser = new this.module.PropertyParser();
        const result = parser.parse_properties(jsonStr, options?.maxDepth ?? 100);

        const duration = performance.now() - start;
        this.recordMetrics('parse_properties', duration, true);

        return result;
      } catch (error: unknown) {
        this.log.warn('WASM property parsing failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('parse_properties', duration, false);

    return this.fallbackParseProperties(jsonStr);
  }

  /**
   * Compose transform (location, rotation, scale) with WASM optimization
   */
  composeTransform(
    location: [number, number, number],
    rotation: [number, number, number],
    scale: [number, number, number]
  ): Float32Array {
    const start = performance.now();

    if (this.isReady() && this.module && typeof this.module.TransformCalculator === 'function') {
      try {
        const calculator = new this.module.TransformCalculator();
        const result = calculator.composeTransform(
          new Float32Array(location),
          new Float32Array(rotation),
          new Float32Array(scale)
        );

        const duration = performance.now() - start;
        this.recordMetrics('compose_transform', duration, true);

        return new Float32Array(result);
      } catch (error: unknown) {
        this.log.warn('WASM transform calculation failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('compose_transform', duration, false);

    return this.fallbackComposeTransform(location, rotation, scale);
  }

  /**
   * Decompose a transformation matrix
   */
  decomposeMatrix(matrix: Float32Array): number[] {
    const start = performance.now();

    if (this.isReady() && this.module && typeof this.module.TransformCalculator === 'function') {
      try {
        const calculator = new this.module.TransformCalculator();
        const result = calculator.decomposeMatrix(matrix);

        const duration = performance.now() - start;
        this.recordMetrics('decompose_matrix', duration, true);

        return Array.from(result);
      } catch (error: unknown) {
        this.log.warn('WASM matrix decomposition failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('decompose_matrix', duration, false);

    return this.fallbackDecomposeMatrix(matrix);
  }

  /**
   * Calculate vector operations with WASM
   */
  vectorAdd(
    v1: [number, number, number],
    v2: [number, number, number]
  ): [number, number, number] {
    const start = performance.now();

    if (this.isReady() && this.module && typeof this.module.Vector === 'function') {
      try {
        const vec1 = new this.module.Vector(v1[0], v1[1], v1[2]);
        const vec2 = new this.module.Vector(v2[0], v2[1], v2[2]);
        const result = vec1.add(vec2);

        const duration = performance.now() - start;
        this.recordMetrics('vector_add', duration, true);

        return [result.x, result.y, result.z];
      } catch (error: unknown) {
        this.log.warn('WASM vector addition failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('vector_add', duration, false);

    return this.fallbackVectorAdd(v1, v2);
  }

  /**
   * Resolve asset dependencies with WASM optimization
   */
  async resolveDependencies(
    assetPath: string,
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): Promise<unknown> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    // Try WASM first if available
    if (this.module && typeof this.module.DependencyResolver === 'function') {
      try {
        const resolver = new this.module.DependencyResolver();
        const dependenciesJson = JSON.stringify(dependencies);
        const result = resolver.analyzeDependencies(
          assetPath,
          dependenciesJson,
          options?.maxDepth ?? 100
        );

        const duration = performance.now() - start;
        this.recordMetrics('resolve_dependencies', duration, true);

        return result;
      } catch (error: unknown) {
        this.log.warn('WASM dependency resolution failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('resolve_dependencies', duration, false);

    return this.fallbackResolveDependencies(assetPath, dependencies, options);
  }

  async calculateDependencyDepth(
    assetPath: string,
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): Promise<number> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    if (this.module && typeof this.module.DependencyResolver === 'function') {
      try {
        const resolver = new this.module.DependencyResolver();
        const dependenciesJson = JSON.stringify(dependencies);
        const result = resolver.calculateDepth(
          assetPath,
          dependenciesJson,
          options?.maxDepth ?? 100
        );

        const duration = performance.now() - start;
        this.recordMetrics('calculate_dependency_depth', duration, true);

        return result as number;
      } catch (error: unknown) {
        this.log.warn('WASM dependency depth calculation failed, falling back to TypeScript:', error);
      }
    }

    const duration = performance.now() - start;
    this.recordMetrics('calculate_dependency_depth', duration, false);

    return this.fallbackCalculateDependencyDepth(assetPath, dependencies, options);
  }

  async findCircularDependencies(
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): Promise<string[][]> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    if (this.module && typeof this.module.DependencyResolver === 'function') {
      try {
        const resolver = new this.module.DependencyResolver();
        const dependenciesJson = JSON.stringify(dependencies);
        const result = resolver.findCircularDependencies(
          dependenciesJson,
          options?.maxDepth ?? 100
        );

        const duration = performance.now() - start;
        this.recordMetrics('find_circular_dependencies', duration, true);

        return result as string[][];
      } catch (error: unknown) {
        this.log.warn('WASM circular dependency detection failed, falling back to TypeScript:', error);
      }
    }

    const duration = performance.now() - start;
    this.recordMetrics('find_circular_dependencies', duration, false);

    return this.fallbackFindCircularDependencies(dependencies, options);
  }

  async topologicalSort(
    dependencies: Record<string, string[]>
  ): Promise<string[]> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    if (this.module && typeof this.module.DependencyResolver === 'function') {
      try {
        const resolver = new this.module.DependencyResolver();
        const dependenciesJson = JSON.stringify(dependencies);
        const result = resolver.topologicalSort(dependenciesJson);

        const duration = performance.now() - start;
        this.recordMetrics('topological_sort', duration, true);

        return result as string[];
      } catch (error: unknown) {
        this.log.warn('WASM topological sort failed, falling back to TypeScript:', error);
      }
    }

    const duration = performance.now() - start;
    this.recordMetrics('topological_sort', duration, false);

    return this.fallbackTopologicalSort(dependencies);
  }

  /**
   * Get performance metrics for WASM operations
   */
  getMetrics(): {
    totalOperations: number;
    wasmOperations: number;
    tsOperations: number;
    averageTime: number;
    operations: PerformanceMetrics[];
  } {
    const operations = this.metrics.slice(-this.maxMetrics);
    const wasmOperations = operations.filter(m => m.useWASM).length;
    const tsOperations = operations.filter(m => !m.useWASM).length;
    const totalTime = operations.reduce((sum, m) => sum + m.duration, 0);
    const averageTime = operations.length > 0 ? totalTime / operations.length : 0;

    return {
      totalOperations: operations.length,
      wasmOperations,
      tsOperations,
      averageTime,
      operations
    };
  }

  /**
   * Lightweight status information for observability/metrics.
   */
  getStatus(): {
    enabled: boolean;
    ready: boolean;
    moduleUnavailable: boolean;
  } {
    return {
      enabled: this.config.enabled,
      ready: this.isReady(),
      moduleUnavailable: this.moduleUnavailable
    };
  }

  /**
   * Clear performance metrics
   */
  clearMetrics(): void {
    this.metrics = [];
  }

  /**
   * Report performance summary
   */
  reportPerformance(): string {
    const metrics = this.getMetrics();
    const wasmPercentage = metrics.totalOperations > 0
      ? ((metrics.wasmOperations / metrics.totalOperations) * 100).toFixed(1)
      : '0';

    const report = [
      '=== WASM Performance Report ===',
      `Total Operations: ${metrics.totalOperations}`,
      `WASM Operations: ${metrics.wasmOperations} (${wasmPercentage}%)`,
      `TypeScript Operations: ${metrics.tsOperations}`,
      `Average Time: ${metrics.averageTime.toFixed(2)}ms`,
      '=============================='
    ].join('\n');

    return report;
  }

  private recordMetrics(operation: string, duration: number, useWASM: boolean): void {
    if (!this.config.performanceMonitoring) {
      return;
    }

    this.metrics.push({
      operation,
      duration,
      timestamp: Date.now(),
      useWASM
    });

    // Keep only the last maxMetrics entries
    if (this.metrics.length > this.maxMetrics) {
      this.metrics.shift();
    }
  }

  // TypeScript fallback implementations

  private fallbackParseProperties(jsonStr: string): unknown {
    try {
      return JSON.parse(jsonStr);
    } catch (error: unknown) {
      this.log.error('Failed to parse JSON:', error);
      throw error;
    }
  }

  private fallbackComposeTransform(
    location: [number, number, number],
    rotation: [number, number, number],
    scale: [number, number, number]
  ): Float32Array {
    // Simplified transformation matrix composition
    const matrix = new Float32Array(16);

    const [x, y, z] = location;
    const [_pitch, _yaw, _roll] = rotation.map(angle => angle * Math.PI / 180);
    const [sx, sy, sz] = scale;

    // Create transformation matrix (simplified)
    matrix[0] = sx;
    matrix[5] = sy;
    matrix[10] = sz;
    matrix[12] = x;
    matrix[13] = y;
    matrix[14] = z;
    matrix[15] = 1;

    return matrix;
  }

  private fallbackDecomposeMatrix(matrix: Float32Array): number[] {
    const m0 = matrix[0] ?? 0, m1 = matrix[1] ?? 0, m2 = matrix[2] ?? 0;
    const m4 = matrix[4] ?? 0, m5 = matrix[5] ?? 0, m6 = matrix[6] ?? 0;
    const m8 = matrix[8] ?? 0, m9 = matrix[9] ?? 0, m10 = matrix[10] ?? 0;
    const m12 = matrix[12] ?? 0, m13 = matrix[13] ?? 0, m14 = matrix[14] ?? 0;
    
    const location = [m12, m13, m14];
    const scale = [
      Math.sqrt(m0 * m0 + m1 * m1 + m2 * m2),
      Math.sqrt(m4 * m4 + m5 * m5 + m6 * m6),
      Math.sqrt(m8 * m8 + m9 * m9 + m10 * m10)
    ];

    const rotation = [0, 0, 0]; // Simplified

    return [...location, ...rotation, ...scale];
  }

  private fallbackVectorAdd(
    v1: [number, number, number],
    v2: [number, number, number]
  ): [number, number, number] {
    return [
      v1[0] + v2[0],
      v1[1] + v2[1],
      v1[2] + v2[2]
    ];
  }

  private fallbackResolveDependencies(
    assetPath: string,
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): Record<string, unknown> {
    const maxDepth = options?.maxDepth ?? 100;
    const visited = new Set<string>();
    const result: Array<{ path: string; dependencies: string[]; depth: number }> = [];
    const queue: Array<{ path: string; depth: number }> = [
      { path: assetPath, depth: 0 }
    ];

    while (queue.length > 0) {
      const item = queue.shift();
      if (!item) continue;

      const { path, depth } = item;

      if (depth > maxDepth || visited.has(path)) {
        continue;
      }

      visited.add(path);

      if (dependencies[path]) {
        result.push({
          path,
          dependencies: dependencies[path],
          depth
        });

        for (const dep of dependencies[path]) {
          queue.push({ path: dep, depth: depth + 1 });
        }
      }
    }

    const totalCount = result.length;

    return {
      asset: assetPath,
      dependencies: result,
      total_dependency_count: totalCount,
      max_depth: maxDepth,
      analysis_time_ms: 0,
      totalDependencyCount: totalCount,
      maxDepth,
      analysisTimeMs: 0,
      circular_dependencies: [],
      circularDependencies: []
    };
  }

  private fallbackCalculateDependencyDepth(
    assetPath: string,
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): number {
    const maxDepth = options?.maxDepth ?? 100;
    const visited = new Set<string>();

    const dfs = (path: string, depthRemaining: number): number => {
      if (visited.has(path) || depthRemaining === 0) {
        return 0;
      }

      visited.add(path);
      const deps = dependencies[path] || [];
      if (!deps.length) {
        visited.delete(path);
        return 0;
      }

      let maxChildDepth = 0;
      for (const dep of deps) {
        const childDepth = dfs(dep, depthRemaining - 1);
        if (childDepth > maxChildDepth) {
          maxChildDepth = childDepth;
        }
      }

      visited.delete(path);
      return maxChildDepth + 1;
    };

    return dfs(assetPath, maxDepth);
  }

  private fallbackFindCircularDependencies(
    dependencies: Record<string, string[]>,
    options?: { maxDepth?: number }
  ): string[][] {
    const maxDepth = options?.maxDepth ?? 100;
    const cycles: string[][] = [];
    const visited = new Set<string>();
    const stack = new Set<string>();

    const dfs = (current: string, depth: number, path: string[]): void => {
      if (depth > maxDepth) {
        return;
      }

      visited.add(current);
      stack.add(current);
      path.push(current);

      const deps = dependencies[current] || [];
      for (const dep of deps) {
        if (!visited.has(dep)) {
          dfs(dep, depth + 1, path);
        } else if (stack.has(dep)) {
          const startIndex = path.indexOf(dep);
          if (startIndex !== -1) {
            cycles.push(path.slice(startIndex));
          }
        }
      }

      stack.delete(current);
      path.pop();
    };

    for (const asset of Object.keys(dependencies)) {
      if (!visited.has(asset)) {
        dfs(asset, 0, []);
      }
    }

    return cycles;
  }

  private fallbackTopologicalSort(
    dependencies: Record<string, string[]>
  ): string[] {
    const inDegree = new Map<string, number>();
    const graph = new Map<string, string[]>();

    for (const [asset, deps] of Object.entries(dependencies)) {
      if (!inDegree.has(asset)) {
        inDegree.set(asset, 0);
      }
      for (const dep of deps) {
        if (!inDegree.has(dep)) {
          inDegree.set(dep, 0);
        }
        const list = graph.get(dep) || [];
        list.push(asset);
        graph.set(dep, list);
      }
    }

    const queue: string[] = [];
    for (const [asset, degree] of inDegree.entries()) {
      if (degree === 0) {
        queue.push(asset);
      }
    }

    const sorted: string[] = [];

    while (queue.length > 0) {
      const asset = queue.shift() as string;
      sorted.push(asset);

      const dependents = graph.get(asset) || [];
      for (const dependent of dependents) {
        const current = inDegree.get(dependent) ?? 0;
        const next = current - 1;
        inDegree.set(dependent, next);
        if (next === 0) {
          queue.push(dependent);
        }
      }
    }

    return sorted;
  }
}

// Create a singleton instance
export const wasmIntegration = new WASMIntegration();

// Export initialization function
export async function initializeWASM(config?: WASMConfig): Promise<void> {
  const integration = config ? new WASMIntegration(config) : wasmIntegration;
  await integration.initialize();
}

// Export utility functions
export function isWASMReady(): boolean {
  return wasmIntegration.isReady();
}

export function getWASMPerformanceReport(): string {
  return wasmIntegration.reportPerformance();
}
