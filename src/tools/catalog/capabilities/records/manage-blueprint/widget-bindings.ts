/**
 * Widget binding records (8): create_property_binding, bind_text, bind_visibility,
 * bind_color, bind_enabled, bind_on_clicked, bind_on_hovered, bind_on_value_changed.
 *
 * Each binds a widget property or event to a Blueprint variable or function.
 * Widget handles: widgetPath + slotName identify the target widget;
 * bindingSource identifies the variable or function to bind to.
 */
import type { CapabilityRecordSource } from '../../index.js';
import type { JsonObject } from '../../model.js';
import { buildPromotedRecord, buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-bindings';
const DOMAIN = 'widget';

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
    whenToUse: [`A ${action.replace(/_/g, ' ')} must be created on a widget.`],
    whenNotToUse: ['The widget should be set directly rather than bound.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, bindingSource: P.bindingSource, ...extraProps },
    required: ['action', 'widgetPath', 'slotName', 'bindingSource', ...extraRequired],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Widget_Text', bindingSource: 'PlayerName', ...(extras?.example ?? {}) },
    exampleOutput: { success: true, message: `${action} bound` },
  });
}

export const WIDGET_BINDINGS_RECORDS: readonly CapabilityRecordSource[] = [
  binding('create_property_binding', 'blueprint.create_property_binding', 'Create a property binding between a widget property and a Blueprint variable or function.',
    // Dogfood #35: the handler reads propertyName (the widget property, default Text) and functionName (alias of bindingSource).
    { props: { propertyName: { type: 'string', description: 'Widget property to bind (defaults to Text).' }, functionName: { type: 'string', description: 'Alias of bindingSource: the function or variable that feeds the binding.' } }, required: [], example: {} }),
  binding('bind_text', 'blueprint.bind_text', 'Bind the Text property of a widget to a Blueprint variable or function.'),
  binding('bind_visibility', 'blueprint.bind_visibility', 'Bind the Visibility property of a widget to a Blueprint variable or function.'),
  binding('bind_color', 'blueprint.bind_color', 'Bind the ColorAndOpacity property of a widget to a Blueprint variable or function.'),
  binding('bind_enabled', 'blueprint.bind_enabled', 'Bind the IsEnabled property of a widget to a Blueprint variable or function.'),
  binding('bind_on_clicked', 'blueprint.bind_on_clicked', 'Bind the OnClicked event of a Button widget to a Blueprint function.'),
  binding('bind_on_hovered', 'blueprint.bind_on_hovered', 'Bind the OnHovered/OnUnhovered events of a widget to Blueprint functions.',
    { props: { onHoveredFunction: P.onHoveredFunction, onUnhoveredFunction: P.onUnhoveredFunction }, required: ['onHoveredFunction'], example: { onHoveredFunction: 'OnButtonHovered', onUnhoveredFunction: 'OnButtonUnhovered' } }),
  binding('bind_on_value_changed', 'blueprint.bind_on_value_changed', 'Bind the OnValueChanged event of a Slider or SpinBox to a Blueprint function.'),
  buildPromotedRecord({
    id: 'blueprint.bind_localized_text',
    action: 'bind_localized_text',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Point a Text Block at a string-table entry so its text follows the active localization.',
    whenToUse: ['Displayed text must come from a string table rather than a literal.'],
    whenNotToUse: ['The text is authored inline and never localized (set it directly instead).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, stringTableId: P.stringTableId, stringKey: P.stringKey },
    required: ['action', 'widgetPath', 'slotName', 'stringTableId', 'stringKey'],
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
  }, 'Resolves text through a string table, unlike the property bindings that read a Blueprint member.'),
  buildPromotedRecord({
    id: 'blueprint.set_localization_key',
    action: 'set_localization_key',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Assign the localization namespace and key a Text Block resolves its text through.',
    whenToUse: ['A Text Block must carry an explicit localization key for translators.'],
    whenNotToUse: ['The text should read from a string table asset (use bind_localized_text).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, key: P.key, namespace: P.namespace },
    required: ['action', 'widgetPath', 'slotName', 'key'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, namespace: P.namespace, key: P.key },
    outputRequired: ['widgetPath', 'slotName', 'namespace', 'key'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_localization_key', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', key: 'Menu_Title', namespace: 'Game' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', namespace: 'Game', key: 'Menu_Title' },
  }, 'Stamps namespace/key metadata on the text itself rather than binding it to another asset or member.'),
  buildPromotedRecord({
    id: 'blueprint.set_widget_binding',
    action: 'set_widget_binding',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Bind any supported widget property to a Blueprint function, choosing the binding type from the property.',
    whenToUse: ['One call should bind a property without picking the per-property bind action.'],
    whenNotToUse: ['The specific binding action is already known (use bind_text, bind_visibility, and so on).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, targetWidget: P.targetWidget, property: P.property, functionName: P.functionName },
    required: ['action', 'widgetPath', 'targetWidget', 'property'],
    outputProps: {
      widgetPath: P.widgetPath,
      targetWidget: P.targetWidget,
      property: P.property,
      functionName: P.functionName,
      bindingType: { type: 'string', description: 'Binding kind the property resolved to (text, visibility, enabled, percent, or colorAndOpacity).' },
      targetVerified: { type: 'boolean', description: 'Whether the named widget was found in the widget tree.' },
      saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved after binding.' },
    },
    outputRequired: ['widgetPath', 'targetWidget', 'property', 'functionName', 'bindingType', 'targetVerified', 'saved'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_widget_binding', widgetPath: '/Game/UI/WBP_MainUI', targetWidget: 'HealthBar', property: 'Percent', functionName: 'GetHealthPercent' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', targetWidget: 'HealthBar', property: 'Percent', functionName: 'GetHealthPercent', bindingType: 'percent', targetVerified: true, saved: true },
  }, 'Selects the binding kind from the property name, where every other bind action fixes one kind up front.'),
];
