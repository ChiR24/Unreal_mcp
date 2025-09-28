export interface PromptArgument {
  type: string;
  description?: string;
  enum?: string[];
  default?: unknown;
  required?: boolean;
}

export interface PromptTemplate {
  name: string;
  description: string;
  arguments?: Record<string, PromptArgument>;
  build: (args: Record<string, unknown>) => Array<{
    role: 'user' | 'assistant';
    content: { type: 'text'; text: string };
  }>;
}

function clampChoice(value: unknown, choices: string[], fallback: string): string {
  if (typeof value === 'string') {
    const normalized = value.toLowerCase();
    if (choices.includes(normalized)) {
      return normalized;
    }
  }
  return fallback;
}

function coerceNumber(value: unknown, fallback: number, min?: number, max?: number): number {
  const num = typeof value === 'number' ? value : Number(value);
  if (!Number.isFinite(num)) {
    return fallback;
  }
  if (min !== undefined && num < min) {
    return min;
  }
  if (max !== undefined && num > max) {
    return max;
  }
  return num;
}

function formatVector(value: unknown): string | null {
  if (!value || typeof value !== 'object') {
    return null;
  }
  const vector = value as Record<string, unknown>;
  const x = typeof vector.x === 'number' ? vector.x : Number(vector.x);
  const y = typeof vector.y === 'number' ? vector.y : Number(vector.y);
  const z = typeof vector.z === 'number' ? vector.z : Number(vector.z);
  if ([x, y, z].some((component) => !Number.isFinite(component))) {
    return null;
  }
  return `${x.toFixed(2)}, ${y.toFixed(2)}, ${z.toFixed(2)}`;
}

