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

// Dynamic import for WASM module
type WASMModule = any;

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

export class WASMIntegration {
  private log = new Logger('WASMIntegration');
  private module: WASMModule | null = null;
  private config: Required<WASMConfig>;
  private metrics: PerformanceMetrics[] = [];
  private maxMetrics = 1000;
  private initialized = false;

  constructor(config: WASMConfig = {}) {
    this.config = {
      enabled: config.enabled ?? process.env.WASM_ENABLED === 'true',
      wasmPath: config.wasmPath ?? process.env.WASM_PATH ?? './pkg/unreal_mcp_wasm.js',
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

    if (!this.config.enabled) {
      this.log.info('WASM integration is disabled');
      return;
    }

    try {
      this.log.info(`Loading WASM module from ${this.config.wasmPath}...`);

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
    } catch (error) {
      this.log.error('Failed to initialize WebAssembly module:', error);
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
  async parseProperties(jsonStr: string, options?: { maxDepth?: number }): Promise<any> {
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
      } catch (error) {
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
        const result = calculator.compose_transform(location, rotation, scale);

        const duration = performance.now() - start;
        this.recordMetrics('compose_transform', duration, true);

        return new Float32Array(result);
      } catch (error) {
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
        const result = calculator.decompose_matrix(matrix);

        const duration = performance.now() - start;
        this.recordMetrics('decompose_matrix', duration, true);

        return Array.from(result);
      } catch (error) {
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
      } catch (error) {
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
  ): Promise<any> {
    const start = performance.now();

    if (!this.isReady()) {
      await this.initialize();
    }

    // Try WASM first if available
    if (this.module && typeof this.module.DependencyResolver === 'function') {
      try {
        const resolver = new this.module.DependencyResolver();
        const dependenciesJson = JSON.stringify(dependencies);
        const result = resolver.analyze_dependencies(
          assetPath,
          dependenciesJson,
          options?.maxDepth ?? 100
        );

        const duration = performance.now() - start;
        this.recordMetrics('resolve_dependencies', duration, true);

        return result;
      } catch (error) {
        this.log.warn('WASM dependency resolution failed, falling back to TypeScript:', error);
      }
    }

    // Fallback to TypeScript
    const duration = performance.now() - start;
    this.recordMetrics('resolve_dependencies', duration, false);

    return this.fallbackResolveDependencies(assetPath, dependencies, options);
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

  private fallbackParseProperties(jsonStr: string): any {
    try {
      return JSON.parse(jsonStr);
    } catch (error) {
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
    const location = [matrix[12], matrix[13], matrix[14]];
    const scale = [
      Math.sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] + matrix[2] * matrix[2]),
      Math.sqrt(matrix[4] * matrix[4] + matrix[5] * matrix[5] + matrix[6] * matrix[6]),
      Math.sqrt(matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10])
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
  ): any {
    const maxDepth = options?.maxDepth ?? 100;
    const visited = new Set<string>();
    const result: any[] = [];
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

    return {
      asset: assetPath,
      dependencies: result,
      total_dependency_count: result.length,
      max_depth: maxDepth,
      analysis_time_ms: 0
    };
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
