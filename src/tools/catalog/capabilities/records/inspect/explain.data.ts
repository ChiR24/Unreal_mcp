/**
 * Explain records (6 actions): why two actors do not collide, why an actor is not seen, why a key does nothing, why a
 * widget is not on screen, why a sound is not heard, why a light lights nothing. Each walks an ordered fault tree and
 * answers the first cause with the call that fixes it.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const D = 'inspect';
const F = 'explain';
const REPLY = 'The reply gives verdict (failed, unknown when a check could not be made, or ok), cause (the first failed check: check, code, detail and, when one fixes it, nextCall), causes (every failed check) and checks (each check made, in order, passed, failed or unknown). Nothing is changed.';

export const EXPLAIN_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_collision', dispatchAction: 'explain_collision', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['why no overlap', 'overlap not firing', 'collision not working', 'hit event not firing', 'trigger does nothing'],
    summary: `Explain why actorName and otherActor do not overlap (expect overlap, the default) or hit each other (expect hit), from what the engine uses to decide: collision turned off on an actor (ACTOR_COLLISION_DISABLED), no component taking part in collision queries (NO_COLLIDING_COMPONENT), a static mesh with no collision shape (NO_COLLISION_SHAPE), a channel response on either side that ignores the other's object type (RESPONSE_IGNORES; a hit needs Block both ways, RESPONSE_DOES_NOT_BLOCK), two blockers that raise hits rather than overlaps (BLOCKS_INSTEAD), Generate Overlap Events off on a side (OVERLAP_EVENTS_OFF), a simulating body without hit events (HIT_EVENTS_OFF), one told to ignore the other (IGNORED_WHEN_MOVING), and whether they touch now. Reads the running game during Play, the editor level otherwise. ${REPLY}`,
    whenToUse: ['An overlap or hit event between two actors never fires.', 'A trigger volume does nothing when something walks into it.'],
    whenNotToUse: ['The collision settings must be changed (use control_actor set_actor_collision, the nextCall the cause names).'],
    inputProps: {
      actorName: { type: 'string', description: 'The first actor, by label, name or path.' },
      otherActor: { type: 'string', description: 'The second actor, by label, name or path.' },
      expect: { type: 'string', enum: ['overlap', 'hit'], description: 'The event that should happen: overlap (default) or hit.' },
    },
    required: ['actorName', 'otherActor'],
    effect: 'read',
    exampleInput: { action: 'explain_collision', actorName: 'TriggerBox_Door', otherActor: 'BP_Player', expect: 'overlap' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_visible', dispatchAction: 'explain_visible', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['why invisible', 'why is actor invisible', 'actor not showing', 'cannot see actor', 'mesh not rendering', 'object disappeared'],
    summary: `Explain why actorName is not seen, from the player's camera during Play or the editor viewport otherwise: hidden in the game or in the editor (HIDDEN_IN_GAME, HIDDEN_IN_EDITOR), editor-only during Play (EDITOR_ONLY), its streaming level not visible (LEVEL_HIDDEN), no component that draws (NOTHING_TO_DRAW: each hidden, unregistered, or a static mesh component with no mesh), beyond a component's draw distance (CULLED_BY_DISTANCE), nearer than the near clip plane (INSIDE_NEAR_CLIP), outside the camera's field of view (OUT_OF_VIEW), drawn only for its owner or hidden from its owner (ONLY_OWNER_SEES, OWNER_NO_SEE), or buried in the surface under it (BURIED, with the location that rests it on top). The reply names the view it judged from (viewFrom). ${REPLY}`,
    whenToUse: ['A placed actor does not appear in the viewport or in Play.'],
    whenNotToUse: ['The actor should be found first; use inspect query_object or the outliner.'],
    inputProps: {
      actorName: { type: 'string', description: 'The actor that should be seen, by label, name or path.' },
    },
    required: ['actorName'],
    effect: 'read',
    exampleInput: { action: 'explain_visible', actorName: 'SM_Crate_12' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_input', dispatchAction: 'explain_input', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['key does nothing', 'input not working', 'button press ignored', 'jump key dead', 'enhanced input not firing'],
    summary: `Explain why the Enhanced Input action at actionPath (pressed with key, when given) does nothing: no mapping context under /Game maps it (NOT_MAPPED), no Add Mapping Context node adds such a context in the game mode's player controller or pawn Blueprints, or during Play the player has none of them added (CONTEXT_NOT_ADDED: every key stays dead), and no event for the action in those Blueprints (NOT_HANDLED); during Play also no possessed pawn (NO_PAWN), a paused game (GAME_PAUSED) and a widget holding keyboard focus (WIDGET_HAS_FOCUS). A missing action is ACTION_NOT_FOUND. ${REPLY}`,
    whenToUse: ['A key or button press does nothing in Play.', 'An input action event never fires.'],
    whenNotToUse: ['Legacy (Project Settings) input mappings: only Enhanced Input is explained.'],
    inputProps: {
      actionPath: { type: 'string', description: 'The Input Action asset the key should fire, e.g. /Game/Input/IA_Jump.' },
      key: { type: 'string', description: 'The key pressed, by engine key name (SpaceBar, E, Gamepad_FaceButton_Bottom); omitted, any key mapped to the action counts.' },
    },
    required: ['actionPath'],
    effect: 'read',
    exampleInput: { action: 'explain_input', actionPath: '/Game/Input/IA_Jump', key: 'SpaceBar' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_widget', dispatchAction: 'explain_widget', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['widget not showing', 'hud not visible', 'ui does not appear', 'menu not on screen'],
    summary: `Explain why the Widget Blueprint at widgetPath is not on screen. During Play: no instance exists (NOT_CREATED), one exists but was never added to the viewport or to another widget (NOT_IN_VIEWPORT), its visibility is Collapsed or Hidden (HIDDEN), its render opacity is 0 (TRANSPARENT), or its desired size is 0 by 0 (ZERO_SIZE). Outside Play: no Create Widget node for it in the game mode's player controller, pawn or HUD Blueprint or the level's own (NOT_CREATED_ANYWHERE), or the Blueprint that creates it never adds it to the viewport (NOT_ADDED); whether it is on screen is then unknown until Play. A missing Widget Blueprint is WIDGET_NOT_FOUND. ${REPLY}`,
    whenToUse: ['A HUD, menu or other widget never appears.'],
    whenNotToUse: ['The widget appears but its layout is wrong (use manage_blueprint get_widget_info to read its tree).'],
    inputProps: {
      widgetPath: { type: 'string', description: 'The Widget Blueprint that should show, e.g. /Game/UI/WBP_HUD.' },
    },
    required: ['widgetPath'],
    effect: 'read',
    exampleInput: { action: 'explain_widget', widgetPath: '/Game/UI/WBP_HUD' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_audible', dispatchAction: 'explain_audible', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['sound not playing', 'cannot hear sound', 'no audio', 'ambient sound silent'],
    summary: `Explain why actorName's sound is not heard: no audio component (NO_AUDIO_COMPONENT), no sound on it (NO_SOUND), not playing during Play (NOT_PLAYING) or not starting by itself (NOT_AUTO_ACTIVATED), a volume multiplier of 0 (MUTED), or the listener beyond its attenuation range (OUT_OF_RANGE: inner radius plus falloff). The listener is the player's during Play (often the camera, far from the pawn in a side view) and the editor viewport otherwise (listener in the reply). ${REPLY}`,
    whenToUse: ['A placed sound or an actor\'s audio is silent.'],
    whenNotToUse: ['The sound must be changed or started (use manage_audio).'],
    inputProps: {
      actorName: { type: 'string', description: 'The actor whose sound should be heard, by label, name or path.' },
    },
    required: ['actorName'],
    effect: 'read',
    exampleInput: { action: 'explain_audible', actorName: 'AmbientSound_Fire' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'explain_lit', dispatchAction: 'explain_lit', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['light not working', 'light does not light', 'actor too dark', 'lighting has no effect'],
    summary: `Explain why the light actorName lights nothing, or does not light targetActor: hidden or not affecting the world (LIGHT_OFF), an intensity of 0 (NO_INTENSITY), a Static light with lighting unbuilt (UNBUILT_LIGHTING), and against targetActor: beyond the attenuation radius (OUT_OF_RANGE, with the radius that reaches it), outside a spot light's outer cone (OUTSIDE_CONE), or no lighting channel in common (CHANNEL_MISMATCH). A light not found is LIGHT_NOT_FOUND. Auto exposure can also hide a lighting change: look with a fixed exposure before judging by eye. ${REPLY}`,
    whenToUse: ['A light seems to have no effect on the scene or on one actor.'],
    whenNotToUse: ['The light must be created or tuned (use build_environment lighting actions).'],
    inputProps: {
      actorName: { type: 'string', description: 'The light actor, by label, name or path.' },
      targetActor: { type: 'string', description: 'The actor the light should reach; omitted, only the light itself is checked.' },
    },
    required: ['actorName'],
    effect: 'read',
    exampleInput: { action: 'explain_lit', actorName: 'PointLight_Lamp', targetActor: 'SM_Table' },
  }),
];
