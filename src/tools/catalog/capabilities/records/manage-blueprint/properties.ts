/**
 * Shared JSON-Schema property fragments for manage_blueprint records.
 *
 * Every property mirrors a field declared in
 * src/tools/definitions/core/blueprint/manage-blueprint-{core,widget}-properties.ts
 * so the canonical input schema and the capability record schemas agree.
 *
 * Reflection-boundary markers (`x-unreal-reflection-boundary: true`) are applied
 * to unbounded object/array inputs that cross into Unreal reflection (metadata,
 * properties, operations, parameters) per the shared json-schema validator.
 */
import type { JsonObject } from '../../index.js';

const S = (d: string): JsonObject => ({ type: 'string', description: d });
const N = (d: string): JsonObject => ({ type: 'number', description: d });
const B = (d: string): JsonObject => ({ type: 'boolean', description: d });

export const P = {
  // Common
  action: S('The manage_blueprint action to execute.'),
  // Blueprint paths and names
  blueprintPath: S('Canonical /Game Blueprint asset path.'),
  savePath: S('Destination /Game folder for a new Blueprint.'),
  name: S('Name for the new Blueprint or asset.'),
  newName: S('New name for a renamed variable, function, or component.'),
  parentClass: S('Parent class path for Blueprint creation (e.g. /Script/Engine.Actor).'),
  blueprintType: S('Blueprint type hint for creation.'),
  // SCS / components
  componentType: S('Component class name to add.'),
  componentClass: S('Component class path for SCS node creation.'),
  componentName: S('Name for the SCS component node.'),
  parentComponent: S('Parent SCS node name for reparenting.'),
  attachTo: S('Socket or parent component to attach to.'),
  location: { type: 'object', description: 'Relative location {x, y, z} for an SCS component template.', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  rotation: { type: 'object', description: 'Relative rotation {pitch, yaw, roll} for an SCS component template.', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  scale: { type: 'object', description: 'Relative scale {x, y, z} for an SCS component template, or {x, y} for a widget render transform.', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  newParent: S('New parent SCS node name.'),
  meshPath: S('Static/Skeletal mesh asset path for a mesh component.'),
  materialPath: S('Material asset path for a component.'),
  applyAndSave: B('Whether to save the Blueprint after applying SCS changes.'),
  // Variables
  variableName: S('Variable name to add, remove, rename, or modify.'),
  variableType: S('Variable type (Boolean, Float, Integer, Vector, String, Object).'),
  defaultValue: { description: 'Default value for the variable or property.' },
  oldName: S('Current variable name before renaming.'),
  category: S('Category folder for the variable.'),
  isReplicated: B('Whether the variable is replicated.'),
  isPublic: B('Whether the variable is exposed to the editor/BP graph.'),
  // Metadata
  propertyName: S('Property name to set on the CDO or component.'),
  propertyValue: { description: 'Value to assign to the property.' },
  metadata: {
    type: 'object',
    description: 'Arbitrary metadata key-value pairs.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  properties: {
    type: 'object',
    description: 'Property bag applied to the CDO, component template, or node.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  // Graph
  graphName: S('Target graph name (Event Graph, Construction Script, etc.).'),
  nodeType: S('Blueprint node type string for creation.'),
  nodeId: S('Existing node identifier returned by create_node or get_graph_details.'),
  nodeName: S('Human-readable node name.'),
  memberName: S('Member (function/variable/event) name the node represents.'),
  memberClass: S('Member class (function, variable, event, etc.).'),
  targetClass: S('Target class for a class-member node.'),
  pinName: S('Pin name on a graph node.'),
  linkedTo: S('Target pin descriptor for a pin link.'),
  nodeGuid: S('Node GUID accepted in place of nodeId.'),
  sourceNode: S('Source node id accepted in place of fromNodeId.'),
  targetNode: S('Target node id accepted in place of toNodeId.'),
  sourcePin: S('Source pin name on the originating node.'),
  targetPin: S('Target pin name on the destination node.'),
  inputAxisName: S('Axis name for an InputAxis event node.'),
  inputActionPath: S('Enhanced Input action asset path for an EnhancedInputAction node.'),
  inputActionAssetPath: S('Enhanced Input action asset path accepted in place of inputActionPath.'),
  actionPath: S('Enhanced Input action asset path accepted in place of inputActionPath.'),
  timeoutMs: N('Request timeout in milliseconds.'),
  fromNodeId: S('Source node identifier for a pin connection.'),
  fromPinName: S('Source pin name for a pin connection.'),
  toNodeId: S('Target node identifier for a pin connection.'),
  toPinName: S('Target pin name for a pin connection.'),
  posX: N('X coordinate for node placement.'),
  posY: N('Y coordinate for node placement.'),
  includePins: B('When true, graph details include per-node pins and links.'),
  structPath: S('Blueprint Struct asset path (UserDefinedStruct or native UScriptStruct).'),
  // Functions / events
  functionName: S('Function name to add or remove.'),
  eventType: S('Event type string for add_event.'),
  eventName: S('Custom event name.'),
  customEventName: S('Custom event name to create.'),
  parameters: {
    type: 'array',
    description: 'Function/event parameter descriptors.',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
  },
  inputs: {
    type: 'array',
    description: 'Function input parameter descriptors.',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
  },
  outputs: {
    type: 'array',
    description: 'Function output parameter descriptors.',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
  },
  // Compilation
  saveAfterCompile: B('Whether to save the asset after compiling.'),
  // Probe / handle
  operations: {
    type: 'array',
    description: 'Batch operations for probe_handle.',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    'x-unreal-reflection-boundary': true,
  },
  // Widget paths
  path: S('Destination /Game folder for a new Widget Blueprint.'),
  widgetPath: S('Canonical /Game Widget Blueprint asset path.'),
  folder: S('Destination /Game folder for a Widget Blueprint.'),
  slotName: S('Slot name for a child widget inside its parent.'),
  parentSlot: S('Parent slot to add the widget to.'),
  // Widget layout
  anchorMin: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Minimum anchor point (0-1).' },
  anchorMax: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Maximum anchor point (0-1).' },
  alignment: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget alignment (0-1).' },
  zOrder: N('Z-order for a canvas slot.'),
  padding: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget slot padding {left,top,right,bottom}.' },
  position: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget position offset.' },
  // set_size reads this via GetObjectField + x/y, exactly like position/alignment;
  // it was declared as a bare number, which no handler ever read.
  size: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget size override {x,y}.' },
  width: N('Width override.'),
  height: N('Height override.'),
  translation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Render translation offset.' },
  shear: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Render shear.' },
  angle: N('Render rotation angle in degrees.'),
  visibility: {
    type: 'string',
    enum: ['Visible', 'Collapsed', 'Hidden', 'HitTestInvisible', 'SelfHitTestInvisible'],
    description: 'Widget visibility state.',
  },
  clipping: {
    type: 'string',
    enum: ['Inherit', 'ClipToBounds', 'ClipToBoundsWithoutIntersecting', 'ClipToBoundsAlways', 'OnDemand'],
    description: 'Widget clipping mode.',
  },
  // Widget content
  text: S('Text content for a text block or button.'),
  fontSize: N('Font size.'),
  colorAndOpacity: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Color and opacity (0-1 values).' },
  autoWrap: B('Enable text auto-wrap.'),
  texturePath: S('Texture asset path for an image or brush.'),
  brushSize: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Brush/image size.' },
  brushColor: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Border brush color.' },
  isEnabled: B('Widget enabled state.'),
  isChecked: B('Checkbox checked state.'),
  minValue: N('Minimum slider/spinbox value.'),
  maxValue: N('Maximum slider/spinbox value.'),
  stepSize: N('Value step size for slider.'),
  delta: N('Spinbox increment.'),
  percent: N('Progress bar percentage (0-1).'),
  fillColorAndOpacity: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Fill color for progress bar.' },
  isMarquee: B('Progress bar marquee mode.'),
  inputType: { type: 'string', enum: ['single', 'multi'], description: 'Text input type.' },
  hintText: S('Placeholder hint text.'),
  options: { type: 'array', items: S('Option string.'), description: 'Combo box options.' },
  selectedOption: S('Selected combo box option.'),
  orientation: { type: 'string', enum: ['Horizontal', 'Vertical'], description: 'Widget orientation.' },
  // Widget panels
  scrollBarVisibility: { type: 'string', enum: ['Visible', 'Collapsed', 'Auto'], description: 'Scroll bar visibility.' },
  alwaysShowScrollbar: B('Always show scrollbar.'),
  columnCount: N('Number of columns in a uniform/grid panel.'),
  rowCount: N('Number of rows in a uniform/grid panel.'),
  slotPadding: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Padding between uniform grid slots.' },
  parentName: S('Optional parent panel name to add the widget under.'),
  x: N('Canvas X position for a HUD element (default 20).'),
  y: N('Canvas Y position for a HUD element (default 20).'),
  value: N('Numeric value for a slider, spin box, or animation keyframe.'),
  title: S('Title text for a menu template.'),
  preset: S('Named anchor preset (e.g. TopCenter) applied in place of anchorMin/anchorMax.'),
  minDesiredSlotWidth: N('Minimum slot width.'),
  minDesiredSlotHeight: N('Minimum slot height.'),
  innerSlotPadding: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Inner wrap box slot padding.' },
  wrapWidth: N('Wrap width for wrap box.'),
  explicitWrapWidth: B('Use explicit wrap width.'),
  widthOverride: N('Width override for size box.'),
  heightOverride: N('Height override for size box.'),
  minDesiredWidth: N('Minimum desired width.'),
  minDesiredHeight: N('Minimum desired height.'),
  stretch: { type: 'string', enum: ['None', 'Fill', 'ScaleToFit', 'ScaleToFitX', 'ScaleToFitY', 'ScaleToFill', 'UserSpecified'], description: 'Scale box stretch mode.' },
  stretchDirection: { type: 'string', enum: ['Both', 'DownOnly', 'UpOnly'], description: 'Scale box stretch direction.' },
  userSpecifiedScale: N('User specified scale value.'),
  // Widget bindings
  bindingSource: S('Variable or function name to bind to.'),
  onHoveredFunction: S('Function to call on hover.'),
  onUnhoveredFunction: S('Function to call on unhover.'),
  // Widget animation
  animationName: S('Widget animation name.'),
  trackType: { type: 'string', enum: ['transform', 'color', 'opacity', 'renderOpacity', 'material'], description: 'Animation track type.' },
  time: N('Keyframe time.'),
  interpolation: { type: 'string', enum: ['linear', 'cubic', 'constant'], description: 'Keyframe interpolation.' },
  loopCount: N('Number of loops (-1 for infinite).'),
  playMode: { type: 'string', enum: ['forward', 'reverse', 'pingpong'], description: 'Animation play mode.' },
  // Widget templates
  settingsType: { type: 'string', enum: ['video', 'audio', 'controls', 'gameplay', 'all'], description: 'Settings menu type.' },
  includeProgressBar: B('Include progress bar.'),
  promptFormat: S('Interaction prompt format.'),
  maxVisibleObjectives: N('Maximum visible objectives.'),
  fadeTime: N('Fade time in seconds.'),
  gridSize: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Inventory grid size {columns,rows}.' },
  showSpeakerName: B('Show speaker name in dialog.'),
  segmentCount: N('Number of radial segments.'),
  previewSize: { type: 'string', enum: ['1080p', '720p', 'mobile', 'custom'], description: 'Preview resolution preset.' },
  duration: N('Duration in seconds.'),
  // Common output
  success: B('Whether the action succeeded.'),
  message: S('Human-readable result message.'),
} as const;

export type PropertyMap = JsonObject;
