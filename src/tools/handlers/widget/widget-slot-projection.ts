// BB-043: the nine widget-game-ui actions declare slotName as a REQUIRED
// output, but the native handlers answer with widgetName for the eight
// template actions, so a schema-valid mutation was reported as a violation.
// The caller-supplied slot name always wins; this only fills the gap.
//
// add_quest_tracker is in the list for completeness of the declared contract,
// but its promotion branch is inert by design: its native handler always emits
// slotName itself.

const WIDGET_GAME_UI_SLOT_ACTIONS: ReadonlySet<string> = new Set([
  'add_ammo_counter',
  'add_compass',
  'add_crosshair',
  'add_damage_indicator',
  'add_health_bar',
  'add_interaction_prompt',
  'add_minimap',
  'add_objective_tracker',
  'add_quest_tracker'
]);

function nonEmptyString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

/** Fill the declared slotName from widgetName when the native reply omitted it. */
export function projectWidgetSlotName(
  action: string,
  response: Record<string, unknown>
): Record<string, unknown> {
  if (!WIDGET_GAME_UI_SLOT_ACTIONS.has(action)) return response;
  if (nonEmptyString(response.slotName) !== undefined) return response;

  const widgetName = nonEmptyString(response.widgetName);
  if (widgetName === undefined) return response;

  return { ...response, slotName: widgetName };
}
