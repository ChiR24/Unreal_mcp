import { Logger } from '../../utils/logging/logger.js';
import { consolidatedToolDefinitions, type ToolDefinition } from '../catalog/consolidated-tool-definitions.js';
import { countEnabledTools, isToolStateEnabled, listCategoryStates } from './dynamic-tool-queries.js';
import {
  disableCategoryState,
  disableToolStates,
  enableCategoryState,
  enableToolStates,
  resetToolStates
} from './dynamic-tool-state-operations.js';
import type {
  CategoryDisableResult,
  CategoryEnableResult,
  CategoryState,
  DisableToolsResult,
  EnableToolsResult,
  ToolCategory,
  ToolState
} from './dynamic-tool-types.js';

export type { ToolCategory } from './dynamic-tool-types.js';

const log = new Logger('DynamicToolManager');

function logInfoForNames(message: string, names: readonly string[]): void {
  if (names.length > 0) {
    log.info(`${message}: ${names.join(', ')}`);
  }
}

class DynamicToolManager {
  private readonly toolStates = new Map<string, ToolState>();
  private readonly categoryStates = new Map<ToolCategory, CategoryState>();
  private initialized = false;
  private catalogStateRevision = 0;

  initialize(): void {
    if (this.initialized) {
      log.warn('DynamicToolManager already initialized');
      return;
    }

    for (const def of consolidatedToolDefinitions) {
      this.addToolDefinition(def);
    }

    this.initialized = true;
    log.info(`Initialized with ${this.toolStates.size} tools across ${this.categoryStates.size} categories`);
  }

  getEnabledToolDefinitions(): ToolDefinition[] {
    this.ensureInitialized();
    return consolidatedToolDefinitions.filter(def => {
      const state = this.toolStates.get(def.name);
      if (state === undefined) return false;
      return isToolStateEnabled(this.toolStates, this.categoryStates, state.name);
    });
  }

  getAllToolDefinitions(): ToolDefinition[] {
    return consolidatedToolDefinitions;
  }

  listTools(): ToolState[] {
    this.ensureInitialized();
    return Array.from(this.toolStates.values());
  }

  listCategories(): CategoryState[] {
    this.ensureInitialized();
    return listCategoryStates(this.toolStates, this.categoryStates);
  }

  enableTools(toolNames: string[]): EnableToolsResult {
    this.ensureInitialized();
    const result = this.applyMutation(() => enableToolStates(this.toolStates, this.categoryStates, toolNames));

    logInfoForNames('Enabled tools', result.enabled);
    if (result.notFound.length > 0) {
      log.warn(`Tools not found: ${result.notFound.join(', ')}`);
    }

    return result;
  }

  disableTools(toolNames: string[]): DisableToolsResult {
    this.ensureInitialized();
    const result = this.applyMutation(() => disableToolStates(this.toolStates, this.categoryStates, toolNames));

    logInfoForNames('Disabled tools', result.disabled);
    if (result.protected.length > 0) {
      log.warn(`Cannot disable protected tools: ${result.protected.join(', ')}`);
    }

    return result;
  }

  enableCategory(category: ToolCategory): CategoryEnableResult {
    this.ensureInitialized();
    const result = this.applyMutation(() => enableCategoryState(this.toolStates, this.categoryStates, category));

    if (result.enabled.length > 0) {
      const target = category === 'all' ? 'all categories' : `category '${category}'`;
      log.info(`Enabled ${target}: ${result.enabled.length} tools`);
    }

    return result;
  }

  disableCategory(category: ToolCategory): CategoryDisableResult {
    this.ensureInitialized();
    const result = this.applyMutation(() => disableCategoryState(this.toolStates, this.categoryStates, category));
    if (category === 'core' && !result.notFound) {
      log.warn(`Cannot disable protected category: ${category}`);
    }

    if (result.disabled.length > 0) {
      const target = category === 'all' ? 'all categories' : `category '${category}'`;
      log.info(`Disabled ${target}: ${result.disabled.length} tools`);
    }

    return result;
  }

  getStatus(): {
    totalTools: number;
    enabledTools: number;
    disabledTools: number;
    categories: CategoryState[];
    catalogStateRevision: number;
  } {
    this.ensureInitialized();
    const visibleTools = this.listTools();
    const enabledCount = countEnabledTools(this.toolStates, this.categoryStates);

    return {
      totalTools: visibleTools.length,
      enabledTools: enabledCount,
      disabledTools: visibleTools.length - enabledCount,
      categories: this.listCategories(),
      catalogStateRevision: this.catalogStateRevision
    };
  }

  getCatalogStateRevision(): number {
    return this.catalogStateRevision;
  }

  reset(): { enabled: number } {
    this.ensureInitialized();
    const count = this.applyMutation(() => resetToolStates(this.toolStates, this.categoryStates));

    log.info(`Reset ${count} tools to enabled state`);
    return { enabled: count };
  }

  isToolEnabled(toolName: string): boolean {
    this.ensureInitialized();
    return isToolStateEnabled(this.toolStates, this.categoryStates, toolName);
  }

  getToolState(toolName: string): ToolState | undefined {
    this.ensureInitialized();
    return this.toolStates.get(toolName);
  }

  // Only the flags isToolStateEnabled() reads. enabledCount is excluded because it
  // is a cache recomputed on every read and rewritten unconditionally by reset(),
  // so folding it in would report a no-op batch as a visibility change.
  private visibilityFingerprint(): string {
    const tools = Array.from(this.toolStates.values(), state => `${state.name}=${state.enabled ? 1 : 0}`);
    const categories = Array.from(this.categoryStates.values(), cat => `${cat.name}=${cat.enabled ? 1 : 0}`);
    return `${tools.join(',')}|${categories.join(',')}`;
  }

  // Advances the revision at most once per batch, and only when visibility really
  // moved. Compare-mutate-compare runs synchronously, so no caller can observe a
  // revision before the state change it describes.
  private applyMutation<T>(mutate: () => T): T {
    const before = this.visibilityFingerprint();
    const result = mutate();
    if (this.visibilityFingerprint() !== before) {
      this.catalogStateRevision++;
    }
    return result;
  }

  private addToolDefinition(def: ToolDefinition): void {
    const category: ToolCategory = def.category ?? 'utility';
    this.toolStates.set(def.name, {
      name: def.name,
      category,
      enabled: true,
      description: def.description
    });

    let catState = this.categoryStates.get(category);
    if (catState === undefined) {
      catState = { name: category, enabled: true, toolCount: 0, enabledCount: 0 };
      this.categoryStates.set(category, catState);
    }
    catState.toolCount++;
    catState.enabledCount++;
  }

  private ensureInitialized(): void {
    if (!this.initialized) {
      this.initialize();
    }
  }
}

export const dynamicToolManager = new DynamicToolManager();
dynamicToolManager.initialize();
