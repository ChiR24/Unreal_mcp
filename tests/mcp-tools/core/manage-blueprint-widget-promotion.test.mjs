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
  { scenario: 'COMPONENT: add_widget_component placed on the canvas', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_component', { componentType: 'TextBlock', componentName: 'ScoreLabel', parentName: 'RootCanvas', positionX: 32, positionY: 64, sizeX: 200, sizeY: 40, text: 'Score' }), expected: 'success' },
  { scenario: 'COMPONENT: add_widget_component names itself when unnamed', toolName: 'manage_blueprint', arguments: widgetArgs('add_widget_component', { componentType: 'Button' }), expected: 'success' },

  // === STYLING ===
  { scenario: 'STYLE: set_font with an explicit face and size', toolName: 'manage_blueprint', arguments: widgetArgs('set_font', { slotName: 'TitleText', font: '/Engine/EngineFonts/Roboto.Roboto', fontSize: 32 }), expected: 'success' },
  { scenario: 'STYLE: set_font applies the default size', toolName: 'manage_blueprint', arguments: widgetArgs('set_font', { slotName: 'TitleText' }), expected: 'success' },
  { scenario: 'STYLE: set_margin on every edge', toolName: 'manage_blueprint', arguments: widgetArgs('set_margin', { slotName: 'MenuColumn', left: 8, top: 4, right: 8, bottom: 4 }), expected: 'success' },

  // === GAME UI TEMPLATES ===
  { scenario: 'TEMPLATE: add_quest_tracker', toolName: 'manage_blueprint', arguments: widgetArgs('add_quest_tracker', { slotName: 'QuestTracker' }), expected: 'success' },
  { scenario: 'TEMPLATE: create_credits_screen with an explicit path', toolName: 'manage_blueprint', arguments: { action: 'create_credits_screen', name: `WBP_Credits_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'TEMPLATE: create_shop_ui with folder alias and columns', toolName: 'manage_blueprint', arguments: { action: 'create_shop_ui', name: `WBP_Shop_${ts}`, folder: TEST_FOLDER, columns: 3 }, expected: 'success|already exists' },
  { scenario: 'TEMPLATE: create_shop_ui relies on every default', toolName: 'manage_blueprint', arguments: { action: 'create_shop_ui' }, expected: 'success|already exists' },

  // === LOCALIZATION AND BINDING ===
  { scenario: 'BIND: set_localization_key with an explicit namespace', toolName: 'manage_blueprint', arguments: widgetArgs('set_localization_key', { slotName: 'TitleText', key: 'MainMenu_Title', namespace: 'MenuUI' }), expected: 'success' },
  { scenario: 'BIND: set_localization_key defaults the namespace', toolName: 'manage_blueprint', arguments: widgetArgs('set_localization_key', { slotName: 'TitleText', key: 'MainMenu_Subtitle' }), expected: 'success' },
  { scenario: 'BIND: bind_localized_text against a missing table entry', toolName: 'manage_blueprint', arguments: widgetArgs('bind_localized_text', { slotName: 'TitleText', stringTableId: '/Game/UI/ST_Menu.ST_Menu', stringKey: 'Title' }), expected: 'success|not found' },
  { scenario: 'BIND: set_widget_binding with an explicit function', toolName: 'manage_blueprint', arguments: widgetArgs('set_widget_binding', { targetWidget: 'TitleText', property: 'Text', functionName: 'GetTitleText' }), expected: 'success' },
  { scenario: 'BIND: set_widget_binding derives the function name', toolName: 'manage_blueprint', arguments: widgetArgs('set_widget_binding', { targetWidget: 'TitleText', property: 'Visibility' }), expected: 'success' },

  // === QUERIES (before the destructive cases invalidate the slots) ===
  { scenario: 'INFO: get_widget_slot_info', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_slot_info', { slotName: 'TitleText' }), expected: 'success' },

  // === DESTRUCTIVE (last: these invalidate slots the cases above address) ===
  { scenario: 'ACTION: reparent_widget', toolName: 'manage_blueprint', arguments: widgetArgs('reparent_widget', { slotName: 'RenameMe', newParent: 'MenuColumn' }), expected: 'success' },
  { scenario: 'ACTION: rename_widget', toolName: 'manage_blueprint', arguments: widgetArgs('rename_widget', { slotName: 'RenameMe', newName: 'RenamedText' }), expected: 'success' },
  { scenario: 'ACTION: remove_widget', toolName: 'manage_blueprint', arguments: widgetArgs('remove_widget', { slotName: 'RemoveMe' }), expected: 'success' },
  { scenario: 'ACTION: delete_animation', toolName: 'manage_blueprint', arguments: widgetArgs('delete_animation', { animationName: 'Anim_Doomed' }), expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
];

runToolTests('manage-blueprint-widget-promotion', testCases);
