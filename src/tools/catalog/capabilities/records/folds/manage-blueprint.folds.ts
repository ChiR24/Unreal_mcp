// Fold specs for manage_blueprint. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_BLUEPRINT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create', selector: 'createMode',
    summary: 'Create a Blueprint asset from a parent class; optionally typed, or only if it does not exist yet.',
    members: { create: 'create', blueprint: 'create_blueprint', ensure: 'ensure_exists' },
  },
  {
    primary: 'get_blueprint', selector: 'info',
    summary: 'Read a Blueprint summary (variables, functions, components) or one of its default property values.',
    topics: ['get blueprint info', 'blueprint info', 'list blueprint variables', 'blueprint details', 'blueprint property'],
    members: { blueprint: 'get_blueprint', property: 'get' },
  },
  {
    primary: 'edit_scs', selector: 'edit',
    summary: 'Edit the Simple Construction Script: add a component template (or a non-SCS instance), reparent, set a property or transform, or apply batched operations.',
    topics: ['add component', 'add component to blueprint', 'static mesh component', 'scs component', 'component transform', 'component property', 'light component', 'collision box component', 'spring arm component', 'make component child'],
    members: {
      add_scs_component: 'add_scs_component', add_component: 'add_component', modify: 'modify_scs', reparent: 'reparent_scs_component',
      set_property: 'set_scs_property', set_transform: 'set_scs_transform',
    },
  },
  {
    primary: 'edit_variable', selector: 'edit',
    summary: 'Add or rename a Blueprint variable, set variable or Blueprint metadata, or set a class default value.',
    topics: ['add variable', 'blueprint variable', 'rename variable', 'default value', 'variable metadata', 'class defaults', 'expose on spawn', 'make variable public'],
    members: byName(['add_variable', 'rename_variable', 'set_variable_metadata', 'set_metadata', 'set_default']),
  },
  {
    primary: 'edit_graph', selector: 'edit',
    summary: 'Edit a Blueprint graph: add or create nodes (including reroute and struct make/break nodes), connect pins, set node properties and pin defaults, add a construction script, arrange nodes beside what they are wired to, or run many of those edits, variables included, in one batch.',
    topics: ['blueprint node', 'create node', 'connect pins in blueprint', 'connect blueprint nodes', 'connect nodes', 'wire pins', 'print string', 'print to screen', 'pin default', 'node property', 'construction script', 'reroute node', 'batch graph edit', 'branch node', 'timeline', 'delay node', 'cast node', 'arrange nodes', 'tidy graph', 'clean up graph layout'],
    members: {
      ...byName(['add_node', 'create_node', 'create_reroute_node', 'create_struct_make_break_nodes', 'connect_pins',
        'set_node_property', 'set_pin_default_value', 'add_construction_script', 'arrange_nodes']),
      batch: 'build_graph',
    },
  },
  {
    primary: 'delete_node', selector: 'deleteScope',
    summary: 'Delete a graph node, or break the links of one of its pins.',
    members: { node: 'delete_node', pin_links: 'break_pin_links' },
  },
  {
    primary: 'add_function', selector: 'kind',
    summary: 'Add a function or an event to a Blueprint; an event can be bound to a component\'s or a Widget Blueprint widget\'s delegate (OnClicked).',
    topics: ['add function', 'add event', 'custom event', 'blueprint function', 'pure function'],
    members: { function: 'add_function', event: 'add_event' },
  },
  {
    primary: 'remove_function', selector: 'kind',
    summary: 'Remove a function or an event from a Blueprint.',
    members: { function: 'remove_function', event: 'remove_event' },
  },
  {
    primary: 'inspect_graph', selector: 'info',
    summary: 'Inspect a Blueprint graph: its nodes (every one, or those whose title, pin default, text or object path matches a filter), node details, pin details, or the available node types.',
    topics: ['graph details', 'node details', 'pin details', 'node types', 'find nodes', 'search graph nodes'],
    members: { graph: 'get_graph_details', node: 'get_node_details', pins: 'get_pin_details', node_types: 'list_node_types' },
  },
  {
    primary: 'edit_widget_blueprint', selector: 'edit',
    summary: 'Create a Widget Blueprint, set its parent class, or rename/reparent a widget in its tree.',
    topics: ['widget blueprint', 'umg', 'rename widget', 'reparent widget', 'widget parent class', 'rename button', 'widget name'],
    members: { create: 'create_widget_blueprint', set_parent_class: 'set_widget_parent_class', rename_widget: 'rename_widget', reparent_widget: 'reparent_widget' },
  },
  {
    primary: 'add_panel_widget', selector: 'widgetKind',
    summary: 'Add a panel widget to a Widget Blueprint: canvas, overlay, boxes, grids, border, scroll/size/scale box, spacer, safe zone, switcher, wrap box, or a whole layout of panels, text and images with their looks in one call (widget_tree).',
    topics: ['canvas panel', 'vertical box', 'horizontal box', 'overlay', 'scroll box', 'grid panel', 'widget switcher', 'umg panel', 'row of buttons', 'stack buttons vertically', 'buttons side by side'],
    members: byTarget('add_', ['add_canvas_panel', 'add_overlay', 'add_vertical_box', 'add_horizontal_box', 'add_grid_panel', 'add_uniform_grid',
      'add_wrap_box', 'add_border', 'add_scroll_box', 'add_size_box', 'add_scale_box', 'add_spacer', 'add_safe_zone', 'add_widget_switcher', 'add_widget_tree']),
  },
  {
    primary: 'add_content_widget', selector: 'widgetKind',
    summary: 'Add a content widget to a Widget Blueprint: text, rich text, image, button, check box, combo box, slider, spin box, progress bar, text input, list/tree view, or any widget class.',
    topics: ['text block', 'button', 'image', 'progress bar', 'slider', 'check box', 'combo box', 'umg widget', 'checkbox', 'dropdown', 'icon'],
    members: {
      ...byTarget('add_', ['add_text_block', 'add_rich_text_block', 'add_image', 'add_button', 'add_check_box', 'add_combo_box', 'add_slider',
        'add_spin_box', 'add_progress_bar', 'add_text_input', 'add_list_view', 'add_tree_view']),
      component: 'add_widget_component',
    },
  },
  {
    primary: 'set_font', selector: 'textStyle',
    summary: 'Set the font or the margin of a widget slot.',
    members: { font: 'set_font', margin: 'set_margin' },
  },
  {
    primary: 'set_widget_layout', selector: 'layoutProperty',
    summary: 'Set a widget slot layout property (anchor, position, size (canvas {x,y}; box child sizeRule Auto/Fill and fillValue weight), alignment, padding, z-order, visibility, clipping, render transform) or its style: the text of a text block or button label, text justification, color, font size, font face, family and letter spacing (or the look of another text block), image texture, opacity, rounded corners, button hover/press sounds.',
    topics: ['widget anchor', 'widget position', 'move widget', 'widget size', 'resize widget', 'make widget bigger', 'widget padding', 'widget visibility', 'z order', 'render transform', 'widget style',
      'change widget text', 'button label', 'widget image', 'widget color', 'button sound', 'click sound', 'hover sound', 'center text', 'text alignment',
      'bold text', 'make text bold', 'change button text', 'hide widget', 'show widget', 'font face', 'letter spacing', 'copy text style', 'fill the screen', 'expand to fill', 'stretch widget', 'tint image', 'make text red', 'align widget', 'bring widget to front', 'rounded corners', 'text outline', 'outline text', 'add outline to text', 'text shadow', 'drop shadow on text'],
    members: byTarget('set_', ['set_anchor', 'set_position', 'set_size', 'set_alignment', 'set_padding', 'set_z_order', 'set_visibility',
      'set_clipping', 'set_render_transform', 'set_style']),
  },
  {
    primary: 'bind_widget', selector: 'bindingKind',
    summary: 'Bind a widget property to a variable or pure function (text, color, enabled, visibility, progress bar percent or slider value; the converting getter is generated), wire an event to a function (on-clicked, hovered, value-changed; the function is created when missing), or bind localized text or a localization key.',
    topics: ['bind text', 'bind visibility', 'bind progress bar', 'on clicked', 'button click', 'button click event', 'click event', 'slider changed', 'property binding', 'localized text', 'localization key', 'handle button click', 'call function on button click', 'bind health bar', 'localize text'],
    members: {
      ...byTarget('bind_', ['bind_text', 'bind_color', 'bind_enabled', 'bind_percent', 'bind_visibility', 'bind_on_clicked', 'bind_on_hovered', 'bind_on_value_changed', 'bind_localized_text']),
      localization_key: 'set_localization_key',
    },
  },
  {
    primary: 'add_game_widget', selector: 'widgetKind',
    summary: 'Add a ready-made HUD piece to a Widget Blueprint: health bar, ammo counter, crosshair, minimap, compass, damage indicator, interaction prompt, objective or quest tracker.',
    topics: ['health bar', 'ammo counter', 'crosshair', 'minimap', 'compass', 'damage indicator', 'interaction prompt', 'objective tracker', 'quest tracker', 'hud element'],
    members: byTarget('add_', ['add_health_bar', 'add_ammo_counter', 'add_crosshair', 'add_minimap', 'add_compass', 'add_damage_indicator',
      'add_interaction_prompt', 'add_objective_tracker', 'add_quest_tracker']),
  },
  {
    primary: 'create_widget_template', selector: 'screen',
    summary: 'Create a new Widget Blueprint holding a ready-made screen: main menu, pause menu, settings menu, loading screen, HUD, dialog box, inventory, radial menu, credits or shop.',
    topics: ['main menu', 'pause menu', 'settings menu', 'loading screen', 'hud', 'dialog box', 'inventory ui', 'radial menu', 'credits screen', 'shop ui', 'title screen', 'settings screen', 'start screen', 'make a start screen', 'build menu'],
    members: {
      main_menu: 'create_main_menu', pause_menu: 'create_pause_menu', settings_menu: 'create_settings_menu', loading_screen: 'create_loading_screen',
      hud: 'create_hud_widget', dialog: 'create_dialog_widget', inventory: 'create_inventory_ui', radial_menu: 'create_radial_menu',
      credits: 'create_credits_screen', shop: 'create_shop_ui',
    },
  },
  {
    primary: 'edit_widget_animation', selector: 'edit',
    summary: 'Create a widget animation or add tracks and keyframes to it.',
    topics: ['widget animation', 'animation track', 'animation keyframe', 'animate widget', 'slide in menu', 'animate widget scale', 'widget scale animation', 'pulse widget', 'pop a widget', 'fade in widget'],
    members: { create: 'create_widget_animation', add_track: 'add_animation_track', add_keyframe: 'add_animation_keyframe' },
  },
  {
    primary: 'get_widget_info', selector: 'info',
    summary: 'Read a Widget Blueprint: its widget tree (each widget with its class and parent, so a panel\'s children), or one slot layout (padding, alignment, size rule).',
    topics: ['read widget layout', 'widget layout', 'slot layout', 'widget children'],
    members: { widget: 'get_widget_info', slot: 'get_widget_slot_info' },
  },
];
