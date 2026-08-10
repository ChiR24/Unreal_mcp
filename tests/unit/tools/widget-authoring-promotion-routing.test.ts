/**
 * Routing contract for the promoted WidgetAuthoring routes.
 *
 * Eighteen WidgetAuthoring operations are implemented natively but were never
 * registered on a canonical parent, so the route ledger classes them
 * `promote`. Reaching them through `manage_blueprint` needs BOTH routing
 * surfaces to name the action, and neither is generated:
 *
 * - TypeScript: `WIDGET_AUTHORING_ACTIONS` feeds `widgetAuthoringActionSet`,
 *   which decides whether the consolidated handler dispatches to the widget
 *   domain or falls through to the Blueprint handler.
 * - Native: the `WidgetAuthoring()` array backs `IsWidgetAuthoringAction`,
 *   which is the gate the `manage_blueprint` registration lambda consults
 *   before forwarding to `HandleManageWidgetAuthoringAction`.
 *
 * The native half is asserted as source text on purpose: this suite has no
 * compiler, so the array contents are the only checkable proof that the
 * plugin would forward the action rather than silently mis-route it.
 */

import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { widgetAuthoringActionSet } from '../../../src/tools/orchestration/consolidated-routing.js';

const PROMOTED_ACTIONS: readonly string[] = [
  'add_quest_tracker',
  'add_safe_zone',
  'add_spacer',
  'add_widget_component',
  'add_widget_switcher',
  'bind_localized_text',
  'create_credits_screen',
  'create_shop_ui',
  'create_widget_style',
  'delete_animation',
  'get_widget_slot_info',
  'remove_widget',
  'rename_widget',
  'reparent_widget',
  'set_font',
  'set_localization_key',
  'set_margin',
  'set_widget_binding',
];

const NATIVE_ROUTING_HEADER =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/'
  + 'McpConsolidatedActionRoutingBlueprints.h';

describe('promoted WidgetAuthoring routes dispatch on both surfaces', () => {
  it('the TypeScript widget-authoring action set names every promoted action', () => {
    const missing = PROMOTED_ACTIONS.filter((action) => !widgetAuthoringActionSet.has(action));

    expect(
      missing,
      `absent from WIDGET_AUTHORING_ACTIONS, so the consolidated handler would `
      + `fall through to the Blueprint domain:\n${missing.join('\n')}`,
    ).toEqual([]);
  });

  it('the native routing array names every promoted action', () => {
    const source = readFileSync(NATIVE_ROUTING_HEADER, 'utf8');
    const missing = PROMOTED_ACTIONS.filter((action) => !source.includes(`TEXT("${action}")`));

    expect(
      missing,
      `absent from WidgetAuthoring(), so IsWidgetAuthoringAction() is false and `
      + `manage_blueprint never forwards to HandleManageWidgetAuthoringAction:\n${missing.join('\n')}`,
    ).toEqual([]);
  });
});
