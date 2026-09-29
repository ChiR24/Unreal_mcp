#!/usr/bin/env node
/**
 * manage_blueprint widget-authoring promotion suite.
 *
 * Covers the eighteen WidgetAuthoring routes promoted from the route ledger:
 * implemented natively, but previously unreachable because no canonical parent
 * named them.
 *
 * These cases carry a second, static duty. `npm run test:params
 * --optional-strict` reads this file WITHOUT executing it and fails when a
 * declared action has no case, or when an optional parameter declared in a
 * capability record never appears as a top-level `arguments` key. So every
 * optional in the eighteen records is referenced at least once below, and the
 * union here is exactly the union declared there.
 *
 * Setup builds a widget tree first: the manipulation, styling, and localization
 * actions all resolve a named slot, so they need real widgets to address.
 * Destructive actions run last so they cannot invalidate the slots the earlier
 * cases read.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/WidgetPromotion';
const ts = Date.now();
const WIDGET_NAME = `WBP_Promotion_${ts}`;
const WIDGET_PATH = `${TEST_FOLDER}/${WIDGET_NAME}`;

const widgetArgs = (action, extra = {}) => ({ action, widgetPath: WIDGET_PATH, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create widget blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: WIDGET_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: add root canvas panel', toolName: 'manage_blueprint', arguments: widgetArgs('add_canvas_panel', { slotName: 'RootCanvas' }), expected: 'success|already exists' },
  { scenario: 'Setup: add text block for styling and localization', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'TitleText', text: 'Title' }), expected: 'success|already exists' },
  { scenario: 'Setup: add vertical box as a reparent target', toolName: 'manage_blueprint', arguments: widgetArgs('add_vertical_box', { slotName: 'MenuColumn' }), expected: 'success|already exists' },
  { scenario: 'Setup: add text block to rename', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'RenameMe', text: 'Rename' }), expected: 'success|already exists' },
  { scenario: 'Setup: add text block to remove', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'RemoveMe', text: 'Remove' }), expected: 'success|already exists' },
  { scenario: 'Setup: create widget animation to delete', toolName: 'manage_blueprint', arguments: widgetArgs('create_widget_animation', { animationName: 'Anim_Doomed' }), expected: 'success|already exists' },

  // === PANELS: layout containers ===
  { scenario: 'PANEL: add_spacer with explicit size', toolName: 'manage_blueprint', arguments: widgetArgs('add_spacer', { slotName: 'GapWide', parentSlot: 'RootCanvas', sizeX: 240, sizeY: 16 }), expected: 'success' },
  { scenario: 'PANEL: add_spacer relies on default size', toolName: 'manage_blueprint', arguments: widgetArgs('add_spacer'), expected: 'success' },
  { scenario: 'PANEL: add_safe_zone', toolName: 'manage_blueprint', arguments: widgetArgs('add_safe_zone', { slotName: 'TitleSafeArea', parentSlot: 'RootCanvas' }), expected: 'success' },
  { scenario: 'PANEL: add_safe_zone relies on default slot name', toolName: 'manage_blueprint', arguments: widgetArgs('add_safe_zone'), expected: 'success' },
  { scenario: 'PANEL: add_widget_switcher with active index', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_switcher', { slotName: 'PageSwitcher', parentSlot: 'RootCanvas', activeIndex: 0 }), expected: 'success' },

  // === COMPONENTS ===
  { scenario: 'COMPONENT: add_widget_component placed on the canvas', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_component', { componentType: 'TextBlock', slotName: 'ScoreLabel', parentSlot: 'RootCanvas', positionX: 32, positionY: 64, sizeX: 200, sizeY: 40, text: 'Score' }), expected: 'success' },
  { scenario: 'COMPONENT: add_widget_component names itself when unnamed', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_component', { componentType: 'Button' }), expected: 'success' },

  // === NAMES: a widget compiles to a member, so a taken name is refused before anything is built ===
  { scenario: 'NAME: add_text_block named like an inherited property is refused', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'DisplayLabel', parentSlot: 'RootCanvas', text: 'Label' }), expected: 'error|NAME_CONFLICT' },
  { scenario: 'NAME: add_image under a text block name is refused', toolName: 'manage_blueprint', arguments: widgetArgs('add_image', { slotName: 'TitleText', parentSlot: 'RootCanvas' }), expected: 'error|NAME_CONFLICT' },
  { scenario: 'NAME: add_widget_component under a text block name is refused', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_component', { componentType: 'Image', slotName: 'TitleText' }), expected: 'error|NAME_CONFLICT' },
  { scenario: 'NAME: a second unnamed add gets a free name', toolName: 'manage_blueprint', arguments: widgetArgs('add_spacer', { parentSlot: 'RootCanvas' }), expected: 'success', assertions: [{ path: 'structuredContent.result.slotName', includes: 'Spacer_', label: 'the first Spacer is left alone' }] },

  // Re-using a slotName edits that widget in place; the funnel says so, since the caller may have meant a second widget.
  { scenario: 'NAME: re-adding a taken slotName still warns that the widget was already in the tree', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'TitleText', parentSlot: 'RootCanvas', text: 'Title' }), expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'was already in the tree', label: 'the slotName re-use warning is kept' }] },

  // === STYLING ===
  { scenario: 'STYLE: set_font with an explicit face and size', toolName: 'manage_blueprint', arguments: widgetArgs('set_font', { slotName: 'TitleText', font: '/Engine/EngineFonts/Roboto.Roboto', fontSize: 32 }), expected: 'success' },
  { scenario: 'STYLE: set_font applies the default size', toolName: 'manage_blueprint', arguments: widgetArgs('set_font', { slotName: 'TitleText' }), expected: 'success' },
  { scenario: 'STYLE: set_margin on every edge', toolName: 'manage_blueprint', arguments: widgetArgs('set_margin', { slotName: 'MenuColumn', left: 8, top: 4, right: 8, bottom: 4 }), expected: 'success' },

  // === GAME UI TEMPLATES ===

  // === LOCALIZATION AND BINDING ===
  { scenario: 'BIND: set_localization_key with an explicit namespace', toolName: 'manage_blueprint', arguments: widgetArgs('set_localization_key', { slotName: 'TitleText', key: 'MainMenu_Title', namespace: 'MenuUI' }), expected: 'success' },
  { scenario: 'BIND: set_localization_key defaults the namespace', toolName: 'manage_blueprint', arguments: widgetArgs('set_localization_key', { slotName: 'TitleText', key: 'MainMenu_Subtitle' }), expected: 'success' },
  { scenario: 'BIND: bind_localized_text against a missing table entry', toolName: 'manage_blueprint', arguments: widgetArgs('bind_localized_text', { slotName: 'TitleText', stringTableId: '/Game/UI/ST_Menu.ST_Menu', stringKey: 'Title' }), expected: 'success|not found' },

  // === QUERIES (before the destructive cases invalidate the slots) ===
  { scenario: 'INFO: get_widget_slot_info', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_slot_info', { slotName: 'TitleText' }), expected: 'success' },

  // === DESTRUCTIVE (last: these invalidate slots the cases above address) ===
  // A reparent moves a seated widget on purpose; the add funnel's "was already in the tree" warning is for an add that re-uses a slotName.
  { scenario: 'ACTION: reparent_widget', toolName: 'manage_blueprint', arguments: widgetArgs('reparent_widget', { slotName: 'RenameMe', newParent: 'MenuColumn' }), expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings', length: 0, label: 'a move is not reported as a duplicate add' }, { path: 'structuredContent.receipt.changes', length: 1, label: 'the widget is listed once, not as a package path and again as an object path' }] },
  // reparent_widget keeps the slot layout and takes an index; the same parent reorders.
  { scenario: 'Setup: second child of MenuColumn', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'OrderMe', parentSlot: 'MenuColumn', text: 'Order' }), expected: 'success' },
  { scenario: 'Setup: pad the second child', toolName: 'manage_blueprint', arguments: widgetArgs('set_margin', { slotName: 'OrderMe', top: 34 }), expected: 'success' },
  { scenario: 'ACTION: reparent_widget reorders within its parent and keeps the padding', toolName: 'manage_blueprint', arguments: widgetArgs('reparent_widget', { slotName: 'OrderMe', newParent: 'MenuColumn', index: 0 }), expected: 'success', assertions: [{ path: 'structuredContent.result.index', equals: 0, label: 'moved to the front' }, { path: 'structuredContent.result.applied.padding.top', equals: 34, label: 'padding kept' }] },
  { scenario: 'ACTION: reparent_widget into its own subtree is refused', toolName: 'manage_blueprint', arguments: widgetArgs('reparent_widget', { slotName: 'RootCanvas', newParent: 'MenuColumn' }), expected: 'error|INVALID_PARENT' },
  // duplicate_widget copies a panel with its children, named and placed on request.
  { scenario: 'ACTION: duplicate_widget copies a subtree beside the original', toolName: 'manage_blueprint', arguments: widgetArgs('duplicate_widget', { slotName: 'MenuColumn', newName: 'MenuColumnCopy' }), expected: 'success', assertions: [{ path: 'structuredContent.result.slotName', equals: 'MenuColumnCopy', label: 'the copy takes newName' }] },
  { scenario: 'ACTION: duplicate_widget into another panel at an index', toolName: 'manage_blueprint', arguments: widgetArgs('duplicate_widget', { slotName: 'OrderMe', newParent: 'MenuColumnCopy', index: 0 }), expected: 'success', assertions: [{ path: 'structuredContent.result.index', equals: 0, label: 'placed first' }] },
  { scenario: 'ACTION: rename_widget', toolName: 'manage_blueprint', arguments: widgetArgs('rename_widget', { slotName: 'RenameMe', newName: 'RenamedText' }), expected: 'success' },
  { scenario: 'ACTION: remove_widget', toolName: 'manage_blueprint', arguments: widgetArgs('remove_widget', { slotName: 'RemoveMe' }), expected: 'success' },
  { scenario: 'ACTION: delete_animation', toolName: 'manage_blueprint', arguments: widgetArgs('delete_animation', { animationName: 'Anim_Doomed' }), expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
];

runToolTests('manage-blueprint-widget-promotion', testCases);
