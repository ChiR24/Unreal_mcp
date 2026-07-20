/**
 * Widget animation records (4): create_widget_animation, add_animation_track,
 * add_animation_keyframe, set_animation_loop.
 *
 * Widget animations are distinct from Blueprint graph animations: they
 * animate UMG widget properties (transform, color, opacity, material) over
 * time within a Widget Blueprint's animation timeline. The native route
 * `set_animation_speed` (route disposition: remove) is a documented no-op:
 * it returns success and echoes the speed value but does not apply it at
 * design time (no SetPlayRate call).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-animation';
const DOMAIN = 'widget';

export const WIDGET_ANIMATION_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.create_widget_animation',
    action: 'create_widget_animation',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Create a new widget animation timeline in a Widget Blueprint.',
    whenToUse: ['A new UMG widget animation must be created for property keyframing.'],
    whenNotToUse: ['A Blueprint graph animation is needed (use animation_physics).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, animationName: P.animationName },
    required: ['action', 'widgetPath', 'animationName'],
    outputProps: { animationName: P.animationName },
    outputRequired: ['animationName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'create_widget_animation', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn' },
    exampleOutput: { success: true, animationName: 'FadeIn' },
  }),
  buildRecord({
    id: 'blueprint.add_animation_track',
    action: 'add_animation_track',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a property track (transform, color, opacity, material) to a widget animation.',
    whenToUse: ['A property track must be added to a widget animation for keyframing.'],
    whenNotToUse: ['The animation has enough tracks.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, animationName: P.animationName, trackType: P.trackType, slotName: P.slotName },
    required: ['action', 'widgetPath', 'animationName', 'trackType'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_animation_track', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn', trackType: 'opacity', slotName: 'Widget_Text' },
    exampleOutput: { success: true, message: 'Animation track added' },
  }),
  buildRecord({
    id: 'blueprint.add_animation_keyframe',
    action: 'add_animation_keyframe',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a keyframe at a specific time on a widget animation track.',
    whenToUse: ['A property value must be keyframed at a specific time in a widget animation.'],
    whenNotToUse: ['The track should be removed rather than keyframed.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, animationName: P.animationName, trackType: P.trackType, slotName: P.slotName, time: P.time, propertyValue: P.propertyValue, interpolation: P.interpolation, value: P.value },
    required: ['action', 'widgetPath', 'animationName', 'time'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_animation_keyframe', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn', trackType: 'opacity', slotName: 'Widget_Text', time: 0, propertyValue: 1, interpolation: 'linear' },
    exampleOutput: { success: true, message: 'Keyframe added' },
  }),
  buildRecord({
    id: 'blueprint.set_animation_loop',
    action: 'set_animation_loop',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the loop count and play mode for a widget animation.',
    whenToUse: ['A widget animation must loop a specified number of times.'],
    whenNotToUse: ['The animation should play once.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, animationName: P.animationName, loopCount: P.loopCount, playMode: P.playMode },
    required: ['action', 'widgetPath', 'animationName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_animation_loop', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'Pulse', loopCount: -1, playMode: 'pingpong' },
    exampleOutput: { success: true, message: 'Animation loop set' },
  }),
];
