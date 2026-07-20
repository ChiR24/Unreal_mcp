import {
  PROTECTED_TOOL_NAMES,
  type CategoryDisableResult,
  type CategoryEnableResult,
  type CategoryState,
  type DisableToolsResult,
  type EnableToolsResult,
  type ToolCategory,
  type ToolState
} from './dynamic-tool-types.js';

const PROTECTED_CATEGORIES: readonly ToolCategory[] = ['core'];

export function enableToolStates(
  toolStates: Map<string, ToolState>,
  categoryStates: Map<ToolCategory, CategoryState>,
  toolNames: string[]
): EnableToolsResult {
  const enabled: string[] = [];
  const notFound: string[] = [];

  for (const name of toolNames) {
    const state = toolStates.get(name);
    if (state === undefined) {
      notFound.push(name);
      continue;
    }

    const catState = categoryStates.get(state.category);
    if (catState !== undefined) catState.enabled = true;
    if (!state.enabled) {
      state.enabled = true;
      if (catState !== undefined) catState.enabledCount++;
    }
    enabled.push(name);
  }

  return { success: true, enabled, notFound };
}

export function disableToolStates(
  toolStates: Map<string, ToolState>,
  categoryStates: Map<ToolCategory, CategoryState>,
  toolNames: string[]
): DisableToolsResult {
  const disabled: string[] = [];
  const notFound: string[] = [];
  const protectedTools: string[] = [];

  for (const name of toolNames) {
    if (PROTECTED_TOOL_NAMES.has(name)) {
      protectedTools.push(name);
      continue;
    }

    const state = toolStates.get(name);
    if (state === undefined) {
      notFound.push(name);
      continue;
    }

    if (state.enabled) {
      state.enabled = false;
      const catState = categoryStates.get(state.category);
      if (catState !== undefined && catState.enabledCount > 0) catState.enabledCount--;
    }
    disabled.push(name);
  }

  return { success: true, disabled, notFound, protected: protectedTools };
}

export function enableCategoryState(
  toolStates: Map<string, ToolState>,
  categoryStates: Map<ToolCategory, CategoryState>,
  category: ToolCategory
): CategoryEnableResult {
  const enabled: string[] = [];

  if (category === 'all') {
    for (const catState of categoryStates.values()) {
      catState.enabled = true;
      catState.enabledCount = catState.toolCount;
    }
    for (const state of toolStates.values()) {
      if (!state.enabled) {
        state.enabled = true;
        enabled.push(state.name);
      }
    }
    return { success: true, enabled, notFound: false };
  }

  const catState = categoryStates.get(category);
  if (catState === undefined) {
    return { success: false, enabled: [], notFound: true };
  }

  catState.enabled = true;
  for (const state of toolStates.values()) {
    if (state.category === category && !state.enabled) {
      state.enabled = true;
      enabled.push(state.name);
    }
  }
  catState.enabledCount = catState.toolCount;

  return { success: true, enabled, notFound: false };
}

export function disableCategoryState(
  toolStates: Map<string, ToolState>,
  categoryStates: Map<ToolCategory, CategoryState>,
  category: ToolCategory
): CategoryDisableResult {
  const disabled: string[] = [];
  const protectedTools: string[] = [];

  if (category === 'all') {
    for (const catState of categoryStates.values()) {
      if (catState.name === 'core') {
        catState.enabled = true;
      } else {
        catState.enabled = false;
        catState.enabledCount = 0;
      }
    }
    for (const state of toolStates.values()) {
      if (PROTECTED_TOOL_NAMES.has(state.name) || PROTECTED_CATEGORIES.includes(state.category)) {
        protectedTools.push(state.name);
      } else if (state.enabled) {
        state.enabled = false;
        disabled.push(state.name);
      }
    }
    const coreCatState = categoryStates.get('core');
    if (coreCatState !== undefined) {
      coreCatState.enabledCount = protectedTools.filter(name => toolStates.get(name)?.category === 'core').length;
    }
    return { success: true, disabled, notFound: false, protected: protectedTools };
  }

  const catState = categoryStates.get(category);
  if (catState === undefined) {
    return { success: false, disabled: [], notFound: true, protected: [] };
  }

  // Rejected outright, not partially applied: the catalog advertises that core
  // cannot be disabled, so disabling its unprotected members would falsify that claim.
  if (PROTECTED_CATEGORIES.includes(category)) {
    for (const state of toolStates.values()) {
      if (state.category === category && PROTECTED_TOOL_NAMES.has(state.name)) {
        protectedTools.push(state.name);
      }
    }
    return { success: true, disabled, notFound: false, protected: protectedTools };
  }

  catState.enabled = false;

  for (const state of toolStates.values()) {
    if (state.category !== category) {
      continue;
    }
    if (PROTECTED_TOOL_NAMES.has(state.name)) {
      protectedTools.push(state.name);
    } else if (state.enabled) {
      state.enabled = false;
      disabled.push(state.name);
    }
  }

  catState.enabledCount = 0;
  return { success: true, disabled, notFound: false, protected: protectedTools };
}

export function resetToolStates(
  toolStates: Map<string, ToolState>,
  categoryStates: Map<ToolCategory, CategoryState>
): number {
  let count = 0;
  for (const state of toolStates.values()) {
    if (!state.enabled) {
      state.enabled = true;
      count++;
    }
  }

  for (const catState of categoryStates.values()) {
    catState.enabled = true;
    catState.enabledCount = catState.toolCount;
  }

  return count;
}
