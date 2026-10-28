#!/usr/bin/env node
// manage_blueprint viewmodels (MVVM): a Blueprint viewmodel with a FieldNotify float, added to a HUD widget and bound
// one way to a progress bar's Percent; a float bound to a text's Text without a conversion is refused and leaves
// nothing; a binding and then the viewmodel are removed. Needs the ModelViewViewModel plugin on (UE 5.4+).
// (tests/mcp-tools/core/widget-viewmodels.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = '/Game/MCPTest/ViewModels';
const VM = `${FOLDER}/VM_Health_${ts}`;
const WBP = `${FOLDER}/WBP_HealthHud_${ts}`;
const hud = (action, extra = {}) => ({ action, widgetPath: WBP, ...extra });

const testCases = [
  // === SETUP === a viewmodel Blueprint with a float, a widget with a progress bar and a text
  { scenario: 'Setup: create the viewmodel Blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: `VM_Health_${ts}`, savePath: FOLDER, parentClass: '/Script/ModelViewViewModel.MVVMViewModelBase' }, expected: 'success' },
  { scenario: 'Setup: give it a Health float', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: VM, variableName: 'Health', variableType: 'Float' }, expected: 'success' },
  { scenario: 'Setup: create the HUD widget', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', widgetPath: WBP }, expected: 'success' },
  { scenario: 'Setup: add a canvas', toolName: 'manage_blueprint', arguments: hud('add_canvas_panel', { slotName: 'RootCanvas' }), expected: 'success' },
  { scenario: 'Setup: add a progress bar', toolName: 'manage_blueprint', arguments: hud('add_progress_bar', { slotName: 'HealthBar', parentSlot: 'RootCanvas' }), expected: 'success' },
  { scenario: 'Setup: add a text', toolName: 'manage_blueprint', arguments: hud('add_text_block', { slotName: 'HealthText', parentSlot: 'RootCanvas', text: '100' }), expected: 'success' },

  // === FIELD NOTIFY ===
  { scenario: 'ACTION: set_field_notify marks Health', toolName: 'manage_blueprint', arguments: { action: 'set_field_notify', blueprintPath: VM, variableNames: ['Health'], on: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.fieldNotify', equals: true, label: 'marked' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'the viewmodel compiles' },
  ] },
  { scenario: 'ERROR: set_field_notify on a variable that is not there', toolName: 'manage_blueprint', arguments: { action: 'set_field_notify', blueprintPath: VM, variableNames: ['Mana'] }, expected: 'error|VARIABLE_NOT_FOUND' },

  // === LIST + ADD ===
  { scenario: 'READ: list_viewmodels finds the viewmodel and its notifying field', toolName: 'manage_blueprint', arguments: { action: 'list_viewmodels', folder: FOLDER }, expected: 'success', assertions: [
    { path: 'structuredContent.result.blueprints.0.assetPath', equals: VM, label: 'the Blueprint viewmodel' },
    { path: 'structuredContent.result.blueprints.0.fields.0.name', equals: 'Health', label: 'Health notifies' },
  ] },
  { scenario: 'ACTION: add_widget_viewmodel adds it under its own name', toolName: 'manage_blueprint', arguments: hud('add_widget_viewmodel', { viewModelClass: VM, name: 'HealthModel', creation: 'create_instance' }), expected: 'success', assertions: [
    { path: 'structuredContent.result.viewModel.name', equals: 'HealthModel', label: 'named' },
    { path: 'structuredContent.result.viewModel.creation', equals: 'CreateInstance', label: 'the widget makes its own' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'ERROR: add_widget_viewmodel with a class that sends no notifications', toolName: 'manage_blueprint', arguments: hud('add_widget_viewmodel', { viewModelClass: '/Script/Engine.Actor' }), expected: 'error|NOT_A_VIEWMODEL' },
  { scenario: 'ERROR: add_widget_viewmodel under a name the widget uses', toolName: 'manage_blueprint', arguments: hud('add_widget_viewmodel', { viewModelClass: VM, name: 'HealthBar' }), expected: 'error|NAME_TAKEN' },

  // === BIND ===
  { scenario: 'ACTION: bind_widget_viewmodel binds Health to the bar', toolName: 'manage_blueprint', arguments: hud('bind_widget_viewmodel', { source: 'HealthModel.Health', destination: 'HealthBar.Percent', mode: 'one_way' }), expected: 'success', captureResult: { key: 'healthBinding', fromField: 'result.binding.id' }, assertions: [
    { path: 'structuredContent.result.binding.destination', equals: 'HealthBar.Percent', label: 'read back as written' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'the widget compiles with it' },
  ] },
  { scenario: 'ERROR: bind_widget_viewmodel from a field that is not there', toolName: 'manage_blueprint', arguments: hud('bind_widget_viewmodel', { source: 'HealthModel.Stamina', destination: 'HealthBar.Percent' }), expected: 'error|PATH_NOT_FOUND' },
  { scenario: 'ERROR: bind_widget_viewmodel a float to a text without a conversion', toolName: 'manage_blueprint', arguments: hud('bind_widget_viewmodel', { source: 'HealthModel.Health', destination: 'HealthText.Text' }), expected: 'error|BINDING_INVALID' },
  { scenario: 'READ: read_widget_viewmodels lists one viewmodel and one binding', toolName: 'manage_blueprint', arguments: hud('read_widget_viewmodels'), expected: 'success', assertions: [
    { path: 'structuredContent.result.viewModels.length', equals: 1, label: 'one viewmodel' },
    { path: 'structuredContent.result.bindings.length', equals: 1, label: 'the refused binding left nothing' },
    { path: 'structuredContent.result.bindings.0.source', equals: 'HealthModel.Health', label: 'its source' },
  ] },

  // === REMOVE ===
  { scenario: 'ERROR: remove_widget_viewmodel a viewmodel a binding still uses', toolName: 'manage_blueprint', arguments: hud('remove_widget_viewmodel', { viewModel: 'HealthModel' }), consent: { capability: 'blueprint.remove_widget_viewmodel', acknowledge: 'explicit' }, expected: 'error|VIEWMODEL_IN_USE' },
  { scenario: 'ACTION: remove_widget_viewmodel the binding', toolName: 'manage_blueprint', arguments: hud('remove_widget_viewmodel', { bindingId: '${captured:healthBinding}' }), consent: { capability: 'blueprint.remove_widget_viewmodel', acknowledge: 'explicit' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.removed.destination', equals: 'HealthBar.Percent', label: 'the bar binding went' },
  ] },
  { scenario: 'ACTION: remove_widget_viewmodel the viewmodel', toolName: 'manage_blueprint', arguments: hud('remove_widget_viewmodel', { viewModel: 'HealthModel' }), consent: { capability: 'blueprint.remove_widget_viewmodel', acknowledge: 'explicit' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.removed.name', equals: 'HealthModel', label: 'the viewmodel went' },
  ] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
];

runToolTests('widget-viewmodels', testCases);
