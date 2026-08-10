/**
 * Evidence ledger for declared preview / undo / compensation semantics.
 *
 * This is the ONLY place a capability can earn a claim stronger than the
 * pessimistic default. Every entry must cite the implementation that proves it
 * (a `path:line` for `source-verified`, a named rule for `contract-derived`).
 *
 * Acceptance rules, applied when adding an entry:
 *  - UNDO requires a scoped editor transaction around the whole mutation AND no
 *    durable persistence escaping that scope -- where "escaping" covers a write
 *    that happens INSIDE the scope just as much as one before or after it, since
 *    rolling back the transaction does not unwrite bytes either way.
 *    `build_environment.create_landscape` is deliberately absent: its actor is
 *    spawned before the transaction opens
 *    (McpAutomationBridge_LandscapeCreationTask.cpp:31 spawn vs :88 transaction),
 *    so undo would leave the actor behind. `struct.import_struct` is absent for
 *    the same reason -- it calls McpSafeAssetSave on the success path
 *    (McpAutomationBridge_AssetWorkflowStructsImport.cpp:172).
 *    Persistence must be traced through ALIASES, never grepped by name: a search
 *    for `McpSafeAssetSave` does not match `SaveLoadedAssetThrottled`, which is
 *    how the Blueprint-graph handlers reach disk (see UNDO_EVIDENCE below).
 *    `tests/unit/plugin/undo-evidence-durability-contract.test.ts` derives that
 *    alias set from source and fails any entry whose cited handler can reach a
 *    package write.
 *  - COMPENSATION targets must be capability ids that exist in this catalogue;
 *    the resolver rejects an unknown id so a stale entry fails the build.
 */
import type { PREVIEW_REPORTS } from '../../constants.js';

export type PreviewLedgerEntry = {
  readonly mode: 'validate-only' | 'simulate';
  readonly reports: readonly (typeof PREVIEW_REPORTS)[number][];
  readonly citation: string;
};

export type UndoLedgerEntry = {
  readonly transactionScope: string;
  readonly citation: string;
};

export type CompensationLedgerEntry =
  | { readonly kind: 'inverse'; readonly inverse: readonly string[]; readonly citation: string }
  | { readonly kind: 'manual'; readonly guidance: string; readonly citation: string };

/**
 * No capability declares a real no-op preview.
 *
 * Verified absence, not an oversight: no dry-run path exists on either
 * transport. The only `preview` strings in the plugin are a lighting build
 * QUALITY level (McpAutomationBridge_LightingHandlersBuild.cpp:33) and an
 * editor viewport control (McpAutomationBridge_EditorFunctionHandlersControls.cpp:98),
 * and no domain handler branches on a dry-run flag. `options.preview` therefore
 * cannot be honored by any leaf today, which is exactly what the gateway needs
 * in order to fail closed.
 */
export const PREVIEW_EVIDENCE: ReadonlyMap<string, PreviewLedgerEntry> = new Map();

const BLUEPRINT_GRAPH = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph';

/**
 * No capability earns a transaction-backed undo claim.
 *
 * Verified absence, not an oversight. Seven Blueprint-graph mutations used to
 * sit here (`blueprint.create_node`, `connect_pins`, `break_pin_links`,
 * `set_pin_default_value`, `delete_node`, `create_reroute_node`,
 * `set_node_property`). They really do open an FScopedTransaction and really do
 * call Modify() on the Blueprint and graph before mutating -- but every one of
 * them also writes the Blueprint package to disk while that scope is still
 * open, which the UNDO rule above disqualifies. Transaction opens -> save fires
 * -> scope closes, at these lines:
 *   ...HandlersNodeMutations.cpp   40 -> 48 -> 54   (delete_node)
 *                                  63 -> 79 -> 91   (create_reroute_node)
 *                                 100 -> 186 -> 196 (set_node_property)
 *   ...HandlersPinMutations.cpp    16 -> 110 -> 115 (connect_pins)
 *                                 124 -> 150 -> 156 (break_pin_links)
 *                                 192 -> 199 -> 209 (set_pin_default_value)
 *   ...HandlersNodeCreation.cpp    85 -> (see below) -> 109 (create_node)
 *
 * `create_node` reaches the save indirectly, which is why it looked clean: the
 * transaction opened at ...HandlersNodeCreation.cpp:85 is still on the stack
 * while every create branch runs, and each branch saves before returning --
 * FActionContext::FinalizeNode (...HandlersPrivate.h:63), CreateDynamicNode
 * (...HandlersNodeCreationDynamic.cpp:289), ...HandlersSpecialNodes.cpp:204,
 * ...HandlersConstructObjectNodes.cpp:104, ...HandlersCustomEvents.cpp:214.
 *
 * An earlier revision of this comment asserted these handlers "never call
 * McpSafeAssetSave or SavePackage". Literally true by name, false in effect:
 * they call `SaveLoadedAssetThrottled`
 * (Private/Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h:53),
 * which calls `McpSafeAssetSave`, which calls
 * `UPackageTools::SavePackagesForObjects` / `FEditorFileUtils::PromptForCheckoutAndSave`
 * (Private/Safety/McpSafeOperationsAssetSave.h:101 and :119). A second alias
 * with the same body lives at Private/Safety/McpSafeOperationsMaterial.h:53.
 *
 * So undo restores the in-memory graph and leaves the mutated `.uasset` on
 * disk. Worse, the save is THROTTLED: a call inside the throttle window is
 * skipped and still reports success
 * (McpAutomationBridgeHelpersAssetSaveRegistry.h:36-51), so whether any bytes
 * landed is nondeterministic. A partial, nondeterministic rollback advertised
 * as `undo: true` is worse than claiming nothing, so this map is empty and the
 * three of the seven that have a real inverse capability are recorded under
 * COMPENSATION instead.
 */
