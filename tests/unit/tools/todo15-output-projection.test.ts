// Plan Todo 15 (BB-010, BB-024, BB-036, BB-037, BB-040, BB-043, BB-044,
// BB-048, BB-064) - a declared output field must actually be emitted, and an
// emitted field must actually be declared.
//
// Written after the fixes landed, so non-vacuity is proven by mutation: toggle
// any one fix off and the case naming it fails. The native cases are
// source-contract reads because no engine root exists here to compile or run
// the plugin.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it, vi } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { projectWidgetSlotName } from '../../../src/tools/handlers/widget/widget-slot-projection.js';
import { handleWidgetAuthoringTools } from '../../../src/tools/handlers/widget/widget-authoring-handlers.js';
import { getWidgetActionRequiredFields } from '../../../src/tools/handlers/widget/widget-authoring-action-requirements.js';

const DOMAINS = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains'
);

function nativeSource(...segments: readonly string[]): string {
  return readFileSync(join(DOMAINS, ...segments), 'utf8');
}

function outputProperties(id: string): readonly string[] {
  const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === id);
  if (!found) throw new Error(`fixture record '${id}' is absent from the canonical source`);
  return Object.keys(JSON.parse(JSON.stringify(found.schemas.output.properties)) as object);
}

function inputProperties(id: string): readonly string[] {
  const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === id);
  if (!found) throw new Error(`fixture record '${id}' is absent from the canonical source`);
  return Object.keys(JSON.parse(JSON.stringify(found.schemas.input.properties)) as object);
}

describe('todo15 BB-010: the Blueprint snapshot emits the declared parentClass', () => {
  it('BuildBlueprintSnapshot writes parentClass', () => {
    const source = nativeSource('Blueprint', 'Events', 'McpAutomationBridge_BlueprintHandlersEventIntrospection.cpp');

    expect(source).toContain('SetStringField(TEXT("parentClass")');
    expect(source).toContain('Blueprint->ParentClass');
  });
});

describe('todo15 BB-024: create_niagara_system returns the system it created', () => {
  it('CreateNiagaraSystem writes systemPath into the result', () => {
    const source = nativeSource('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersSystems.cpp');

    expect(source).toContain('SetStringField(TEXT("systemPath")');
    expect(source).toContain('NewSystem->GetPathName()');
  });
});

describe('todo15 BB-036/BB-037: the runtime report and frame step stay complete', () => {
  it('the runtime report emits playerController', () => {
    const source = nativeSource('Environment', 'Inspection', 'McpAutomationBridge_EnvironmentHandlersInspectRuntime.cpp');

    expect(source).toContain('SetStringField(TEXT("playerController")');
  });

  it('step_frame reports how many steps it actually took', () => {
    const source = nativeSource('ControlEditor', 'McpAutomationBridge_ControlEditorPlay.cpp');

    expect(source).toContain('SetNumberField(TEXT("steps")');
  });
});

describe('todo15 BB-040: get_properties frameRate matches its declared union', () => {
  const source = (): string =>
    nativeSource('Sequence', 'McpAutomationBridge_SequenceHandlersProperties.cpp');

  it('get_properties emits a decimal number, never the numerator/denominator object', () => {
    const getProperties = source().split('HandleSequenceGetProperties')[1] ?? '';

    expect(getProperties).toContain('SetNumberField(TEXT("frameRate"), FR.AsDecimal())');
    // Broad, not just the FrameRateObj spelling: the success-answering fallback
    // further down emitted an empty object, which the narrow form did not catch.
    expect(getProperties).not.toContain('SetObjectField(TEXT("frameRate")');
  });

  it('the set_properties site keeps its object shape (deliberately untouched)', () => {
    const setProperties = source().split('HandleSequenceGetProperties')[0] ?? '';

    expect(setProperties).toContain('SetObjectField(TEXT("frameRate"), FrameRateObj)');
  });

  it('the record declares frameRate as a number|string union, never an object', () => {
    const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === 'sequence.get_properties');
    if (!found) throw new Error('sequence.get_properties is absent from the canonical source');
    const frameRate = JSON.parse(
      JSON.stringify(found.schemas.output.properties.frameRate)
    ) as Record<string, unknown>;

    expect(frameRate.type).toEqual(['number', 'string']);
  });
});

describe('todo15 BB-044: the lightweight Blueprint probe reports reachability', () => {
  it('the probe emits reachable alongside exists', () => {
    const source = nativeSource('Blueprint', 'Queries', 'McpAutomationBridge_BlueprintHandlersEnsureProbe.cpp');

    expect(source).toContain('SetBoolField(TEXT("reachable"), bExists)');
  });
});

