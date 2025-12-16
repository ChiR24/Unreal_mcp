export interface PropertyDictionaryEntry {
  description: string;
  typeHint?: string;
  commonValues?: Array<string | number | boolean>;
  recommendedActions?: string[];
  category?: string;
}

/**
 * Minimal curated metadata for frequently surfaced Unreal Engine properties.
 * This list can be expanded over time as we observe additional properties
 * that benefit from richer explanations.
 */
export const PROPERTY_DICTIONARY: Record<string, PropertyDictionaryEntry> = {
  intensity: {
    description: 'Controls the brightness emitted by the light component.',
    typeHint: 'float (lumens for point/spot, lux for directional)',
    commonValues: [1000, 5000, 10000],
    recommendedActions: ['Adjust to match the desired exposure', 'Animate via sequencer for flicker effects'],
    category: 'Lighting'
  },
  lightcolor: {
    description: 'Tint applied to emitted light. Uses linear RGBA (0-255) values.',
    typeHint: 'FColor',
    recommendedActions: ['Set to emphasize mood', 'Match art direction color palette'],
    category: 'Lighting'
  },
  mobility: {
    description: 'Whether the component is Static, Stationary, or Movable.',
    typeHint: 'enum (EComponentMobility)',
    commonValues: ['Static', 'Stationary', 'Movable'],
    recommendedActions: ['Switch to Movable for runtime transforms', 'Use Static for baked lighting'],
    category: 'Rendering'
  },
  relativelocation: {
    description: 'Local-space translation of the component relative to its parent.',
    typeHint: 'FVector',
    recommendedActions: ['Adjust to reposition relative to parent', 'Consider zeroing to reset offsets'],
    category: 'Transform'
  },
  relativerotation: {
    description: 'Local-space rotation applied after parent transforms.',
    typeHint: 'FRotator',
    recommendedActions: ['Tweak to orient the component correctly'],
    category: 'Transform'
  },
  relativescale3d: {
    description: 'Local-space non-uniform scale relative to parent.',
    typeHint: 'FVector',
    commonValues: [1],
    recommendedActions: ['Use uniform scaling to avoid skewing', 'Reset to (1,1,1) when debugging'],
    category: 'Transform'
  },
  tags: {
    description: 'Gameplay or editor tags applied to the actor for lookup operations.',
    typeHint: 'string[]',
    recommendedActions: ['Add descriptive tags to enable find_by_tag', 'Standardise tag prefixes for systems'],
    category: 'Metadata'
  },
  collisionenabled: {
    description: 'Specifies whether the primitive participates in collision queries.',
    typeHint: 'enum (ECollisionEnabled)',
    commonValues: ['NoCollision', 'QueryOnly', 'PhysicsOnly', 'QueryAndPhysics'],
    recommendedActions: ['Disable when using purely visual meshes', 'Enable QueryAndPhysics for interactive props'],
    category: 'Physics'
  },
  simulatephysics: {
    description: 'Enables physics simulation for the component allowing forces and gravity.',
    typeHint: 'bool',
    recommendedActions: ['Enable for ragdolls or dynamic rigid bodies', 'Disable for static set dressing'],
    category: 'Physics'
  },
  materials: {
    description: 'Array of material slots applied to the mesh component.',
    typeHint: 'UMaterialInterface[]',
    recommendedActions: ['Replace slot materials to change surface look', 'Verify slot count matches mesh sections'],
    category: 'Rendering'
  },
  skeletalmeshsocketnames: {
    description: 'Sockets exposed by the skeletal mesh for attachments.',
    typeHint: 'string[]',
    recommendedActions: ['Attach weapons or FX to named sockets', 'Use to verify sockets exist before attaching'],
    category: 'Animation'
  }
};

/**
 * Normalizes property names to dictionary keys (lowercase, no punctuation).
 */
export function normalizeDictionaryKey(name: string | undefined): string {
  if (!name) return '';
  return name.replace(/[^a-zA-Z0-9]/g, '').toLowerCase();
}

export function lookupPropertyMetadata(property: { name?: string } | string) {
  const key = typeof property === 'string' ? normalizeDictionaryKey(property) : normalizeDictionaryKey(property.name);
  return PROPERTY_DICTIONARY[key];
}
