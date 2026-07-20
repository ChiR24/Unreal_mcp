/**
 * Shared path constants and the raw route-disposition record type.
 *
 * Every constant is a CONCRETE FILE path (never a directory) so the
 * evidence-verification test can assert `statSync().isFile()` and grep the
 * cited symbol token inside the file. The per-route file map for the 18
 * unowned widget routes is included because each route lives in a distinct
 * WidgetAuthoring subfile.
 */
import type { RouteDisposition, RouteDispositionStatus } from './types.js';

/** One raw (pre-build) route-disposition ledger row. */
export interface RawRouteDisposition {
  readonly key: string;
  readonly route: string;
  readonly action?: string;
  readonly domain: string;
  readonly status: RouteDispositionStatus;
  readonly owner: string;
  readonly evidenceSource: string;
  readonly evidenceSymbol: string;
  readonly evidenceTool: string;
  readonly citations?: readonly { readonly source: string; readonly symbol: string }[];
  readonly disposition: RouteDisposition;
  readonly targetCanonicalId?: string;
  readonly removalGuidance?: string;
  readonly rationale: string;
}

const P = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains';

/** 18 unowned widget extras with real bodies; dead-from-MCP, promote to surface. */
export const WIDGET_UNOWNED_PROMOTE = [
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
] as const;

/**
 * Per-route concrete source file for each unowned widget route (v2 source-derived).
 * Keyed by route name; every path is a concrete .cpp file inside WidgetAuthoring/.
 */
export const WIDGET_UNOWNED_FILES: Readonly<Record<string, string>> = {
  add_quest_tracker: `${P}/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringQuestTemplate.cpp`,
  add_safe_zone: `${P}/WidgetAuthoring/Layout/McpAutomationBridge_WidgetAuthoringAdditionalPanels.cpp`,
  add_spacer: `${P}/WidgetAuthoring/Layout/McpAutomationBridge_WidgetAuthoringAdditionalPanels.cpp`,
  add_widget_component: `${P}/WidgetAuthoring/Components/McpAutomationBridge_WidgetAuthoringGenericComponent.cpp`,
  add_widget_switcher: `${P}/WidgetAuthoring/Layout/McpAutomationBridge_WidgetAuthoringAdditionalPanels.cpp`,
  bind_localized_text: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringLocalization.cpp`,
  create_credits_screen: `${P}/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringCreditsTemplate.cpp`,
  create_shop_ui: `${P}/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringShopTemplate.cpp`,
  create_widget_style: `${P}/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringStyleVariables.cpp`,
  delete_animation: `${P}/WidgetAuthoring/Animation/McpAutomationBridge_WidgetAuthoringAnimationQueries.cpp`,
  get_widget_slot_info: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringManipulation.cpp`,
  remove_widget: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringManipulation.cpp`,
  rename_widget: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringManipulation.cpp`,
  reparent_widget: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringManipulation.cpp`,
  set_font: `${P}/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringAdvancedStyling.cpp`,
  set_localization_key: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringLocalization.cpp`,
  set_margin: `${P}/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringAdvancedStyling.cpp`,
  set_widget_binding: `${P}/WidgetAuthoring/Bindings/McpAutomationBridge_WidgetAuthoringUnifiedBinding.cpp`,
};

