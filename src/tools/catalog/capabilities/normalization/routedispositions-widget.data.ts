/**
 * Widget route dispositions (C20 / O38).
 *
 * 18 unowned widget extras with real native bodies but dead-from-MCP
 * (promote), plus 5 named routes that are raw or no-op (map/remove).
 * Every row cites a concrete .cpp file (v2 source-derived).
 */
import { type RawRouteDisposition, ROUTE_EVIDENCE_PATHS, WIDGET_UNOWNED_FILES, WIDGET_UNOWNED_PROMOTE } from './routedispositions-paths.js';

const { WIDGET_UI, WIDGET_ANIM, WIDGET_STYLING, WIDGET_CREATION, ANIM_AUTHORING_INFO } = ROUTE_EVIDENCE_PATHS;

export const WIDGET_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  ...WIDGET_UNOWNED_PROMOTE.map((route) => ({
    key: `route:widget:${route}`,
    route,
    domain: 'widget',
    status: 'dead' as const,
    owner: 'WidgetAuthoring',
    evidenceSource: WIDGET_UNOWNED_FILES[route],
    evidenceSymbol: `SubAction.Equals(TEXT("${route}"), ESearchCase::IgnoreCase)`,
    evidenceTool: 'manage_blueprint',
    disposition: 'promote' as const,
    targetCanonicalId: `cap:manage_blueprint:${route}`,
    rationale:
      'C20/O38: implemented native WidgetAuthoring route not registered on any canonical MCP parent (dead-from-MCP); promote to manage_blueprint widget surface.',
  })),
  {
    key: 'route:widget:create_widget',
    route: 'create_widget',
    domain: 'widget',
    status: 'raw',
    owner: 'Ui/WidgetAuthoring',
    evidenceSource: WIDGET_UI,
    evidenceSymbol: 'LowerSub == TEXT("create_widget")',
    evidenceTool: 'system_control',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_blueprint:create_widget_blueprint',
    rationale:
      'C20/O38: alias of create_widget_blueprint; reachable via system_control; map to the canonical blueprint creation id.',
  },
  {
    key: 'route:widget:get_animation_info',
    route: 'get_animation_info',
    domain: 'widget',
    status: 'raw',
    owner: 'AnimationAuthoring',
    evidenceSource: ANIM_AUTHORING_INFO,
    evidenceSymbol: 'SubAction == TEXT("get_animation_info")',
    evidenceTool: 'animation_physics',
    disposition: 'map',
    targetCanonicalId: 'cap:animation_physics:get_animation_info',
    rationale:
      'C20/O38: cross-parent owned read-only query; reachable via animation_physics; map to canonical id.',
  },
  {
    key: 'route:widget:show_widget',
    route: 'show_widget',
    domain: 'widget',
    status: 'raw',
    owner: 'WidgetAuthoring',
    evidenceSource: WIDGET_CREATION,
    evidenceSymbol: 'SubAction.Equals(TEXT("show_widget"), ESearchCase::IgnoreCase)',
    evidenceTool: 'system_control',
    disposition: 'map',
    targetCanonicalId: 'cap:system_control:show_widget',
    rationale:
      'C20/O38: cross-parent owned real mutation; reachable via system_control; map to canonical id.',
  },
  {
    key: 'route:widget:apply_style_to_widget',
    route: 'apply_style_to_widget',
    domain: 'widget',
    status: 'raw',
    owner: 'WidgetAuthoring',
    evidenceSource: WIDGET_STYLING,
    evidenceSymbol: 'SubAction.Equals(TEXT("apply_style_to_widget"), ESearchCase::IgnoreCase)',
    evidenceTool: 'manage_blueprint',
    disposition: 'remove',
    removalGuidance:
      'C20/O38: body-proven no-op; returns success without applying the named style (runtime binding setup required). Remove or implement real styling.',
    rationale:
      'C20/O38: success reported but TargetWidget style not mutated (no SetStyle/ApplyStyle).',
  },
  {
    key: 'route:widget:set_animation_speed',
    route: 'set_animation_speed',
    domain: 'widget',
    status: 'raw',
    owner: 'WidgetAuthoring',
    evidenceSource: WIDGET_ANIM,
    evidenceSymbol: 'SubAction.Equals(TEXT("set_animation_speed"), ESearchCase::IgnoreCase)',
    evidenceTool: 'manage_blueprint',
    disposition: 'remove',
    removalGuidance:
      'C20/O38: body-proven no-op; returns success without applying the named playback speed (no SetPlayRate/SetPlaybackSpeed). Remove or implement real speed.',
    rationale:
      'C20/O38: speed echoed back but not applied to the target widget.',
  },
];