export const UNDO_EVIDENCE: ReadonlyMap<string, UndoLedgerEntry> = new Map();

const pairedInverse = (inverse: string, citation: string): CompensationLedgerEntry => ({
  kind: 'inverse',
  inverse: [inverse],
  citation
});

const CATALOGUE_PAIR = 'catalogue-verified inverse capability';

/**
 * Cleanup for durable effects that no transaction can revert. An `inverse`
 * entry names a capability that reverses the durable effect; it never implies
 * the original call was rolled back.
 *
 * An inverse is only recorded when the ORIGINAL call hands back everything the
 * inverse needs. That is why only three of the seven ex-UNDO Blueprint-graph
 * mutations appear here: `delete_node`, `break_pin_links`,
 * `set_pin_default_value` and `set_node_property` destroy state they never
 * report (the removed node's type/position/links, the prior link set, the prior
 * pin default, the prior property value), so no catalogue call can restore it
 * from the response alone. Those four stay fully pessimistic.
 */
export const COMPENSATION_EVIDENCE: ReadonlyMap<string, CompensationLedgerEntry> = new Map([
  ['control_actor.spawn', pairedInverse('control_actor.delete', CATALOGUE_PAIR)],
  ['control_actor.spawn_actor', pairedInverse('control_actor.delete', CATALOGUE_PAIR)],
  ['control_actor.spawn_blueprint', pairedInverse('control_actor.delete', CATALOGUE_PAIR)],
  ['control_actor.duplicate', pairedInverse('control_actor.delete', CATALOGUE_PAIR)],
  ['build_environment.spawn_light', pairedInverse('build_environment.delete', CATALOGUE_PAIR)],
  ['build_environment.spawn_sky_light', pairedInverse('build_environment.delete', CATALOGUE_PAIR)],
  ['build_environment.create_landscape', pairedInverse('build_environment.delete', CATALOGUE_PAIR)],
  ['manage_audio.push_sound_mix', pairedInverse('manage_audio.pop_sound_mix', CATALOGUE_PAIR)],
  ['control_editor.start_recording', pairedInverse('control_editor.stop_recording', CATALOGUE_PAIR)],
  ['control_editor.open_asset', pairedInverse('control_editor.close_asset', CATALOGUE_PAIR)],
  ['system_control.start_profiling', pairedInverse('system_control.stop_profiling', CATALOGUE_PAIR)],
  ['system_control.start_session', pairedInverse('system_control.stop_session', CATALOGUE_PAIR)],
  ['manage_tools.enable_tools', pairedInverse('manage_tools.disable_tools', CATALOGUE_PAIR)],
  ['manage_tools.enable_category', pairedInverse('manage_tools.disable_category', CATALOGUE_PAIR)],
  ['sequence.take.start_recording', pairedInverse('sequence.take.stop_recording', CATALOGUE_PAIR)],
  [
    'sequence.replay.start_demo_recording',
    pairedInverse('sequence.replay.stop_demo_recording', CATALOGUE_PAIR)
  ],
  [
    'struct.import_struct',
    pairedInverse(
      'struct.delete_struct',
      'save escapes the import transaction (McpAutomationBridge_AssetWorkflowStructsImport.cpp:172)'
    )
  ],
  [
    'blueprint.create_node',
    pairedInverse(
      'blueprint.delete_node',
      `${CATALOGUE_PAIR}; the create response returns the new nodeId that delete_node consumes (${BLUEPRINT_GRAPH}/McpAutomationBridge_BlueprintGraphHandlersPrivate.h:66)`
    )
  ],
  [
    'blueprint.create_reroute_node',
    pairedInverse(
      'blueprint.delete_node',
      `${CATALOGUE_PAIR}; the create response returns the new nodeId that delete_node consumes (${BLUEPRINT_GRAPH}/McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp:82)`
    )
  ],
  [
    'blueprint.connect_pins',
    pairedInverse(
      'blueprint.break_pin_links',
      `${CATALOGUE_PAIR}, but OVER-BROAD: break_pin_links severs every link on the named pin, not just the one connect_pins added (${BLUEPRINT_GRAPH}/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp:148), so a pin that already carried links loses those too`
    )
  ],
  [
    'sequence.mrq.start_render',
    {
      kind: 'manual',
      guidance:
        'Rendered frames are written to the configured MRQ output directory and are not removed by any capability; delete them from disk. The render itself only supports advisory cancellation while running.',
      citation: 'no render-cancel capability exists in the catalogue'
    }
  ]
]);
