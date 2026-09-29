/**
 * Widget localization binding records: bind_localized_text, set_localization_key.
 *
 * Each binds a widget property or event to a Blueprint variable or function.
 * Widget handles: widgetPath + slotName identify the target widget;
 * bindingSource identifies the variable or function to bind to.
 */
import type { CapabilityRecordSource } from '../../model.js';
import type { JsonObject } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-bindings';
const DOMAIN = 'widget';

// Property binds report the getter they bound; event binds report the node and call per event.
const BINDING_OUT = {
  slotName: { type: 'string', description: 'The widget that was bound.' },
  property: { type: 'string', description: 'Property binds: the widget property now bound (Text, Visibility, ColorAndOpacity, bIsEnabled, ...).' },
  functionName: { type: 'string', description: 'The function now bound or called: the generated Get_<Widget>_<Property> getter, or the event handler.' },
  generatedGetter: { type: 'boolean', description: 'Property binds: true when a converting getter was generated for a variable.' },
  bindings: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Event binds: per event, its name, functionName, createdFunction, createdEvent, nodeId and callNodeId.' },
  saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
};

/** `required` and `example` share K, so a required binding argument absent from the example fails to compile. */
interface BindingExtras<K extends string> {
  readonly props: Record<string, unknown>;
  readonly required: readonly K[];
  readonly example: Record<K, unknown> & JsonObject;
}

function binding<K extends string>(action: string, id: string, summary: string, extras?: BindingExtras<K>): CapabilityRecordSource {
  const extraProps = extras?.props ?? {};
  const extraRequired = extras?.required ?? [];
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [action.startsWith('bind_on_')
      ? `A widget's ${action.slice(5).split('_').map((word) => word.charAt(0).toUpperCase() + word.slice(1)).join('')} event must run a Blueprint function.`
      : `A widget's ${action.slice(5).replace(/_/g, ' ')} must follow a Blueprint variable or function at runtime.`],
    whenNotToUse: ['The widget should be set directly rather than bound.'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, bindingSource: P.bindingSource, ...extraProps },
    required: ['widgetPath', 'slotName', 'bindingSource', ...extraRequired],
    outputProps: BINDING_OUT,
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Widget_Text', bindingSource: 'PlayerName', ...(extras?.example ?? {}) },
    exampleOutput: { success: true, message: `${action} bound` },
  });
}

export const WIDGET_BINDINGS_RECORDS: readonly CapabilityRecordSource[] = [
  binding('bind_text', 'blueprint.bind_text', 'Bind the Text of a widget to a Blueprint variable (any type that converts to text) or pure function.'),
  binding('bind_visibility', 'blueprint.bind_visibility', 'Bind the Visibility of a widget to a Blueprint variable (a bool maps true to Visible, false to Collapsed) or pure function.'),
  binding('bind_color', 'blueprint.bind_color', 'Bind the tint of a widget (text or image color, progress fill, border brush) to a Blueprint variable or pure function.'),
  binding('bind_enabled', 'blueprint.bind_enabled', 'Bind whether a widget is enabled to a Blueprint bool variable or pure function.'),
  binding('bind_percent', 'blueprint.bind_percent', 'Bind the fill Percent of a Progress Bar (or the Value of a Slider or SpinBox) to a Blueprint float variable or pure function, so a health or loading bar follows it at runtime.'),
  binding('bind_on_clicked', 'blueprint.bind_on_clicked', 'Wire the OnClicked event of a Button to a Blueprint function (bindingSource), creating the function when missing.'),
  buildRecord({
    id: 'blueprint.bind_on_hovered',
    action: 'bind_on_hovered',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Wire the OnHovered (and optionally OnUnhovered) event of a Button to Blueprint functions, creating them when missing.',
    whenToUse: ['A button must react when the pointer moves onto or off it.'],
    whenNotToUse: ['Only a hover sound is wanted (set_style hoverSoundPath).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, onHoveredFunction: P.onHoveredFunction, onUnhoveredFunction: P.onUnhoveredFunction },
    required: ['widgetPath', 'slotName', 'onHoveredFunction'],
    outputProps: BINDING_OUT,
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'bind_on_hovered', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'PlayButton', onHoveredFunction: 'OnPlayHovered', onUnhoveredFunction: 'OnPlayUnhovered' },
    exampleOutput: { success: true, message: "OnHovered of 'PlayButton' now calls OnPlayHovered" },
  }),
  binding('bind_on_value_changed', 'blueprint.bind_on_value_changed', 'Wire the value-changed event of a Slider, SpinBox, CheckBox or ComboBoxString to a Blueprint function (bindingSource) that receives the new value; the function is created when missing.'),
  buildRecord({
    id: 'blueprint.bind_localized_text',
    action: 'bind_localized_text',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Point a Text Block at a string-table entry so its text follows the active localization.',
    whenToUse: ['Displayed text must come from a string table rather than a literal.'],
    whenNotToUse: ['The text is authored inline and never localized (set it directly instead).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, stringTableId: P.stringTableId, stringKey: P.stringKey },
    required: ['widgetPath', 'slotName', 'stringTableId', 'stringKey'],
    outputProps: {
      widgetPath: P.widgetPath,
      slotName: P.slotName,
      stringTableId: P.stringTableId,
      stringKey: P.stringKey,
      note: { type: 'string', description: 'Why the binding was not applied; present only when success is false.' },
    },
    outputRequired: ['widgetPath', 'slotName', 'stringTableId', 'stringKey'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'bind_localized_text', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', stringTableId: '/Game/Localization/ST_UI.ST_UI', stringKey: 'Menu_Title' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', stringTableId: '/Game/Localization/ST_UI.ST_UI', stringKey: 'Menu_Title' },
  }),
  buildRecord({
    id: 'blueprint.set_localization_key',
    action: 'set_localization_key',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Assign the localization namespace and key a Text Block resolves its text through.',
    whenToUse: ['A Text Block must carry an explicit localization key for translators.'],
    whenNotToUse: ['The text should read from a string table asset (use bind_localized_text).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, key: P.key, namespace: P.namespace },
    required: ['widgetPath', 'slotName', 'key'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, namespace: P.namespace, key: P.key },
    outputRequired: ['widgetPath', 'slotName', 'namespace', 'key'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_localization_key', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', key: 'Menu_Title', namespace: 'Game' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', namespace: 'Game', key: 'Menu_Title' },
  }),
];
