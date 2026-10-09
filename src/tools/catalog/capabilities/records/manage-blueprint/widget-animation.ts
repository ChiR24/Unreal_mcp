/**
 * Widget animations are distinct from Blueprint graph animations: they
 * animate UMG widget properties (transform, color, opacity, material) over
 * time within a Widget Blueprint's animation timeline. The native route
 * `set_animation_speed` (route disposition: remove) is a documented no-op:
 * it returns success and echoes the speed value but does not apply it at
 * design time (no SetPlayRate call).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-animation';
const DOMAIN = 'widget';

// What add_animation_track and add_animation_keyframe answer about the track they touched.
const TRACK_OUTPUT = {
  widgetPath: P.widgetPath,
  animationName: P.animationName,
  slotName: { type: 'string', description: 'Widget the track animates.' },
  trackType: { type: 'string', description: 'Normalised track type.' },
  propertyName: { type: 'string', description: 'Widget property driven by the track (RenderOpacity, ColorAndOpacity, RenderTransform).' },
  trackClass: { type: 'string', description: 'MovieScene track class that owns the section.' },
  createdTrack: { type: 'boolean', description: 'Whether the property track was created by this call.' },
  createdBinding: { type: 'boolean', description: 'Whether the widget binding was created by this call.' },
  bindingGuid: { type: 'string', description: 'MovieScene possessable GUID bound to the widget.' },
  saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
} as const;

export const WIDGET_ANIMATION_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.create_widget_animation',
    action: 'create_widget_animation',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Create a new widget animation timeline in a Widget Blueprint.',
    whenToUse: ['A new UMG widget animation must be created for property keyframing.'],
    whenNotToUse: [
      'A Blueprint graph animation is needed (use animation_physics).',
      'An animation must be seen playing in PIE: write the value that sets it off with inspect.set_property and watch (a short pop ends before a later call can read it).',
    ],
    inputProps: { widgetPath: P.widgetPath, animationName: P.animationName, duration: P.duration },
    required: ['widgetPath', 'animationName'],
    outputProps: {
      animationName: P.animationName,
      widgetPath: P.widgetPath,
      duration: { type: 'number', description: 'Length of the new animation in seconds.' },
    },
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
    inputProps: { widgetPath: P.widgetPath, animationName: P.animationName, trackType: P.trackType, slotName: P.slotName },
    required: ['widgetPath', 'animationName', 'trackType'],
    outputProps: TRACK_OUTPUT,
    outputRequired: [],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_animation_track', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn', trackType: 'opacity', slotName: 'Widget_Text' },
  }),
  buildRecord({
    id: 'blueprint.add_animation_keyframe',
    action: 'add_animation_keyframe',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a keyframe at a specific time on a widget animation track, or a whole animation across several widgets in one call (keys).',
    whenToUse: [
      'A property value must be keyframed at a specific time in a widget animation.',
      'A whole widget animation (a logo popping in, a menu sliding up, a pulse) must be keyed in one call (keys).',
    ],
    whenNotToUse: ['The track should be removed rather than keyframed.'],
    inputProps: { widgetPath: P.widgetPath, animationName: P.animationName, trackType: P.trackType, slotName: P.slotName, time: P.time,
      propertyValue: { description: 'The key value, by trackType: opacity a number 0-1; color {r,g,b,a} or [r,g,b,a]; translation, scale or shear {x,y} or [x,y]; angle a number in degrees; transform any of {translation:{x,y}, scale:{x,y}, angle, shear:{x,y}}. A value of the wrong shape is refused before anything is added.' },
      interpolation: P.interpolation,
      value: { type: 'number', description: 'A plain number for an opacity or angle key, in place of propertyValue.' },
      keys: {
        type: 'array', minItems: 1, maxItems: 200,
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        'x-unreal-reflection-boundary': true,
        description: 'Many keyframes in one call, in place of time: each {time, propertyValue or value, and any of slotName, trackType, interpolation that differ from the call\'s own}. Every key is checked before any is written: a refused one is named by its index (keys[3]) and nothing is added. The Widget Blueprint is saved once.',
      } },
    required: ['widgetPath', 'animationName'],
    outputProps: {
      ...TRACK_OUTPUT,
      keysAdded: { type: 'number', description: 'With keys: how many keyframes were added; the other fields describe the last one.' },
      time: { type: 'number', description: 'Key time in seconds.' },
      frameNumber: { type: 'number', description: 'Key position in MovieScene tick-resolution frames.' },
      keyCount: { type: 'number', description: 'Keys on the last channel written after this call.' },
      channelCount: { type: 'number', description: 'Channels that received a key.' },
    },
    outputRequired: ['animationName', 'slotName', 'propertyName', 'keyCount'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_animation_keyframe', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn', trackType: 'opacity', slotName: 'Widget_Text', time: 0, propertyValue: 1, interpolation: 'linear' },
    exampleOutput: { success: true, message: 'Keyframe added at 0.500s on Ammo.RenderOpacity (1 key in the track)', animationName: 'FadeIn', slotName: 'Ammo', trackType: 'opacity', propertyName: 'RenderOpacity', keyCount: 1, channelCount: 1, createdTrack: true, createdBinding: true, saved: true },
  }),
  buildRecord({
    id: 'blueprint.delete_animation',
    action: 'delete_animation',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Delete a named animation from a Widget Blueprint.',
    whenToUse: ['An animation is obsolete and must be removed from the Widget Blueprint.'],
    whenNotToUse: ['The animation should only stop playing (change its loop or play mode instead).'],
    inputProps: { widgetPath: P.widgetPath, animationName: P.animationName },
    required: ['widgetPath', 'animationName'],
    outputProps: {
      widgetPath: P.widgetPath,
      deletedAnimation: { type: 'string', description: 'Name of the animation that was removed.' },
      remainingAnimations: { type: 'number', description: 'Number of animations left on the Widget Blueprint.' },
    },
    outputRequired: ['widgetPath', 'deletedAnimation', 'remainingAnimations'],
    effect: 'destructive',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'delete_animation', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'Pulse' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', deletedAnimation: 'Pulse', remainingAnimations: 2 },
  }),
  // A widget animation could be built key by key but read back by name only.
  buildRecord({
    id: 'blueprint.get_widget_animation',
    action: 'get_widget_animation',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['read widget animation', 'widget animation keys', 'check ui animation', 'animation tracks of a widget'],
    summary: 'Read a Widget Blueprint\'s animations: without animationName, each one\'s length and track count; with it, every widget the animation drives and each track\'s keys per channel (translation.x, angle, scale.x, r, value ...) as [seconds, value] pairs, the names add_animation_keyframe takes.',
    whenToUse: ['An animation built with add_animation_keyframe must be checked: which widgets it moves, when, and to what values.', 'An existing menu or HUD animation must be understood before it is changed.'],
    whenNotToUse: ['Only the animation names or the widget tree are needed (use get_widget_info).'],
    inputProps: { widgetPath: P.widgetPath, animationName: { ...P.animationName, description: 'The animation to read in full; omitted, every animation is listed with its timing.' } },
    required: ['widgetPath'],
    outputProps: {
      animations: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Without animationName: each animation with name, durationSeconds and trackCount.' },
      durationSeconds: { type: 'number', description: 'With animationName: the animation\'s length in seconds.' },
      trackCount: { type: 'number', description: 'With animationName: tracks in the animation, those on its widgets included.' },
      widgets: {
        type: 'array',
        description: 'With animationName: each widget it drives, by its name in the widget tree.',
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: '{widget, tracks: [{property (RenderTransform, RenderOpacity, ColorAndOpacity ...), kind (transform, float, color, or the track class), channels: {name: [[seconds, value], ...]}}]}; at most 100 keys a channel.' },
      },
    },
    outputRequired: [],
    effect: 'read',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'get_widget_animation', widgetPath: '/Game/UI/WBP_MainUI', animationName: 'FadeIn' },
    exampleOutput: { success: true, durationSeconds: 0.5, trackCount: 1, widgets: [{ widget: 'Title', tracks: [{ property: 'RenderOpacity', kind: 'float', channels: { value: [[0, 0], [0.5, 1]] } }] }] },
  }),
];
