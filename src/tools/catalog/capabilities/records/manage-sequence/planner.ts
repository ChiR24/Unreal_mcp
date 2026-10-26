/**
 * Cinematic planner records: cameras placed from what the shot must show rather than from coordinates.
 * Native Domains/Sequence/Planner (McpFrameShot*).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';

const F = 'planner';
const D = 'cinematics';

const screenPoint = { type: 'array', items: { type: 'number' }, description: 'A screen point as [x, y]: x -1 (left edge) to 1 (right edge), y -1 (bottom) to 1 (top).' };

export const PLANNER_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.planner.frame_shot', action: 'frame_shot', family: F, domain: D,
    // "frame" and "shot" stay together in the topics: alone, "shot" is a sequence's shot track and "frame" a keyframe.
    topics: ['frame a shot', 'compose a shot', 'shot composition', 'rule of thirds', 'place the camera to show', 'camera framing',
      'two shot', 'aim a camera at two actors', 'camera that shows the character'],
    summary: 'Place a camera so one or two actors sit where they should on screen: by default one subject on the left third line, two on the left and right third lines (framing gives each its own screen point). It searches the places around the subjects for the best view: every subject clear of what stands between (each sight line to it traced), the camera out of walls and floors, seen a little from above, a lone subject three-quarter on from its front and turned into the frame; the camera never rolls. One subject is sized by shotSize (close: its box spans 90% of the frame; medium: 55%; wide: 25%); two are sized by how far apart their screen points are, and kept about as far from the camera as each other (a two-shot, not one subject looming over the other). Moves the camera actor named camera (default ShotCamera), or spawns a CineCameraActor under that label when no actor has it, and sets its focal length (focalLength; else 85, 50 or 24 mm by shot size, 35 mm for two subjects) and its manual focus on the first subject, all in one undo step. dryRun answers the same without moving or spawning anything. Each subject comes back with its target, where it landed (screen) and how hidden it is. Refused, changing nothing, with ACTOR_NOT_FOUND (a subject is not in the level; missing lists it), NO_CLEAR_SHOT (no place found sees every subject at least half clear with the camera out of solid geometry: the best one tried is in the details), INVALID_ARGUMENT (a subject that draws nothing, framing points that do not fit the subjects, focalLength on a plain camera, a camera actor with no camera component).',
    whenToUse: [
      'A camera must show one or two actors well composed (on the thirds, a two-shot of a conversation) without working out where it stands and where it points.',
      'Setting up the camera of a cinematic shot or a screenshot; a Level Sequence can then cut to it (add_camera_cut_track).',
      'Checking whether an actor can be seen clearly at all from around it: dryRun answers NO_CLEAR_SHOT when nothing can.',
    ],
    whenNotToUse: [
      'Moving the editor viewport rather than a camera actor: use control_editor set_camera or focus_actor.',
      'Animating a camera over time: key its transform in a Level Sequence (add_keyframe).',
      'Changing only a camera\'s lens, focus or filmback: use configure_cinematic setting=camera.',
    ],
    inputProps: {
      subjects: { type: 'array', minItems: 1, maxItems: 2, items: { type: 'string' }, description: 'The one or two actors to frame, by exact name. Each is framed by the box of what it draws: its centre goes to its framing point, and its size sets the distance.' },
      framing: {
        type: 'array', minItems: 1, maxItems: 2,
        items: { type: 'object', additionalProperties: false, required: ['x', 'y'], properties: {
          x: { type: 'number', minimum: -1, maximum: 1, description: 'Left edge -1, centre 0, right edge 1; the third lines are at -0.333 and 0.333.' },
          y: { type: 'number', minimum: -1, maximum: 1, description: 'Bottom -1, centre 0, top 1.' },
        } },
        description: 'Where each subject\'s centre should sit on screen, one {x, y} per subject in the order of subjects (default: one subject at {x: -0.333, y: 0}, two at {x: -0.333, y: 0} and {x: 0.333, y: 0}). Two subjects need points apart from each other.',
      },
      shotSize: { type: 'string', enum: ['close', 'medium', 'wide'], description: 'One subject: how much of the frame its box spans, the larger of its height share and its width share (close 90%, medium 55%, the default, wide 25%); it also picks the default focal length. Two subjects are sized by their framing points, so it only picks the lens.' },
      focalLength: { type: 'number', minimum: 4, maximum: 1000, description: 'Focal length in mm for a cine camera (default by shotSize: close 85, medium 50, wide 24; two subjects 35), kept within what its lens allows. A plain camera actor has no lens: its own field of view is used and focalLength is refused.' },
      camera: { type: 'string', description: 'The camera actor to move (a CineCameraActor or a CameraActor), by name; when no actor has this name a CineCameraActor is spawned with it as its label (default ShotCamera, so repeated calls move the same camera).' },
      dryRun: { type: 'boolean', description: 'Find the shot and answer without moving or spawning anything (default false).' },
    },
    required: ['subjects'],
    outputProps: {
      camera: { type: 'string', description: 'The camera\'s label.' },
      spawned: { type: 'boolean', description: 'The camera was spawned by this call (with dryRun: would be).' },
      location: { type: 'array', items: { type: 'number' }, description: 'Where the camera stands, as [x, y, z].' },
      rotation: { type: 'array', items: { type: 'number' }, description: 'Its rotation as [pitch, yaw, roll]; roll is always 0.' },
      fieldOfView: { type: 'number', description: 'Horizontal field of view in degrees.' },
      focalLength: { type: 'number', description: 'Cine camera: the focal length set, in mm.' },
      focusDistance: { type: 'number', description: 'Cine camera: the manual focus distance set, in cm (to the first subject\'s centre).' },
      subjects: { type: 'array', description: 'Each subject as framed.', items: { type: 'object', additionalProperties: false, properties: {
        actorName: { type: 'string' },
        target: screenPoint,
        screen: { ...screenPoint, description: 'Where its centre landed, as [x, y].' },
        error: { type: 'number', description: 'How far that is from its target, in the same units (0.02 is about 1% of the frame\'s width).' },
        hidden: { type: 'number', description: 'Share of the sight lines from the camera to it that something else blocks (0-1).' },
      } } },
      fill: { type: 'number', description: 'One subject: the share of the frame its box spans.' },
      dryRun: { type: 'boolean' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}; the move or spawn and the lens are one step. Absent under dryRun.' },
    },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: [],
    exampleInput: { action: 'frame_shot', subjects: ['Hero'], shotSize: 'medium' },
    exampleOutput: { success: true, message: 'Spawned ShotCamera to frame Hero on its mark from 520 cm with a 50 mm lens; nothing in the way.', camera: 'ShotCamera', spawned: true, location: [452.3, 261.1, 182.4], rotation: [-7.15, -150.0, 0], fieldOfView: 26.6, focalLength: 50, focusDistance: 524.8, subjects: [{ actorName: 'Hero', target: [-0.333, 0], screen: [-0.333, 0], error: 0, hidden: 0 }], fill: 0.55, dryRun: false },
  }),
];
