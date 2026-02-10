/**
 * Dynamic Tool Manager
 * 
 * Manages MCP tool visibility and enablement at runtime.
 * Allows enabling/disabling individual tools or entire categories.
 */

import { Logger } from '../utils/logger.js';
import { consolidatedToolDefinitions, type ToolDefinition } from './consolidated-tool-definitions.js';

const log = new Logger('DynamicToolManager');

export type ToolCategory = 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';

interface ToolState {
  name: string;
  category: ToolCategory;
  enabled: boolean;
  description: string;
}

interface CategoryState {
  name: ToolCategory;
  enabled: boolean;
  toolCount: number;
  enabledCount: number;
}

class DynamicToolManager {
  private toolStates = new Map<string, ToolState>();
  private categoryStates = new Map<ToolCategory, CategoryState>();
  private initialized = false;

  /**
   * Initialize the manager with all tools from definitions
   */
  initialize(): void {
    if (this.initialized) {
      log.warn('DynamicToolManager already initialized');
      return;
    }

    // Initialize all tools from definitions
    for (const def of consolidatedToolDefinitions) {
      const category = (def.category as ToolCategory) || 'utility';
      this.toolStates.set(def.name, {
        name: def.name,
        category,
        enabled: true,
        description: def.description
      });

      // Track category stats
      if (!this.categoryStates.has(category)) {
        this.categoryStates.set(category, {
          name: category,
          enabled: true,
          toolCount: 0,
          enabledCount: 0
        });
      }
      const catState = this.categoryStates.get(category)!;
      catState.toolCount++;
      catState.enabledCount++;
    }

    this.initialized = true;
    log.info(`Initialized with ${this.toolStates.size} tools across ${this.categoryStates.size} categories`);
  }

  /**
   * Get all tool definitions filtered by enabled state
   */
  getEnabledToolDefinitions(): ToolDefinition[] {
    this.ensureInitialized();
    return consolidatedToolDefinitions.filter(def => {
      const state = this.toolStates.get(def.name);
      // Also check if the category is enabled
      const catState = state ? this.categoryStates.get(state.category) : null;
      return state?.enabled && (catState?.enabled ?? true);
    });
  }

  /**
   * Get all tool definitions (unfiltered)
   */
  getAllToolDefinitions(): ToolDefinition[] {
    return consolidatedToolDefinitions;
  }

  /**
   * List all tools with their status
   */
  listTools(): ToolState[] {
    this.ensureInitialized();
    return Array.from(this.toolStates.values());
  }

  /**
   * List all categories with their status
   */
  listCategories(): CategoryState[] {
    this.ensureInitialized();
    return Array.from(this.categoryStates.values());
  }

  /**
   * Enable specific tools
   */
  enableTools(toolNames: string[]): { success: boolean; enabled: string[]; notFound: string[] } {
    this.ensureInitialized();
    const enabled: string[] = [];
    const notFound: string[] = [];

    for (const name of toolNames) {
      const state = this.toolStates.get(name);
      if (state) {
        if (!state.enabled) {
          state.enabled = true;
          // Update category enabled count
          const catState = this.categoryStates.get(state.category);
          if (catState) catState.enabledCount++;
        }
        enabled.push(name);
      } else {
        notFound.push(name);
      }
    }

    if (enabled.length > 0) {
      log.info(`Enabled tools: ${enabled.join(', ')}`);
    }
    if (notFound.length > 0) {
      log.warn(`Tools not found: ${notFound.join(', ')}`);
    }

    return { success: true, enabled, notFound };
  }

  /**
   * Disable specific tools
   */
  disableTools(toolNames: string[]): { success: boolean; disabled: string[]; notFound: string[]; protected: string[] } {
    this.ensureInitialized();
    const disabled: string[] = [];
    const notFound: string[] = [];
    const protected_: string[] = [];

    // Tools that cannot be disabled (essential for operation)
    const protectedTools = ['manage_tools', 'inspect'];

    for (const name of toolNames) {
      if (protectedTools.includes(name)) {
        protected_.push(name);
        continue;
      }

      const state = this.toolStates.get(name);
      if (state) {
        if (state.enabled) {
          state.enabled = false;
          // Update category enabled count
          const catState = this.categoryStates.get(state.category);
          if (catState && catState.enabledCount > 0) catState.enabledCount--;
        }
        disabled.push(name);
      } else {
        notFound.push(name);
      }
    }

    if (disabled.length > 0) {
      log.info(`Disabled tools: ${disabled.join(', ')}`);
    }
    if (protected_.length > 0) {
      log.warn(`Cannot disable protected tools: ${protected_.join(', ')}`);
    }

    return { success: true, disabled, notFound, protected: protected_ };
  }

