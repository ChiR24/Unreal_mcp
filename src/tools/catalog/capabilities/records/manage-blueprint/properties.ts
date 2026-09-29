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
import type { JsonObject } from '../../model.js';

const S = (d: string): JsonObject => ({ type: 'string', description: d });
const N = (d: string): JsonObject => ({ type: 'number', description: d });
const B = (d: string): JsonObject => ({ type: 'boolean', description: d });

export const P = {
  // Common
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
  variableType: S('Variable type, any case. Basic: Boolean, Byte, Integer, Int64, Float, Double, String, Name, Text, Vector, Vector2D, Vector4, Rotator, Transform, Color, LinearColor. References: Object or Class (any UObject), or Object:<class>, Class:<class>, SoftObject:<class>, SoftClass:<class> with a class name or path (Actor, /Script/UMG.Widget, /Game/Blueprints/BP_Door); a bare class name or path is an object reference to that class. Structs: struct:<path> (struct:/Game/Data/F_Item), a /Script path (/Script/Engine.HitResult) or a struct name (HitResult). Enums: enum:<object path> (enum:/Script/Engine.ECollisionChannel, enum:/Game/Enums/E_State.E_State). Containers: Array<T>, Set<T> or Map<Key,Value> (also Array:T, Set:T, Map:Key,Value); a map key is Byte, Integer, Int64, Name, String or an enum. Example: Array<Object:/Script/UMG.Widget>. An unknown spelling is refused with TYPE_RESOLUTION_FAILED and the reason.'),
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
  targetClass: S(
    'Target class for a node that carries one: a Cast target, a CreateWidget or '
      + 'SpawnActor class, or the subsystem a Get Subsystem node returns. Required '
      + 'for cast and subsystem nodes, whose type is stored on the node itself and '
      + 'cannot be set afterwards.',
  ),
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
  nodeFunctionName: S('Function a CallFunction node calls (the same as memberName), e.g. PrintString.'),
  pure: B('Build a Cast node pure (no exec pins, as in the editor\'s Convert to pure cast); default false.'),
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
  alignment: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'CanvasPanel child: the pivot {x, y}, each 0-1. Any other slot (box, overlay, border, scroll box): x is the horizontal and y the vertical alignment, each a number (0 left/top, 0.5 center, 1 right/bottom) or a word: "fill", "left", "center", "right", "top", "bottom"; leave one out to keep it. {"x": "fill", "y": "center"} spans the width and centers vertically.' },
  zOrder: N('Z-order for a canvas slot.'),
  padding: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget slot padding {left,top,right,bottom}.' },
  position: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Widget position offset.' },
  // set_size reads this via GetObjectField + x/y, exactly like position/alignment;
  // it was declared as a bare number, which no handler ever read.
  size: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Size override {x,y} of a CanvasPanel child. A HorizontalBox, VerticalBox or ScrollBox child is sized by sizeRule and fillValue instead.' },
  sizeRule: S('A HorizontalBox, VerticalBox or ScrollBox child: Auto sizes it to its content, Fill shares the free space between the Fill children by their fillValue weights (case-insensitive). Two buttons in a row that should share it evenly are both Fill with the same fillValue; a ScrollBox child that is Fill spans the box when it is shorter, so a vertical alignment of center can center it.'),
  fillValue: { type: 'number', minimum: 0, description: 'A HorizontalBox, VerticalBox or ScrollBox child: the Fill weight, 0 or more (1 by default; a child of 2 gets twice the space of a child of 1). Given without sizeRule it makes the child Fill.' },
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
  cornerRadius: { type: 'number', description: 'Corner radius in pixels for a widget that draws a brush (Image, Button, Border). Switches the brush to a RoundedBox; 0 restores square corners.' },
  outlineColor: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Outline color (0-1 values) drawn around a rounded brush. Ignored unless cornerRadius is set.' },
  outlineWidth: { type: 'number', description: 'Outline thickness in pixels around a rounded brush. Ignored unless cornerRadius is set.' },
  renderOpacity: { type: 'number', description: 'Render opacity (0-1) applied to the widget and everything under it.' },
  hoverSoundPath: S('Sound a Button plays when the pointer moves onto it (SoundCue, SoundWave or MetaSound path); an empty string clears it.'),
  pressSoundPath: S('Sound a Button plays when it is pressed (SoundCue, SoundWave or MetaSound path); an empty string clears it.'),
  justification: S('Text justification of a TextBlock or RichTextBlock: left, center or right. To centre the widget itself in its slot use set_alignment.'),
  fontFamily: S('Font asset for a TextBlock, e.g. /Engine/EngineFonts/Roboto or a project font.'),
  typeface: S('Face of the TextBlock font: Regular, Bold, Italic, Light... A face the font lacks is refused with the list it has.'),
  letterSpacing: { type: 'number', description: 'Extra space between the letters of a TextBlock, in thousandths of an em (0 normal, 100 airy, negative tighter).' },
  copyStyleFrom: S('Name of another TextBlock in the same Widget Blueprint whose font, colour and shadow this one takes; the other fields in the call then apply on top.'),
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
  value: N('Numeric value for a slider, spin box, or animation keyframe.'),
  preset: S('Named anchor preset (e.g. TopCenter) applied in place of anchorMin/anchorMax.'),
  minDesiredSlotWidth: N('Minimum slot width.'),
  minDesiredSlotHeight: N('Minimum slot height.'),
  innerSlotPadding: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Inner wrap box slot padding.' },
  wrapWidth: N('Wrap width for wrap box.'),
  explicitWrapWidth: B('Use explicit wrap width.'),
  widthOverride: N('Width override for size box.'),
  maxDesiredWidth: N('Maximum desired width of a size box.'),
  maxDesiredHeight: N('Maximum desired height of a size box.'),
  contentColorAndOpacity: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Tint (0-1 values) a border applies to its content.' },
  heightOverride: N('Height override for size box.'),
  minDesiredWidth: N('Minimum desired width.'),
  minDesiredHeight: N('Minimum desired height.'),
  stretch: { type: 'string', enum: ['None', 'Fill', 'ScaleToFit', 'ScaleToFitX', 'ScaleToFitY', 'ScaleToFill', 'UserSpecified'], description: 'Scale box stretch mode.' },
  stretchDirection: { type: 'string', enum: ['Both', 'DownOnly', 'UpOnly'], description: 'Scale box stretch direction.' },
  userSpecifiedScale: N('User specified scale value.'),
  // Widget bindings
  bindingSource: S('For bind_text, bind_color, bind_enabled, bind_percent and bind_visibility: the variable (a getter converting it is generated) or pure no-input function the property reads. For bind_on_clicked and bind_on_value_changed: the function the event calls, created with the event inputs when it does not exist.'),
  onHoveredFunction: S('Function the Button calls when the pointer moves onto it; created when it does not exist.'),
  onUnhoveredFunction: S('Function the Button calls when the pointer leaves it; created when it does not exist.'),
  // Widget animation
  animationName: S('Widget animation name.'),
  trackType: { type: 'string', enum: ['transform', 'color', 'opacity', 'renderOpacity', 'translation', 'scale', 'angle', 'shear'], description: 'Animation track type: opacity/renderOpacity (RenderOpacity), color (ColorAndOpacity), translation/scale/angle/shear or transform (RenderTransform).' },
  time: N('Keyframe time.'),
  interpolation: { type: 'string', enum: ['linear', 'cubic', 'constant'], description: 'Keyframe interpolation.' },
  duration: N('Duration in seconds.'),
  // Promoted widget-authoring routes. Names match the native payload fields
  // exactly; a rename here silently stops the handler from reading the value.
  positionX: N('Canvas slot X position, applied only when the parent is a canvas panel.'),
  positionY: N('Canvas slot Y position, applied only when the parent is a canvas panel.'),
  sizeX: N('Slot width in slate units.'),
  sizeY: N('Slot height in slate units.'),
  activeIndex: N('Index shown first by a widget switcher.'),
  stringTableId: S('String table asset backing a localized text binding.'),
  stringKey: S('Key looked up within the string table.'),
  font: S('Font asset path. Omitted keeps the current font; fontSize alone resizes it.'),
  key: S('Localization key assigned to the text widget.'),
  namespace: S('Localization namespace owning the key.'),
  left: N('Left margin in slate units.'),
  top: N('Top margin in slate units.'),
  right: N('Right margin in slate units.'),
  bottom: N('Bottom margin in slate units.'),
  // Ready-made HUD pieces (add_game_widget) and screens (create_widget_template)
  title: S('Title text: the heading of a menu, credits screen or tracker.'),
  keyLabel: S('Key shown in the interaction prompt badge, such as E.'),
  items: { type: 'array', items: S('Row text.'), description: 'Rows the objective or quest tracker lists, top to bottom.' },
  maxVisibleObjectives: N('Rows the objective tracker shows (1-12, default 3); longer items lists are cut to this.'),
  mapSize: N('Minimap width and height in pixels (default 220).'),
  fadeTime: N('Seconds of the fade: the damage flash (default 0.6) or the loading screen FadeIn animation (0 makes none).'),
  buttons: { type: 'array', items: S('Button label.'), description: 'Menu button labels, top to bottom; each becomes <Label>Button.' },
  settingsType: { type: 'string', enum: ['all', 'graphics', 'audio', 'controls'], description: 'Which settings sections the menu holds (default all).' },
  includeProgressBar: B('Whether the loading screen has a progress bar (default true).'),
  elements: {
    type: 'array',
    items: { type: 'string', enum: ['health_bar', 'ammo_counter', 'crosshair', 'minimap', 'compass', 'damage_indicator', 'interaction_prompt', 'objective_tracker', 'quest_tracker'] },
    description: 'HUD pieces the new HUD starts with (default health_bar, crosshair, ammo_counter; [] for an empty canvas).',
  },
  showSpeakerName: B('Whether the dialog shows the speaker name line (default true).'),
  responseCount: N('Response buttons under the dialog line (0-6, default 3).'),
  columns: N('Grid columns: inventory 1-12 (default 6), shop 1-8 (default 4).'),
  rows: N('Inventory grid rows (1-12, default 4).'),
  segmentCount: N('Radial menu segments (2-12, default 8).'),
  itemCount: N('Shop item cards (1-48, default 8).'),
  entries: {
    type: 'array',
    items: { type: 'object', properties: { title: S('Role or section, such as Music.'), name: S('Name credited.') }, required: ['title', 'name'], additionalProperties: false },
    description: 'Credits entries in order; default is four placeholder sections.',
  },
  // Common output
  success: B('Whether the action succeeded.'),
} as const;

export type PropertyMap = JsonObject;