export const prompts: PromptTemplate[] = [
  {
    name: 'setup_three_point_lighting',
    description: 'Author a cinematic three-point lighting rig aligned to the active camera focus.',
    arguments: {
      intensity: {
        type: 'string',
        enum: ['low', 'medium', 'high'],
        default: 'medium',
        description: 'Overall lighting mood. Low = dramatic contrast, high = bright key light.'
      }
    },
    build: (args) => {
      const intensity = clampChoice(args.intensity, ['low', 'medium', 'high'], 'medium');
      const moodHints: Record<string, string> = {
        low: 'gentle key with strong contrast and subtle rim highlights',
        medium: 'balanced key/fill ratio for natural coverage',
        high: 'bright key with energetic fill and crisp rim separation'
      };

      const text = `Configure a three-point lighting rig around the current cinematic focus.

Tasks:
- Position a key light roughly 45° off-axis at eye level. Target the subject center and tune intensity for ${intensity} output (${moodHints[intensity]}).
- Add a fill light on the opposite side with wider spread and softened shadows to control contrast.
- Place a rim/back light to outline silhouettes and separate the subject from the background.
- Ensure all lights use physically plausible color temperature, enable shadow casting where helpful, and adjust attenuation to avoid spill.
- Once balanced, report the final intensity values, color temperatures, and any blockers encountered.`;

      return [{
        role: 'user',
        content: { type: 'text', text }
      }];
    }
  },
  {
    name: 'create_fps_controller',
    description: 'Spin up a first-person controller blueprint with input mappings, collision, and starter movement.',
    arguments: {
      spawnLocation: {
        type: 'vector',
        description: 'Optional XYZ spawn position for the player pawn.',
        required: false
      }
    },
    build: (args) => {
      const spawnVector = formatVector(args.spawnLocation);
      const spawnLine = spawnVector ? `Spawn the pawn at world coordinates (${spawnVector}).` : 'Spawn the pawn at a safe default player start or the origin.';

      const text = `Build a First Person Character blueprint with:
- Camera + arms mesh, basic WASD input, jump, crouch, and sprint bindings using Enhanced Input.
- Proper collision capsule sizing for a 180cm tall human.
- Momentum-preserving air control with configurable acceleration and friction.
- A configurable base turn rate with mouse sensitivity scaling.
- Serialized defaults for walking speed (600 uu/s) and sprint speed (900 uu/s).
- Expose key movement settings as editable defaults.
- ${spawnLine}

Finish by compiling, saving, and summarizing the created blueprint path plus the mapped input actions.`;

      return [{
        role: 'user',
        content: { type: 'text', text }
      }];
    }
  },
  {
    name: 'setup_post_processing',
    description: 'Author a post-process volume tuned to a named cinematic grade.',
    arguments: {
      style: {
        type: 'string',
        enum: ['cinematic', 'realistic', 'stylized', 'noir'],
        default: 'cinematic',
        description: 'Look preset to emphasize color grading and tone-mapping style.'
      }
    },
    build: (args) => {
      const style = clampChoice(args.style, ['cinematic', 'realistic', 'stylized', 'noir'], 'cinematic');
      const styleNotes: Record<string, string> = {
        cinematic: 'filmic tonemapper, gentle bloom, warm highlights, cool shadows, slight vignette',
        realistic: 'minimal grading, accurate white balance, restrained bloom, detail-preserving sharpening',
        stylized: 'bold saturation shifts, custom color LUT, exaggerated contrast, selective bloom',
        noir: 'monochrome conversion, strong contrast curve, subtle film grain, heavy vignette'
      };

      const text = `Create a global post-process volume with priority over level defaults.
- Apply the "${style}" look: ${styleNotes[style]}.
- Configure tone mapping, exposure, bloom, chromatic aberration, and LUTs as required.
- Ensure the volume is unbound unless level-specific constraints apply.
- Provide sanity checks for HDR output and keep auto-exposure transitions smooth.
- Summarize all modified settings with their final numeric values or asset references.`;

      return [{
        role: 'user',
        content: { type: 'text', text }
      }];
    }
  },
  {
    name: 'setup_dynamic_day_night_cycle',
    description: 'Create or update a Blueprint to drive a dynamic day/night cycle with optional weather hooks.',
    arguments: {
      startTime: {
        type: 'string',
        enum: ['dawn', 'noon', 'dusk', 'midnight'],
        default: 'dawn',
        description: 'Initial lighting state for the cycle.'
      },
      transitionMinutes: {
        type: 'number',
        default: 5,
        description: 'Game-time minutes to blend between major lighting states.'
      },
      enableWeather: {
        type: 'boolean',
        default: false,
        description: 'Whether to expose hooks for weather-driven sky adjustments.'
      }
    },
    build: (args) => {
      const startTime = clampChoice(args.startTime, ['dawn', 'noon', 'dusk', 'midnight'], 'dawn');
      const transitionMinutes = coerceNumber(args.transitionMinutes, 5, 1, 60);
      const enableWeather = Boolean(args.enableWeather);

      const weatherLine = enableWeather
        ? '- Expose interfaces for cloud opacity, precipitation-driven skylight updates, and lightning flashes.'
        : '- Weather hooks are disabled; keep the blueprint lean';

      const text = `Implement a Blueprint-based day/night cycle manager.
- Start the sequence at ${startTime} lighting.
- Advance sun rotation, skylight captures, fog, and sky atmosphere continuously with ${transitionMinutes} minute blends between key states.
- Sync directional light intensity/color with real-world sun elevation and inject moonlight at night.
- ${weatherLine}.
- Provide editor controls for time-of-day multiplier and manual overrides.
- Document the generated blueprint path and exposed parameters.`;

      return [{
        role: 'user',
        content: { type: 'text', text }
      }];
    }
  },
  {
    name: 'design_cinematic_camera_move',
    description: 'Author a sequencer shot with a polished camera move and easing markers.',
    arguments: {
      durationSeconds: {
        type: 'number',
        default: 6,
        description: 'Shot duration in seconds.'
      },
      moveStyle: {
        type: 'string',
        enum: ['push_in', 'orbit', 'tracking', 'crane'],
        default: 'push_in',
        description: 'Camera move archetype to emphasize.'
      },
      focusTarget: {
        type: 'string',
        description: 'Optional actor or component name to keep in focus.',
        required: false
      }
    },
    build: (args) => {
      const duration = coerceNumber(args.durationSeconds, 6, 2, 30);
      const moveStyle = clampChoice(args.moveStyle, ['push_in', 'orbit', 'tracking', 'crane'], 'push_in');
      const focusLine = typeof args.focusTarget === 'string' && args.focusTarget.trim().length > 0
        ? `Lock focus distance on "${args.focusTarget}" and animate depth of field pulls if necessary.`
        : 'Pick the most prominent subject in frame and maintain crisp focus throughout the move.';

      const moveHints: Record<string, string> = {
        push_in: 'Ease-in push toward the subject with gentle camera roll stabilization.',
        orbit: '360° orbit with consistent parallax and a tracked look-at target.',
        tracking: 'Match the subject velocity along a spline with smoothed acceleration.',
        crane: 'Combine vertical rise with lateral drift for a reveal shot.'
      };

      const text = `In Sequencer, author a ${duration.toFixed(1)} second cinematic shot.
- Movement style: ${moveStyle} (${moveHints[moveStyle]}).
- Key auto-exposure, camera focal length, and focal distance for a premium look.
- Add ease-in/ease-out tangents at shot boundaries to avoid abrupt starts/stops.
- ${focusLine}
- Annotate the timeline with intent markers (intro beat, climax, resolve).
- Render a preview range and summarize the created assets.`;

      return [{
        role: 'user',
        content: { type: 'text', text }
      }];
    }
  }
];