  /**
   * Enable all tools in a category
   */
  enableCategory(category: ToolCategory): { success: boolean; enabled: string[]; notFound: boolean } {
    this.ensureInitialized();
    const enabled: string[] = [];

    const catState = this.categoryStates.get(category);
    if (!catState) {
      return { success: false, enabled: [], notFound: true };
    }

    catState.enabled = true;

    for (const state of this.toolStates.values()) {
      if (state.category === category && !state.enabled) {
        state.enabled = true;
        enabled.push(state.name);
      }
    }

    // Recount enabled
    catState.enabledCount = catState.toolCount;

    if (enabled.length > 0) {
      log.info(`Enabled category '${category}': ${enabled.length} tools`);
    }

    return { success: true, enabled, notFound: false };
  }

  /**
   * Disable all tools in a category
   */
  disableCategory(category: ToolCategory): { success: boolean; disabled: string[]; notFound: boolean; protected: string[] } {
    this.ensureInitialized();
    const disabled: string[] = [];
    const protected_: string[] = [];

    // Categories that cannot be fully disabled
    const protectedCategories: ToolCategory[] = ['core'];
    const protectedTools = ['manage_tools', 'inspect'];

    const catState = this.categoryStates.get(category);
    if (!catState) {
      return { success: false, disabled: [], notFound: true, protected: [] };
    }

    if (protectedCategories.includes(category)) {
      log.warn(`Cannot disable protected category: ${category}`);
      // Still allow disabling individual non-protected tools
    }

    catState.enabled = !protectedCategories.includes(category);

    for (const state of this.toolStates.values()) {
      if (state.category === category && !protectedTools.includes(state.name)) {
        if (state.enabled) {
          state.enabled = false;
          disabled.push(state.name);
        }
      } else if (state.category === category && protectedTools.includes(state.name)) {
        protected_.push(state.name);
      }
    }

    // Update enabled count
    if (!protectedCategories.includes(category)) {
      catState.enabledCount = 0;
    } else {
      catState.enabledCount = protected_.length;
    }

    if (disabled.length > 0) {
      log.info(`Disabled category '${category}': ${disabled.length} tools`);
    }

    return { success: true, disabled, notFound: false, protected: protected_ };
  }

  /**
   * Get current status summary
   */
  getStatus(): {
    totalTools: number;
    enabledTools: number;
    disabledTools: number;
    categories: CategoryState[];
  } {
    this.ensureInitialized();
    
    let enabledCount = 0;
    for (const state of this.toolStates.values()) {
      if (state.enabled) enabledCount++;
    }

    return {
      totalTools: this.toolStates.size,
      enabledTools: enabledCount,
      disabledTools: this.toolStates.size - enabledCount,
      categories: Array.from(this.categoryStates.values())
    };
  }

  /**
   * Reset all tools to enabled state
   */
  reset(): { enabled: number } {
    this.ensureInitialized();
    
    let count = 0;
    for (const state of this.toolStates.values()) {
      if (!state.enabled) {
        state.enabled = true;
        count++;
      }
    }

    // Reset categories
    for (const catState of this.categoryStates.values()) {
      catState.enabled = true;
      catState.enabledCount = catState.toolCount;
    }

    log.info(`Reset ${count} tools to enabled state`);
    return { enabled: count };
  }

  /**
   * Check if a tool is enabled
   */
  isToolEnabled(toolName: string): boolean {
    this.ensureInitialized();
    const state = this.toolStates.get(toolName);
    if (!state) return false;
    
    // Also check category
    const catState = this.categoryStates.get(state.category);
    return state.enabled && (catState?.enabled ?? true);
  }

  /**
   * Get tool state
   */
  getToolState(toolName: string): ToolState | undefined {
    this.ensureInitialized();
    return this.toolStates.get(toolName);
  }

  private ensureInitialized(): void {
    if (!this.initialized) {
      this.initialize();
    }
  }
}

// Global instance
export const dynamicToolManager = new DynamicToolManager();

// Initialize on load
dynamicToolManager.initialize();
