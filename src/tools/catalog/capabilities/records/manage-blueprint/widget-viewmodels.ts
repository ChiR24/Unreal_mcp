/**
 * Viewmodel (MVVM) records (6): list_viewmodels, read_widget_viewmodels, add_widget_viewmodel, bind_widget_viewmodel,
 * remove_widget_viewmodel, set_field_notify.
 *
 * The native handler (Domains/WidgetAuthoring/ViewModel) works by reflection on the ModelViewViewModel plugin, a Beta
 * plugin a project may leave off, so nothing links it; the names it reads are the same from UE 5.4 to 5.8. Every
 * record answers MVVM_DISABLED where the plugin is off.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'viewmodel';
const DOMAIN = 'widget';
const PLUGINS = [...WIDGET_PLUGINS, 'ModelViewViewModel'];
const SINCE = '5.4';

const FIELD = {
  type: 'object', additionalProperties: false, properties: {
    name: { type: 'string' },
    type: { type: 'string', description: 'Its C++ type (float, int32, FText, bool...).' },
    function: { type: 'boolean', description: 'true: a getter function the class notifies through.' },
  },
};
const CLASS_ROW = {
  type: 'object', additionalProperties: false, properties: {
    class: { type: 'string', description: 'The class path, which add_widget_viewmodel takes.' },
    assetPath: { type: 'string', description: 'The Blueprint asset (Blueprint viewmodels).' },
    fields: { type: 'array', items: FIELD, description: 'The fields marked FieldNotify, which a binding reads changes from.' },
  },
};
const VIEWMODEL_ROW = {
  type: 'object', additionalProperties: false, properties: {
    name: { type: 'string', description: 'Its name in the widget, the head of a binding path.' },
    class: { type: 'string' },
    id: { type: 'string' },
    creation: { type: 'string', description: 'How the widget gets it: CreateInstance (makes its own), Manual (the game sets it), or another of the editor\'s creation types.' },
  },
};
const BINDING_ROW = {
  type: 'object', additionalProperties: false, properties: {
    id: { type: 'string', description: 'What remove_widget_viewmodel bindingId takes.' },
    source: { type: 'string', description: 'Head.Field...: a viewmodel, a widget of the tree, or Self, then its fields.' },
    destination: { type: 'string' },
    mode: { type: 'string', description: 'one_way (source to destination whenever it changes), two_way, one_time, to_source.' },
    enabled: { type: 'boolean' },
    conversion: { type: 'boolean', description: 'true: the binding runs a conversion function.' },
  },
};
const WRITTEN = {
  compiled: { type: 'boolean', description: 'Whether the Blueprint compiled afterwards.' },
  saved: { type: 'boolean', description: 'Whether it was saved.' },
  undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}; the edit is one step.' },
};
const PATH = 'Head.Field...: the head is a viewmodel of the widget (by its name), a widget of its tree, or Self (the Widget Blueprint), then the fields one after another (Health.Current, HealthBar.Percent, Player.Stats.Armor). A field is a property or a getter function.';

export const WIDGET_VIEWMODEL_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.list_viewmodels', action: 'list_viewmodels', family: FAMILY, domain: DOMAIN,
    topics: ['viewmodel', 'mvvm', 'view model', 'list viewmodels', 'viewmodel classes', 'model view viewmodel'],
    summary: 'List the viewmodel classes a widget can bind to (MVVM): Blueprints whose class derives from MVVMViewModelBase under folder (default /Game, the first 200) and native (C++) ones, each with the fields it notifies through: properties and getter functions marked FieldNotify, which are what a binding reads changes from (set_field_notify marks a Blueprint\'s variables). A viewmodel Blueprint is made with create_blueprint, parent class /Script/ModelViewViewModel.MVVMViewModelBase. Needs the ModelViewViewModel plugin (MVVM_DISABLED otherwise; enable_plugin and a restart turn it on). Reads only.',
    whenToUse: ['A widget should show game data through a viewmodel: which viewmodels exist and what can they be bound by?'],
    whenNotToUse: ['A widget\'s own viewmodels and bindings are wanted (use read_widget_viewmodels).'],
    inputProps: { folder: { type: 'string', description: 'Where to look for Blueprint viewmodels (default /Game).' } },
    required: [],
    outputProps: {
      blueprints: { type: 'array', items: CLASS_ROW, description: 'Blueprint viewmodels under folder.' },
      native: { type: 'array', items: CLASS_ROW, description: 'Native viewmodel classes, any module.' },
    },
    effect: 'read', latency: 'interactive', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'list_viewmodels', folder: '/Game/UI' },
    exampleOutput: { success: true, blueprints: [{ class: '/Game/UI/VM_Health.VM_Health_C', assetPath: '/Game/UI/VM_Health', fields: [{ name: 'Health', type: 'float' }] }], native: [] },
  }),
  buildRecord({
    id: 'blueprint.read_widget_viewmodels', action: 'read_widget_viewmodels', family: FAMILY, domain: DOMAIN,
    topics: ['mvvm bindings', 'viewmodel bindings', 'viewmodels of a widget', 'what a widget binds to viewmodels'],
    summary: 'Read a Widget Blueprint\'s viewmodels (name, class, id, how the widget gets it) and its viewmodel bindings (id, source and destination as Head.Field paths, mode, enabled, whether it runs a conversion function). Empty lists for a widget with no viewmodel yet. Needs the ModelViewViewModel plugin (MVVM_DISABLED otherwise). Reads only.',
    whenToUse: ['Before binding or removing: what does this widget already show through viewmodels?'],
    whenNotToUse: ['Property bindings made with bind_text and the like are wanted (get_widget_info).'],
    inputProps: { widgetPath: P.widgetPath },
    required: ['widgetPath'],
    outputProps: {
      widgetPath: P.widgetPath,
      viewModels: { type: 'array', items: VIEWMODEL_ROW },
      bindings: { type: 'array', items: BINDING_ROW },
    },
    effect: 'read', latency: 'instant', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'read_widget_viewmodels', widgetPath: '/Game/UI/WBP_HUD' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', viewModels: [{ name: 'VM_Health', class: '/Game/UI/VM_Health.VM_Health_C', id: '6F9C1B2A-0000-0000-0000-000000000001', creation: 'CreateInstance' }], bindings: [] },
  }),
  buildRecord({
    id: 'blueprint.add_widget_viewmodel', action: 'add_widget_viewmodel', family: FAMILY, domain: DOMAIN,
    topics: ['add viewmodel to widget', 'use a viewmodel in a widget', 'mvvm widget'],
    summary: 'Add a viewmodel to a Widget Blueprint, as the widget editor\'s Viewmodels panel does: viewModelClass is a viewmodel class or Blueprint (list_viewmodels lists them), name its name in the widget (default the editor\'s, from the class), creation create_instance (the widget makes its own, default; an abstract class is manual) or manual (the game hands one over with the widget\'s setter). One undo step; compiled and saved. Refusals change nothing: NOT_A_VIEWMODEL (the class sends no field notifications), NAME_TAKEN, CLASS_NOT_FOUND, NOT_FOUND (no Widget Blueprint there), MVVM_DISABLED.',
    whenToUse: ['A widget must show a viewmodel\'s data: add the viewmodel, then bind_widget_viewmodel.'],
    whenNotToUse: ['A viewmodel class itself must be made (create_blueprint with parent class /Script/ModelViewViewModel.MVVMViewModelBase).'],
    inputProps: {
      widgetPath: P.widgetPath,
      viewModelClass: { type: 'string', description: 'The viewmodel: a class path (/Script/MyGame.HealthViewModel, /Game/UI/VM_Health.VM_Health_C) or a Blueprint asset (/Game/UI/VM_Health).' },
      name: { type: 'string', description: 'Its name in the widget, which binding paths start with (default the editor\'s).' },
      creation: { type: 'string', enum: ['create_instance', 'manual'], description: 'create_instance: the widget makes its own (default; an abstract class cannot be made, and is manual); manual: the game sets one.' },
    },
    required: ['widgetPath', 'viewModelClass'],
    outputProps: { widgetPath: P.widgetPath, viewModel: VIEWMODEL_ROW, ...WRITTEN },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'add_widget_viewmodel', widgetPath: '/Game/UI/WBP_HUD', viewModelClass: '/Game/UI/VM_Health', name: 'Health' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', viewModel: { name: 'Health', class: '/Game/UI/VM_Health.VM_Health_C', id: '6F9C1B2A-0000-0000-0000-000000000001', creation: 'CreateInstance' }, compiled: true, saved: true },
  }),
  buildRecord({
    id: 'blueprint.bind_widget_viewmodel', action: 'bind_widget_viewmodel', family: FAMILY, domain: DOMAIN,
    topics: ['bind widget to viewmodel', 'mvvm binding', 'viewmodel binding', 'show viewmodel value in widget', 'bind health bar'],
    summary: `Bind a widget property to a viewmodel field (MVVM), as the View Bindings panel does: source and destination are paths, ${PATH} mode one_way (the destination follows the source whenever it changes, default), two_way, one_time or to_source (the source follows the destination: a slider writing a viewmodel). The Widget Blueprint is compiled with the binding and saved; a binding the MVVM compiler rejects (types that do not match without a conversion function, a source field that is not FieldNotify) is taken out again, leaving no undo step, and answers BINDING_INVALID with the compiler's message. A path naming nothing that is there is PATH_NOT_FOUND (with the viewmodels and widgets that are), a widget that does not compile beforehand WIDGET_DOES_NOT_COMPILE; refusals change nothing. Conversion functions are not made here.`,
    whenToUse: ['A widget must show a viewmodel field (a health bar\'s Percent from the viewmodel\'s Health), or write one back.'],
    whenNotToUse: ['A plain property binding to a Blueprint function is wanted (bind_text, bind_percent and the like).'],
    inputProps: {
      widgetPath: P.widgetPath,
      source: { type: 'string', description: `The value bound from, usually the viewmodel side: ${PATH}` },
      destination: { type: 'string', description: 'The value bound to, usually a widget property (HealthBar.Percent).' },
      mode: { type: 'string', enum: ['one_way', 'two_way', 'one_time', 'to_source'], description: 'Default one_way.' },
    },
    required: ['widgetPath', 'source', 'destination'],
    outputProps: { widgetPath: P.widgetPath, binding: BINDING_ROW, ...WRITTEN },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'bind_widget_viewmodel', widgetPath: '/Game/UI/WBP_HUD', source: 'Health.Health', destination: 'HealthBar.Percent' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', binding: { id: '0D3F2E1C-0000-0000-0000-000000000002', source: 'Health.Health', destination: 'HealthBar.Percent', mode: 'one_way', enabled: true, conversion: false }, compiled: true, saved: true },
  }),
  buildRecord({
    id: 'blueprint.remove_widget_viewmodel', action: 'remove_widget_viewmodel', family: FAMILY, domain: DOMAIN,
    topics: ['remove viewmodel binding', 'unbind viewmodel', 'remove viewmodel from widget'],
    summary: 'Remove one viewmodel binding (bindingId, from read_widget_viewmodels) or one viewmodel (viewModel, by name) from a Widget Blueprint. A viewmodel bindings still use is VIEWMODEL_IN_USE (their ids: remove them first); a binding that runs a conversion function is BINDING_HAS_CONVERSION (the editor removes its graph with it: the View Bindings panel). One undo step; compiled and saved. Refusals change nothing: BINDING_NOT_FOUND, VIEWMODEL_NOT_FOUND, VIEWMODEL_LOCKED, NOT_FOUND, MVVM_DISABLED.',
    whenToUse: ['A viewmodel binding or a viewmodel the widget no longer needs.'],
    whenNotToUse: ['A binding should only stop for now (the View Bindings panel can disable one).'],
    inputProps: {
      widgetPath: P.widgetPath,
      bindingId: { type: 'string', description: 'The binding\'s id, as read_widget_viewmodels lists it.' },
      viewModel: { type: 'string', description: 'The viewmodel\'s name in the widget.' },
    },
    required: ['widgetPath'],
    requiredOneOf: ['bindingId', 'viewModel'],
    outputProps: {
      widgetPath: P.widgetPath,
      removed: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The binding or viewmodel removed, as read_widget_viewmodels showed it.' },
      ...WRITTEN,
    },
    effect: 'destructive', latency: 'interactive', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'remove_widget_viewmodel', widgetPath: '/Game/UI/WBP_HUD', bindingId: '0D3F2E1C-0000-0000-0000-000000000002' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', removed: { id: '0D3F2E1C-0000-0000-0000-000000000002' }, compiled: true, saved: true },
  }),
  buildRecord({
    id: 'blueprint.set_field_notify', action: 'set_field_notify', family: FAMILY, domain: DOMAIN,
    topics: ['field notify', 'fieldnotify variable', 'notify field changes', 'viewmodel variable'],
    summary: 'Turn FieldNotify on (default) or off for Blueprint variables, as the variable\'s FieldNotify box does: a FieldNotify variable tells its listeners each time a Set node changes it, which is what a viewmodel binding reads. Works on a viewmodel Blueprint (parent class MVVMViewModelBase) and on a Widget Blueprint; any other Blueprint is NOT_NOTIFYING. A name that is no variable of it is VARIABLE_NOT_FOUND (with the variables it has). One undo step; compiled and saved.',
    whenToUse: ['A viewmodel Blueprint\'s variable must be bindable (add_variable makes it, this marks it).'],
    whenNotToUse: ['Other variable metadata is set (set_variable_metadata).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      variableNames: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 32, description: 'The variables.' },
      on: { type: 'boolean', description: 'false turns FieldNotify off (default true).' },
    },
    required: ['blueprintPath', 'variableNames'],
    outputProps: {
      blueprintPath: P.blueprintPath,
      variableNames: { type: 'array', items: { type: 'string' } },
      fieldNotify: { type: 'boolean', description: 'What the variables are now.' },
      ...WRITTEN,
    },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: PLUGINS, minEngineVersion: SINCE,
    exampleInput: { action: 'set_field_notify', blueprintPath: '/Game/UI/VM_Health', variableNames: ['Health'] },
    exampleOutput: { success: true, blueprintPath: '/Game/UI/VM_Health', variableNames: ['Health'], fieldNotify: true, compiled: true, saved: true },
  }),
];