describe('todo15 BB-048: get_ai_info emits nothing it does not declare', () => {
  function aiInfoDeclaredKeysUnderClosedSchema(): readonly string[] {
    const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === 'manage_ai.get_ai_info');
    if (!found) throw new Error('manage_ai.get_ai_info is absent from the canonical source');
    const aiInfo = JSON.parse(
      JSON.stringify(found.schemas.output.properties.aiInfo)
    ) as { properties?: Record<string, unknown>; additionalProperties?: unknown };
    expect(aiInfo.additionalProperties).toBe(false);
    return Object.keys(aiInfo.properties ?? {});
  }

  it('the declared root-decorator arrays are emitted', () => {
    const source = nativeSource('AI', 'Runtime', 'McpAutomationBridge_AIHandlersUtility.cpp');

    expect(source).toContain('SetArrayField(TEXT("rootDecoratorClasses")');
    expect(source).toContain('SetArrayField(TEXT("rootDecorators")');
  });

  it('the declared rootDecoratorCount survives', () => {
    const source = nativeSource('AI', 'Runtime', 'McpAutomationBridge_AIHandlersUtility.cpp');

    expect(source).toContain('SetNumberField(TEXT("rootDecoratorCount")');
  });

  it('both root-decorator arrays are declared by the record', () => {
    const declared = aiInfoDeclaredKeysUnderClosedSchema();

    expect(declared).toContain('rootDecoratorCount');
    expect(declared).toContain('rootDecoratorClasses');
    expect(declared).toContain('rootDecorators');
  });
});

describe('todo15 BB-064: get_node_details echoes the node it described', () => {
  // GetPinDetails carries an identical echo, so a whole-file search proves
  // nothing about GetNodeDetails; the assertion is scoped to that body alone.
  it('GetNodeDetails writes nodeId into its own result', () => {
    const source = nativeSource('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersDetails.cpp');
    const getNodeDetails = (source.split('static bool GetNodeDetails')[1] ?? '')
      .split('static bool GetPinDetails')[0] ?? '';

    expect(getNodeDetails).not.toHaveLength(0);
    expect(getNodeDetails).toContain('SetStringField(TEXT("nodeId"), NodeId)');
  });
});

describe('todo15 BB-043: the widget-game-ui slot name is always answered', () => {
  const GAME_UI_ACTIONS = [
    'add_ammo_counter',
    'add_compass',
    'add_crosshair',
    'add_damage_indicator',
    'add_health_bar',
    'add_interaction_prompt',
    'add_minimap',
    'add_objective_tracker',
    'add_quest_tracker'
  ] as const;

  it.each(GAME_UI_ACTIONS)('%s declares slotName as a required output', (action) => {
    const declared = outputProperties(`blueprint.${action}`);

    expect(declared).toContain('slotName');
  });

  it.each(GAME_UI_ACTIONS)('%s promotes widgetName into the missing slotName', (action) => {
    const projected = projectWidgetSlotName(action, { success: true, widgetName: 'HUD_Slot' });

    expect(projected.slotName).toBe('HUD_Slot');
  });

  it('a caller-supplied slotName always wins over widgetName', () => {
    const projected = projectWidgetSlotName('add_health_bar', {
      slotName: 'ExplicitSlot',
      widgetName: 'DerivedSlot'
    });

    expect(projected.slotName).toBe('ExplicitSlot');
  });

  it('an unrelated widget action is left untouched', () => {
    const response = { success: true, widgetName: 'SomePanel' };

    expect(projectWidgetSlotName('add_canvas_panel', response)).toEqual(response);
  });

  it('a reply with no widgetName is left untouched', () => {
    const response = { success: true };

    expect(projectWidgetSlotName('add_health_bar', response)).toEqual(response);
  });

  it('add_quest_tracker is routable, matching the record it advertises', () => {
    expect(getWidgetActionRequiredFields('add_quest_tracker')).toEqual(['widgetPath']);
    expect(inputProperties('blueprint.add_quest_tracker')).toContain('widgetPath');
  });

  // The helper cases above pass even when nothing calls the helper, so this
  // drives the real handler over a stubbed bridge: it fails if the wiring goes.
  it('the handler applies the projection to a real native reply', async () => {
    const sendAutomationRequest = vi.fn(async () => ({ success: true, widgetName: 'HUD_Slot' }));
    const tools = {
      systemTools: {
        executeConsoleCommand: vi.fn(async () => ({ success: true })),
        getProjectSettings: vi.fn(async () => ({}))
      },
      assetResources: { list: vi.fn(async () => ({})) },
      automationBridge: { isConnected: () => true, sendAutomationRequest }
    } as unknown as ITools;

    const result = await handleWidgetAuthoringTools(
      'add_health_bar',
      { action: 'add_health_bar', widgetPath: '/Game/UI/WBP_HUD' },
      tools
    );

    expect(sendAutomationRequest).toHaveBeenCalled();
    expect(result.slotName).toBe('HUD_Slot');
  });
});

// The Todo 13 modify_scs fix removed the only manage_blueprint declaration of
// `properties`, which left the field consumed by two handlers but declarable by
// none. These lock the field to the actions that genuinely read it.
describe('todo15 regression: a consumed properties bag stays declarable', () => {
  it('blueprint.create declares the properties bag it forwards to the CDO', () => {
    expect(inputProperties('blueprint.create')).toContain('properties');
  });

  it('blueprint.add_component declares the properties bag it nests into its operation', () => {
    expect(inputProperties('blueprint.add_component')).toContain('properties');
  });

  it('modify_scs still does NOT declare it (the Todo 13 fix stands)', () => {
    expect(inputProperties('blueprint.modify_scs')).not.toContain('properties');
  });
});