export const ROUTE_EVIDENCE_PATHS = {
  // Widget - concrete files (v2 source-derived)
  WIDGET_UI: `${P}/Ui/McpAutomationBridge_UiHandlersWidgetAuthoring.cpp`,
  WIDGET_ANIM: `${P}/WidgetAuthoring/Animation/McpAutomationBridge_WidgetAuthoringAnimationQueries.cpp`,
  WIDGET_STYLING: `${P}/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringAdvancedStyling.cpp`,
  WIDGET_CREATION: `${P}/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringCreation.cpp`,
  // Graph
  GRAPH_HANDLERS: `${P}/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersQueries.cpp`,
  // Skeleton
  SKELETON_HANDLERS: `${P}/Skeleton/McpAutomationBridge_SkeletonHandlers.cpp`,
  // Animation
  ANIM_HANDLERS: `${P}/Animation/McpAutomationBridge_AnimationHandlers.cpp`,
  ANIM_AUTHORING_IK_RETARGETING: `${P}/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringHandlersIKRetargeting.cpp`,
  ANIM_AUTHORING_INFO: `${P}/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringHandlersInfo.cpp`,
  TS_ANIM_AUTHORING_EVENTS: 'src/tools/handlers/animation/authoring/animation-authoring-sequence-events.ts',
  // GAS
  GAS_SETS: `${P}/GAS/McpAutomationBridge_GASHandlersAbilitySets.cpp`,
  GAS_GRANT: `${P}/GAS/McpAutomationBridge_GASHandlersAbilityGrantExecution.cpp`,
  GAS_POLICY: `${P}/GAS/McpAutomationBridge_GASHandlersAbilityPolicies.cpp`,
  // AI
  AI_HANDLERS: `${P}/AI/McpAutomationBridge_AIHandlers.cpp`,
  AI_PERCEPTION: `${P}/AI/Perception/McpAutomationBridge_AIHandlersPerceptionUnified.cpp`,
  AI_NAVIGATION: `${P}/AI/Navigation/McpAutomationBridge_AIHandlersNavigationUtilities.cpp`,
  // Effect
  EFFECT_NIAGARA_SPAWN: `${P}/Effect/McpAutomationBridge_EffectHandlersNiagaraSpawn.cpp`,
  EFFECT_MODULE_ROUTING: `${P}/Effect/McpAutomationBridge_EffectHandlersNiagaraModuleRouting.cpp`,
  EFFECT_TOOL: `${P}/Effect/McpAutomationBridge_EffectHandlersNiagaraLifecycle.cpp`,
  // Geometry
  GEOMETRY_HANDLERS: `${P}/Geometry/McpAutomationBridge_GeometryHandlers.cpp`,
  // Audio
  AUDIO_ASSETS: `${P}/Audio/McpAutomationBridge_AudioHandlersAssets.cpp`,
  AUDIO_HANDLERS: `${P}/Audio/McpAutomationBridge_AudioHandlers.cpp`,
  // Asset
  ASSET_WORKFLOW_HANDLERS: `${P}/AssetWorkflow/McpAutomationBridge_AssetWorkflowHandlers.cpp`,
  ASSET_QUERY_HANDLERS: `${P}/AssetQuery/McpAutomationBridge_AssetQueryHandlers.cpp`,
  ASSET_MATERIAL_PINS: `${P}/AssetWorkflow/Materials/McpAutomationBridge_AssetWorkflowMaterialPinConnections.cpp`,
  ASSET_MATERIAL_BREAKING: `${P}/AssetWorkflow/Materials/McpAutomationBridge_AssetWorkflowMaterialConnectionBreaking.cpp`,
  ASSET_MATERIAL_MAINTENANCE: `${P}/AssetWorkflow/Materials/McpAutomationBridge_AssetWorkflowMaterialGraphMaintenance.cpp`,
} as const;

/** Literal native idiom used by all three material_overlap_residual constituent files. */
const MAT = (route: string): string =>
  `Lower.Equals(TEXT("${route}"), ESearchCase::IgnoreCase)`;

/**
 * Source-backed citations for the material_overlap_residual group row: one
 * per constituent symbol, each paired with its own concrete Materials/ cpp.
 */
export const MATERIAL_OVERLAP_CITATIONS: readonly {
  readonly source: string;
  readonly symbol: string;
}[] = [
  { source: ROUTE_EVIDENCE_PATHS.ASSET_MATERIAL_PINS, symbol: MAT('connect_material_pins') },
  { source: ROUTE_EVIDENCE_PATHS.ASSET_MATERIAL_BREAKING, symbol: MAT('break_material_connections') },
  { source: ROUTE_EVIDENCE_PATHS.ASSET_MATERIAL_MAINTENANCE, symbol: MAT('rebuild_material') },
];
