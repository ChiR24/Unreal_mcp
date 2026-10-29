/**
 * Viewport display records: set_view_mode, set_viewport_resolution,
 * set_viewport_realtime, set_editor_mode, set_immersive_mode,
 * set_game_view, show_stats, hide_stats, set_show_flags.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'viewport';
const D = 'editor';

export const VIEWPORT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_view_mode', domain: D, family: F,
    summary: 'Set the rendering view mode of the view on screen (the level viewport, or the running game\'s view while the player plays: method game_viewport) by any engine view mode name, its enum name or display name with case, spaces and underscores ignored: Lit, Unlit, Wireframe, DetailLighting, LightingOnly, LightComplexity, ShaderComplexity, LightmapDensity, ReflectionOverride, PathTracing, RayTracingDebug, CollisionPawn, CollisionVisibility, LODColoration, VisualizeNanite, VisualizeLumen, VisualizeVirtualShadowMap and the rest of this engine version\'s list. An unknown name is refused with that list; with no level viewport open, NO_VIEWPORT.',
    whenToUse: ['The viewport render mode must be changed for debugging or visualization.'],
    whenNotToUse: ['The default Lit mode is acceptable.'],
    inputProps: { viewMode: P.viewMode },
    required: ['viewMode'],
    effect: 'read',
   
    exampleInput: { action: 'set_view_mode', viewMode: 'Wireframe' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_viewport_resolution', dispatchAction: 'console_command',
    domain: D, family: F,
    summary: 'Set the game window resolution through the console (r.SetRes <width>x<height>, fractions dropped); it does not resize an editor viewport. The reply is the console command\'s: command and output.',
    whenToUse: ['The viewport resolution must be changed.'],
    whenNotToUse: ['The default resolution is acceptable.'],
    inputProps: { width: P.width, height: P.height },
    required: ['width', 'height'],
    effect: 'read',
   
    exampleInput: { action: 'set_viewport_resolution', width: 1920, height: 1080 },
    exampleOutput: { success: true, message: 'Command executed: r.SetRes 1920x1080' },
    outputProps: { command: { type: 'string', description: 'The console command that ran.' }, output: { type: 'string', description: 'What the command printed, at most 4096 characters.' } },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_viewport_realtime', domain: D, family: F,
    summary: 'Toggle realtime rendering in the editor viewport: particles and animated materials move only in a realtime viewport. When another system holds the viewport off realtime the request is put on top of it (overrode names it); while the editor is in the background with Use Less CPU when in Background on, realtime stays off (heldBy, realtime false) until control_editor configure_editor setting=window unthrottle lifts the throttle.',
    whenToUse: ['Realtime rendering must be enabled or disabled.', 'A screenshot warned that the level viewport is not realtime, so effects stood still in it.'],
    whenNotToUse: ['The current realtime state is acceptable.'],
    inputProps: { enabled: P.enabled, realtime: P.realtime },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_viewport_realtime', enabled: false },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_editor_mode', domain: D, family: F,
    summary: 'Activate an editor mode by id (EM_Landscape, EM_Foliage, EM_MeshPaint, EM_Default) and confirm it is active.',
    whenToUse: ['A specific editor mode must be activated.'],
    whenNotToUse: ['The default mode is acceptable.'],
    inputProps: { mode: P.mode },
    required: ['mode'],
    effect: 'read',
   
    exampleInput: { action: 'set_editor_mode', mode: 'landscape' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_immersive_mode', domain: D, family: F,
    summary: 'Turn immersive viewport mode on or off (fails with NO_VIEWPORT when no level viewport is open).',
    whenToUse: ['The viewport must enter or exit immersive fullscreen.'],
    whenNotToUse: ['The current immersive state is acceptable.'],
    inputProps: { enabled: P.enabled },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_immersive_mode', enabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_game_view', domain: D, family: F,
    summary: 'Toggle game view (no editor icons) in the level viewport, or in the free view of an ejected player.',
    whenToUse: ['The viewport must show or hide the game view.'],
    whenNotToUse: ['The current game view state is acceptable.',
      'Play In Editor runs and the player is not ejected: the game on screen draws no editor icons anyway, so this is refused with PIE_VIEW_NOT_EJECTED.'],
    inputProps: { enabled: P.enabled },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_game_view', enabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'show_stats', domain: D, family: F,
    summary: 'Show a stat overlay in the viewport (FPS and Unit when no stat is named); a stat already shown stays shown.',
    whenToUse: ['A debug stat must be displayed.'],
    whenNotToUse: ['The stat is already shown.'],
    inputProps: { stat: P.stat },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'show_stats', stat: 'fps' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'hide_stats', domain: D, family: F,
    summary: 'Hide one stat overlay, or every stat when none is named; a stat already hidden stays hidden.',
    whenToUse: ['A debug stat must be hidden.'],
    whenNotToUse: ['The stat is already hidden.'],
    inputProps: { stat: P.stat },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'hide_stats', stat: 'fps' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_show_flags', domain: D, family: F,
    summary: 'Show or hide parts of the level viewport by the engine\'s show flag names, as its Show menu does: flags {"Grid": false, "Collision": true}, with names such as Grid, Collision, Bounds, Navigation, Fog, PostProcessing, Lighting, Particles, Decals, Volumes, Splines, Landscape, Foliage, StaticMeshes, SkeletalMeshes, Translucency, Atmosphere and BillboardSprites (the editor icons). Each flag is read back (flags) with how many changed; a name that is no show flag refuses the whole call and changes nothing (INVALID_ARGUMENT, naming it). Refused while the player plays and is not ejected (PIE_VIEW_NOT_EJECTED) and with no level viewport (NO_VIEWPORT).',
    whenToUse: ['The grid, collision shapes, bounds, the navigation mesh, fog or another layer of the viewport must be shown or hidden.'],
    whenNotToUse: ['The whole render mode must change (set_view_mode: Wireframe, Unlit, LightingOnly).', 'Every editor icon must go (set_game_view).'],
    inputProps: { flags: { type: 'object', additionalProperties: { type: 'boolean' }, description: 'Show flag names, each true (shown) or false (hidden): {"Grid": false}. Names as the engine has them, case ignored: Grid, Collision, Bounds, Navigation, Fog, PostProcessing, Lighting, Particles, Decals, Volumes, Splines, Landscape, Foliage, StaticMeshes, SkeletalMeshes, Translucency, Atmosphere, BillboardSprites and the rest.' } },
    required: ['flags'],
    effect: 'read',
    exampleInput: { action: 'set_show_flags', flags: { Grid: false, Collision: true } },
  }),
];
