/**
 * Widget binding records (8): create_property_binding, bind_text, bind_visibility,
 * bind_color, bind_enabled, bind_on_clicked, bind_on_hovered, bind_on_value_changed.
 *
 * Each binds a widget property or event to a Blueprint variable or function.
 * Widget handles: widgetPath + slotName identify the target widget;
 * bindingSource identifies the variable or function to bind to.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-bindings';
const DOMAIN = 'widget';

function binding(action: string, id: string, summary: string, extraProps: Record<string, unknown> = {}, extraRequired: readonly string[] = []): CapabilityRecordSource {
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
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Widget_Text', bindingSource: 'PlayerName' },
    exampleOutput: { success: true, message: `${action} bound` },
  });
}

export const WIDGET_BINDINGS_RECORDS: readonly CapabilityRecordSource[] = [
  binding('create_property_binding', 'blueprint.create_property_binding', 'Create a property binding between a widget property and a Blueprint variable or function.'),
  binding('bind_text', 'blueprint.bind_text', 'Bind the Text property of a widget to a Blueprint variable or function.'),
  binding('bind_visibility', 'blueprint.bind_visibility', 'Bind the Visibility property of a widget to a Blueprint variable or function.'),
  binding('bind_color', 'blueprint.bind_color', 'Bind the ColorAndOpacity property of a widget to a Blueprint variable or function.'),
  binding('bind_enabled', 'blueprint.bind_enabled', 'Bind the IsEnabled property of a widget to a Blueprint variable or function.'),
  binding('bind_on_clicked', 'blueprint.bind_on_clicked', 'Bind the OnClicked event of a Button widget to a Blueprint function.', {}, []),
  binding('bind_on_hovered', 'blueprint.bind_on_hovered', 'Bind the OnHovered/OnUnhovered events of a widget to Blueprint functions.', { onHoveredFunction: P.onHoveredFunction, onUnhoveredFunction: P.onUnhoveredFunction }, ['onHoveredFunction']),
  binding('bind_on_value_changed', 'blueprint.bind_on_value_changed', 'Bind the OnValueChanged event of a Slider or SpinBox to a Blueprint function.', {}, []),
];
