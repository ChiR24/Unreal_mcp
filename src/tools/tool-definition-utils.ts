/**
 * Common Schema Definitions for MCP Tool Definitions
 * Phase 49: Common Schema Extraction - reduces token consumption by centralizing repeated patterns
 */

export const commonSchemas = {
  // ============================================
  // TRANSFORM & VECTOR SCHEMAS
  // ============================================
  location: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D location (x, y, z).'
  },
  rotation: {
    type: 'object',
    properties: {
      pitch: { type: 'number' },
      yaw: { type: 'number' },
      roll: { type: 'number' }
    },
    description: '3D rotation (pitch, yaw, roll).'
  },
  scale: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D scale (x, y, z).'
  },
  vector3: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D vector.'
  },
  vector2: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' }
    },
    description: '2D vector.'
  },

  // ============================================
  // COLOR SCHEMAS
  // ============================================
  color: {
    type: 'array',
    items: { type: 'number' },
    description: 'RGBA color as an array [r, g, b, a].'
  },
  colorObject: {
    type: 'object',
    properties: {
      r: { type: 'number' },
      g: { type: 'number' },
      b: { type: 'number' },
      a: { type: 'number' }
    },
    description: 'RGBA color as an object.'
  },

  // ============================================
  // PATH SCHEMAS
  // ============================================
  assetPath: { type: 'string', description: 'Asset path (e.g., /Game/Path/Asset).' },
  blueprintPath: { type: 'string', description: 'Blueprint asset path.' },
  meshPath: { type: 'string', description: 'Mesh asset path.' },
  texturePath: { type: 'string', description: 'Texture asset path.' },
  materialPath: { type: 'string', description: 'Material asset path.' },
  soundPath: { type: 'string', description: 'Sound asset path.' },
  animationPath: { type: 'string', description: 'Animation asset path.' },
  levelPath: { type: 'string', description: 'Level asset path.' },
  skeletonPath: { type: 'string', description: 'Skeleton asset path.' },
  skeletalMeshPath: { type: 'string', description: 'Skeletal mesh path.' },
  niagaraPath: { type: 'string', description: 'Niagara system path.' },
  widgetPath: { type: 'string', description: 'Widget blueprint path.' },

  physicsAssetPath: { type: 'string', description: 'Path to physics asset.' },
  morphTargetPath: { type: 'string', description: 'Path to morph target.' },
  clothAssetPath: { type: 'string', description: 'Path to cloth asset.' },
  iconPath: { type: 'string', description: 'Path to icon texture.' },
  itemDataPath: { type: 'string', description: 'Path to item data asset.' },
  gameplayAbilityPath: { type: 'string', description: 'Path to gameplay ability.' },
  gameplayEffectPath: { type: 'string', description: 'Path to gameplay effect.' },
  gameplayCuePath: { type: 'string', description: 'Path to gameplay cue.' },
  meshAssetPath: { type: 'string', description: 'Path to mesh asset.' },
  textureAssetPath: { type: 'string', description: 'Path to texture asset.' },
  materialAssetPath: { type: 'string', description: 'Path to material asset.' },
  soundAssetPath: { type: 'string', description: 'Path to sound asset.' },
  animationAssetPath: { type: 'string', description: 'Path to animation asset.' },
  blueprintAssetPath: { type: 'string', description: 'Path to blueprint asset.' },

  directoryPath: { type: 'string', description: 'Path to a directory.' },
  outputPath: { type: 'string', description: 'Output file or directory path.' },
  destinationPath: { type: 'string', description: 'Destination path for move/copy.' },
  savePath: { type: 'string', description: 'Path to save the asset.' },
  sourcePath: { type: 'string', description: 'Source path for import/move/copy.' },
  targetPath: { type: 'string', description: 'Target path for operations.' },
  directoryPathForCreation: { type: 'string', description: 'Directory path for asset creation.' },

  // ============================================
  // NAME SCHEMAS
  // ============================================
  name: { type: 'string', description: 'Name identifier.' },
  newName: { type: 'string', description: 'New name for renaming.' },
  assetNameForCreation: { type: 'string', description: 'Name of the asset to create.' },
  actorName: { type: 'string', description: 'Name of the actor.' },
  actorNameInLevel: { type: 'string', description: 'Name of the actor in the level.' },
  childActorName: { type: 'string', description: 'Name of the child actor (for attach/detach operations).' },
  parentActorName: { type: 'string', description: 'Name of the parent actor (for attach operations).' },
  componentName: { type: 'string', description: 'Name of the component.' },
  boneName: { type: 'string', description: 'Name of the bone.' },
  socketName: { type: 'string', description: 'Name of the socket.' },
  slotName: { type: 'string', description: 'Name of the slot.' },
  parameterName: { type: 'string', description: 'Name of the parameter.' },
  propertyName: { type: 'string', description: 'Name of the property.' },
  variableName: { type: 'string', description: 'Name of the variable.' },
  functionName: { type: 'string', description: 'Name of the function.' },
  eventName: { type: 'string', description: 'Name of the event.' },
  tagName: { type: 'string', description: 'Name of the tag.' },
  attributeName: { type: 'string', description: 'Name of the attribute.' },
  stateName: { type: 'string', description: 'Name of the state.' },

  // ============================================
  // GRAPH & NODE SCHEMAS
  // ============================================
  nodeId: { type: 'string', description: 'ID of the node.' },
  sourceNodeId: { type: 'string', description: 'ID of the source node.' },
  targetNodeId: { type: 'string', description: 'ID of the target node.' },
  pinName: { type: 'string', description: 'Name of the pin.' },
  sourcePin: { type: 'string', description: 'Name of the source pin.' },
  targetPin: { type: 'string', description: 'Name of the target pin.' },
  graphName: { type: 'string', description: 'Name of the graph.' },
  nodeName: { type: 'string', description: 'Name of the node.' },

  // ============================================
  // COMMON PROPERTY TYPES
  // ============================================
  booleanProp: { type: 'boolean' },
  numberProp: { type: 'number' },
  stringProp: { type: 'string' },
  // Note: 'integer' is a valid JSON Schema type for whole numbers (counts, indices)
  integerProp: { type: 'integer' },
  objectProp: { type: 'object' },
  arrayOfStrings: { type: 'array', items: { type: 'string' } },
  // Note: arrayOfNumbers is used for SCS transforms [x,y,z] format in Blueprint Manager.
  // Use location/rotation/scale objects for Actor Control {x,y,z} format.
  arrayOfNumbers: { type: 'array', items: { type: 'number' } },
  arrayOfObjects: { type: 'array', items: { type: 'object' } },
  value: { description: 'Generic value (any type).' },
  parentClass: { type: 'string', description: 'Path or name of the parent class.' },

  // ============================================
  // COMMON FLAGS (BOOLEANS)
  // ============================================
  save: { type: 'boolean', description: 'Save the asset(s) after the operation.' },
  compile: { type: 'boolean', description: 'Compile the blueprint(s) after the operation.' },
  overwrite: { type: 'boolean', description: 'Overwrite if the asset/file already exists.' },
  recursive: { type: 'boolean', description: 'Perform the operation recursively.' },
  enabled: { type: 'boolean', description: 'Whether the item/feature is enabled.' },
  visible: { type: 'boolean', description: 'Whether the item/actor is visible.' },

  // ============================================
  // FILTERS & SETTINGS
  // ============================================
  filter: { type: 'string', description: 'General search filter.' },
  tagFilter: { type: 'string', description: 'Filter by tags.' },
  classFilter: { type: 'string', description: 'Filter by class.' },
  resolution: { type: 'string', description: 'Resolution setting (e.g., 1024x1024).' },

  // ============================================
  // OUTPUT SCHEMA COMPONENTS (Properties for spreading)
  // ============================================
  outputBase: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' }
  },
  outputWithPath: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    path: { type: 'string' },
    assetPath: { type: 'string' }
  },
  outputWithActor: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    actor: { type: 'string' },
    actorPath: { type: 'string' }
  },
  outputWithNodeId: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    nodeId: { type: 'string' }
  },

  // ============================================
  // DIMENSIONS & SIZES
  // ============================================
  dimensions: {
    type: 'object',
    properties: {
      width: { type: 'number' },
      height: { type: 'number' },
      depth: { type: 'number' }
    },
    description: '3D dimensions (width, height, depth).'
  },
  size2D: {
    type: 'object',
    properties: {
      width: { type: 'number' },
      height: { type: 'number' }
    },
    description: '2D dimensions (width, height).'
  },
  extent: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D extent (half-size).'
  },

  // ============================================
  // NUMBER SCHEMAS
  // ============================================
  width: { type: 'number', description: 'Width value.' },
  height: { type: 'number', description: 'Height value.' },
  depth: { type: 'number', description: 'Depth value.' },
  radius: { type: 'number', description: 'Radius value.' },
  intensity: { type: 'number', description: 'Intensity value.' },
  angle: { type: 'number', description: 'Angle in degrees.' },
  strength: { type: 'number', description: 'Strength or weight.' },
  speed: { type: 'number', description: 'Speed value.' },
  duration: { type: 'number', description: 'Duration in seconds.' },
  distance: { type: 'number', description: 'Distance value.' },

  // ============================================
  // RANGES & BOUNDS
  // ============================================
  floatRange: {
    type: 'object',
    properties: {
      min: { type: 'number' },
      max: { type: 'number' }
    },
    description: 'Range of float values.'
  },
  intRange: {
    type: 'object',
    properties: {
      min: { type: 'integer' },
      max: { type: 'integer' }
    },
    description: 'Range of integer values.'
  },
  bounds: {
    type: 'object',
    properties: {
      min: {
        type: 'object',
        properties: {
          x: { type: 'number' },
          y: { type: 'number' },
          z: { type: 'number' }
        }
      },
      max: {
        type: 'object',
        properties: {
          x: { type: 'number' },
          y: { type: 'number' },
          z: { type: 'number' }
        }
      }
    },
    description: 'Bounding box (min, max vectors).'
  }
};

/**
 * Creates a standard tool output schema by merging custom properties with the base output fields.
 */
export function createOutputSchema(additionalProperties: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    type: 'object',
    properties: {
      success: { type: 'boolean' },
      message: { type: 'string' },
      error: { type: 'string' },
      ...additionalProperties
    }
  };
}

/**
 * Formats a tool description to include a list of supported actions.
 */
export function actionDescription(description: string, actions: string[]): string {
  if (!actions || actions.length === 0) return description;
  return `${description}\n\nSupported actions: ${actions.join(', ')}.`;
}
