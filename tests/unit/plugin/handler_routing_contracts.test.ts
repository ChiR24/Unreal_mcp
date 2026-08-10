import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

describe('plugin handler routing contracts', () => {
  it('preserves the material authoring unknown-action response', () => {
    const source = pluginSource(
      'Domains',
      'MaterialAuthoring',
      'McpAutomationBridge_MaterialAuthoringHandlers.cpp',
    );

    expect(source).toContain('TEXT("Unknown subAction: %s")');
    expect(source).toContain('TEXT("INVALID_SUBACTION")');
  });

  it('leaves unmatched GAS actions to the parent dispatcher', () => {
    const leaf = pluginSource(
      'Domains',
      'GAS',
      'McpAutomationBridge_GASHandlersAbilityGrantExecution.cpp',
    );
    const dispatcher = pluginSource(
      'Domains',
      'GAS',
      'McpAutomationBridge_GASHandlers.cpp',
    );

    expect(leaf).not.toContain('TEXT("Unknown GAS subAction: %s")');
    expect(leaf).toMatch(/return false;\s*}\s*}\s*#endif\s*$/);
    expect(dispatcher).toContain('TEXT("Unknown GAS subAction: %s")');
    expect(dispatcher).toContain('TEXT("UNKNOWN_SUBACTION")');
  });

  it('preserves interaction destruction response contracts after the split', () => {
    const source = pluginSource(
      'Domains',
      'Interaction',
      'McpAutomationBridge_InteractionHandlersDestruction.cpp',
    );

    expect(source).toContain('TEXT("Destructible mesh setup configured")');
    expect(source).toContain('TEXT("setup_destructible_mesh is editor-only")');
    expect(source).toContain('TEXT("Destruction levels configured")');
    expect(source).toContain('TEXT("configure_destruction_levels is editor-only")');
    expect(source).toContain('TEXT("Destruction effects configured")');
    expect(source).toContain('TEXT("configure_destruction_effects is editor-only")');
    expect(source).toContain('TEXT("Destruction damage configured")');
    expect(source).toContain('TEXT("configure_destruction_damage is editor-only")');
    expect(source).toContain('TEXT("Destruction component added")');
    expect(source).toContain('TEXT("add_destruction_component is editor-only")');
  });

  it('preserves trigger configuration response contracts after the split', () => {
    const source = pluginSource(
      'Domains',
      'Interaction',
      'McpAutomationBridge_InteractionHandlersTriggers.cpp',
    );

    expect(source).toContain('TEXT("Trigger events configured")');
    expect(source).toContain('TEXT("configure_trigger_events is editor-only")');
    expect(source).toContain('TEXT("Trigger filter configured")');
    expect(source).toContain('TEXT("configure_trigger_filter is editor-only")');
    expect(source).toContain('TEXT("Trigger response configured")');
    expect(source).toContain('TEXT("configure_trigger_response is editor-only")');
    expect(source).toContain('TEXT("eventsAdded")');
    expect(source).toContain('TEXT("variablesAdded")');
    expect(source).toContain('TEXT("eventCount")');
  });

  it('preserves the landscape grass mesh alias and missing-mesh error', () => {
    const source = pluginSource(
      'Domains',
      'Landscape',
      'McpAutomationBridge_LandscapeGrassType.cpp',
    );

    expect(source).toContain('TryGetStringField(TEXT("meshPath"), MeshPath)');
    expect(source).toContain('TryGetStringField(TEXT("staticMesh"), MeshPath)');
    expect(source).toContain('TEXT("meshPath or staticMesh required")');
    expect(source).toContain('TEXT("INVALID_ARGUMENT")');
  });

  it('preserves manage_effect Niagara authoring delegation', () => {
    const source = pluginSource(
      'Domains',
      'Effect',
      'McpAutomationBridge_EffectHandlers.cpp',
    );
    const normalization = source.indexOf(
      'McpEffectHandlers::NormalizeNativeSubAction',
    );
    const authoringRoute = source.indexOf(
      'McpEffectHandlers::IsNiagaraAuthoringSubAction',
    );
    const legacyContext = source.indexOf(
      'McpEffectHandlers::FEffectActionContext Context',
    );

    expect(normalization).toBeGreaterThanOrEqual(0);
    expect(authoringRoute).toBeGreaterThan(normalization);
    expect(legacyContext).toBeGreaterThan(authoringRoute);
    expect(source).toContain(
      'HandleManageNiagaraAuthoringAction(\n' +
        '            RequestId, TEXT("manage_niagara_authoring"), LocalPayload, RequestingSocket)',
    );
  });

  // Regression: HandleManageSkeleton declined foreign actions with `return true`
  // — the value that means "I consumed this request". Sitting partway down the
  // fallback chain in McpAutomationBridge_ProcessRequestDispatch.cpp, it claimed
  // every action that reached it unmatched and returned without sending
  // anything, so the caller skipped both the "No handler consumed" warning and
  // the UNKNOWN_ACTION reply and the client waited out a silent 300 s transport
  // timeout. It also made every handler ordered after it unreachable by
  // fallback. Every chain handler must DECLINE with false.
  it('declines foreign actions rather than silently consuming them', () => {
    const source = pluginSource(
      'Domains',
      'Skeleton',
      'McpAutomationBridge_SkeletonHandlers.cpp',
    );

    expect(source).toMatch(
      /if \(Action != TEXT\("manage_skeleton"\)\)\s*\{\s*return false;\s*\}/,
    );
  });
});
