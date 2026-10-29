// Fold specs for manage_blueprint. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_BLUEPRINT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create', selector: 'createMode',
    summary: 'Create a Blueprint asset, chosen by createMode (ensure checks first), named name in savePath (default /Game) and save it (saved). parentClass is a native class as /Script/Module.Class or a short name (Actor, Pawn, Character, PlayerController), or a Blueprint class path ending in _C; blueprintType Actor, Pawn, Character or FunctionLibrary stands in for a missing parentClass, and Interface without one makes a Blueprint Interface. A parent that resolves to nothing is CLASS_NOT_FOUND, naming classes of that name. properties sets class defaults, Component.Property and struct members by dotted path, reported as appliedProperties and failedProperties (the create stands either way). A Blueprint already at the path is answered with alreadyExists and left unchanged; one with another parent class is ASSET_ALREADY_EXISTS.',
    members: { create: 'create', blueprint: 'create_blueprint', ensure: 'ensure_exists' },
  },
  {
    primary: 'get_blueprint', selector: 'info',
    summary: 'Read a Blueprint, chosen by info. blueprint returns its parent class, type, compile status (hasCompileErrors), variables (name, type, category, replication, default), functions with their parameters, events, graphs with node counts, components (name, class, parent, root) and class defaults; property reads one value from its class defaults: propertyName is a variable or inherited property, or Component.Property for a component it adds, and a name found nowhere is PROPERTY_NOT_FOUND, listing its variables.',
    topics: ['get blueprint info', 'blueprint info', 'list blueprint variables', 'blueprint details', 'blueprint property'],
    members: { blueprint: 'get_blueprint', property: 'get' },
  },
  {
    primary: 'edit_scs', selector: 'edit',
    summary: 'Change the components a Blueprint adds (its Simple Construction Script), chosen by edit; each edit saves the Blueprint unless save is false. add_scs_component (add_component is the same) adds componentName of componentClass under parentComponent (its own part, a parent Blueprint\'s, or the native root; by default the root), with meshPath and materialPath (refused when they do not load or the class shows no mesh or material), location, rotation, scale and properties, and compiles; a taken name is refused, as is an add during Play. modify applies operations in order (add, modify, attach) or edits one component, naming each failed one in warnings (the call fails when none applied), and compiles only with compile or applyAndSave true; reparent moves a component under another; set_property and set_transform change one template and compile, and placed copies that kept the old value follow (instancesUpdated).',
    topics: ['add component', 'add component to blueprint', 'static mesh component', 'scs component', 'component transform', 'component property', 'light component', 'collision box component', 'spring arm component', 'make a component a child of another'],
    members: {
      add_scs_component: 'add_scs_component', add_component: 'add_component', modify: 'modify_scs', reparent: 'reparent_scs_component',
      set_property: 'set_scs_property', set_transform: 'set_scs_transform',
    },
  },
  {
    primary: 'edit_variable', selector: 'edit',
    summary: 'Change a Blueprint\'s variables, chosen by edit. add_variable adds variableName of variableType with defaultValue, category and the isPublic, isReplicated and exposeOnSpawn flags (a taken name is VARIABLE_NAME_CONFLICT, a type that resolves to nothing TYPE_RESOLUTION_FAILED, a default the type refuses DEFAULT_NOT_APPLIED; nothing is added then); rename_variable renames a variable or a component and its graph nodes follow; set_variable_metadata sets a variable\'s tooltip, category or replication; set_metadata sets metadata pairs on the class or one variable; set_default writes a class default value, an inherited property included.',
    topics: ['add variable', 'blueprint variable', 'rename variable', 'default value', 'variable metadata', 'class defaults', 'expose on spawn', 'make variable public'],
    members: byName(['add_variable', 'rename_variable', 'set_variable_metadata', 'set_metadata', 'set_default']),
  },
  {
    primary: 'edit_graph', selector: 'edit',
    summary: 'Write Blueprint logic of any kind, or edit a graph: add or create nodes (including reroute and struct make/break nodes), connect pins, set node properties and pin defaults, add a construction script, arrange nodes beside what they are wired to, or run many of those edits, variables included, in one batch.',
    topics: ['blueprint logic', 'write blueprint logic', 'blueprint code', 'program a blueprint', 'custom logic', 'custom game mechanic', 'new game mechanic', 'repeat every few seconds', 'blueprint node', 'create node', 'connect pins in blueprint', 'connect blueprint nodes', 'connect nodes', 'wire pins', 'print string', 'print to screen', 'print a message', 'print a message on overlap', 'pin default', 'node property', 'construction script', 'reroute node', 'batch graph edit', 'branch node', 'timeline', 'delay node', 'cast node', 'arrange nodes', 'tidy graph', 'clean up graph layout', 'anim graph node by class', 'add two bone ik node', 'animation blueprint node', 'math expression node', 'blueprint math expression', 'add a math expression', 'math expression in a blueprint', 'for each loop', 'set timer', 'ai move to location', 'move ai to a point', 'random number', 'is valid', 'do once', 'gate node', 'branch on a condition', 'loop over an array', 'cast to player', 'event dispatcher', 'bind event to dispatcher'],
    members: {
      ...byName(['add_node', 'create_node', 'create_reroute_node', 'create_struct_make_break_nodes', 'connect_pins',
        'set_node_property', 'set_pin_default_value', 'add_construction_script', 'arrange_nodes']),
      batch: 'build_graph',
    },
    // The name callers invent for connect_pins (the material graph's word): the handler reads it as connect_pins.
    aliasMembers: { connect_pins: 'connect_nodes' },
    exampleFrom: 'build_graph',
  },
  {
    primary: 'delete_node', selector: 'deleteScope',
    summary: 'Delete Blueprint graph nodes (deleteScope node, the default), or disconnect every wire of one pin (pin_links). node deletes nodeId, or every node in nodeIds (up to 500), all or nothing: an id that matches no node is NODE_NOT_FOUND and a function entry or result node is PROTECTED_NODE, with nothing deleted; their wires go with them, and the reply lists the removed guids. pin_links breaks the links of pinName on nodeId (PIN_NOT_FOUND lists the pins the node has) and names the pins it was wired to under disconnectedFrom; a pin with no links changes nothing. Either is one undo step, then the Blueprint compiles (diagnostics when it no longer compiles) and is saved. A pinName sent with deleteScope node is refused (CONTRADICTORY_SCOPE) rather than deleting the whole node.',
    topics: ['delete node', 'remove node', 'disconnect pins', 'unlink pins', 'break pin links', 'remove wire', 'disconnect wire', 'unwire pin'],
    members: { node: 'delete_node', pin_links: 'break_pin_links' },
  },
  {
    primary: 'add_function', selector: 'kind',
    summary: 'Add a function or an event to a Blueprint; an event can be bound to a component\'s or a Widget Blueprint widget\'s delegate (OnClicked), and a function named after one the parent lets a Blueprint override (a widget\'s OnKeyDown) is made as that override.',
    topics: ['add function', 'add event', 'custom event', 'blueprint function', 'pure function'],
    members: { function: 'add_function', event: 'add_event' },
  },
  {
    primary: 'add_behaviour', selector: 'behaviour',
    summary: 'Write gameplay logic into an Actor Blueprint in one call: health that takes damage and dies, damage on touch, a pickup that heals or counts (coins, keys), a sprint key for a Character, a key that shoots projectiles for a pawn, a pawn that chases the player or patrols a route, a turret that turns to the player and fires at them, an enemy the player defeats by jumping on it, a barrel that explodes when it dies, a moving platform or elevator, walk, look and jump controls for a Character, a pawn that respawns when it falls or dies (with checkpoints), a goal that ends the level when the player reaches it, a pad that launches a Character, a teleporter, a platform that falls when a pawn lands on it, a time limit, a spawner that keeps spawning an actor class, a dash key for a Character, the health of the player or a count such as coins or score on screen, a hint on screen while the player is in an area, or a game over screen that restarts the level when the player dies. Logic none of these covers is written with edit_graph.',
    topics: ['gameplay behaviour', 'add behaviour', 'gameplay logic'],
    members: { health: 'add_health', damage_on_touch: 'add_damage_on_touch', pickup: 'add_pickup', sprint: 'add_sprint', shoot: 'add_shoot', chase: 'add_chase', patrol: 'add_patrol', turret: 'add_turret', stomp: 'add_stomp', explosive: 'add_explosive', moving_platform: 'add_moving_platform', controls: 'add_controls', respawn: 'add_respawn', goal: 'add_goal', launch_pad: 'add_launch_pad', teleporter: 'add_teleporter', falling_platform: 'add_falling_platform', countdown: 'add_countdown', spawner: 'add_spawner', dash: 'add_dash', health_hud: 'add_health_hud', counter_hud: 'add_counter_hud', hint: 'add_hint', game_over: 'add_game_over' },
  },
  {
    primary: 'remove_function', selector: 'kind',
    summary: 'Remove a function the Blueprint defines (kind function: functionName), recompiled and saved; one still called is refused with FUNCTION_IN_USE naming each call node to delete first, and inherited or engine functions are NOT_FOUND. Or remove an event (event: a custom event by eventName, or any event node by nodeId, on graphName or the first event graph page), recompiled and saved; a custom event still called is refused with EVENT_IN_USE naming the call nodes.',
    members: { function: 'remove_function', event: 'remove_event' },
  },
  {
    primary: 'inspect_graph', selector: 'info',
    summary: 'Inspect a Blueprint graph: its nodes (every one, those whose title, pin default, text or object path matches a filter, or the unused ones), node details, pin details, the available node types, or the functions a node can call found by what they do in plain words (game time, random number in range), with the memberName, memberClass and pins create_node takes.',
    topics: ['graph details', 'node details', 'pin details', 'node types', 'find nodes', 'search graph nodes', 'filter graph nodes', 'unused nodes', 'what writes a variable', 'find a blueprint function', 'which function to call', 'function for a node'],
    members: { graph: 'get_graph_details', node: 'get_node_details', pins: 'get_pin_details', node_types: 'list_node_types', functions: 'find_functions' },
  },
  {
    primary: 'edit_widget_blueprint', selector: 'edit',
    summary: 'Create or restructure a Widget Blueprint, chosen by edit. create makes one named name in path (default /Game/UI), or at widgetPath, from parentClass (default UserWidget; a native widget class or a Widget Blueprint class path, anything else INVALID_PARENT_CLASS), with an empty widget tree, then compiles and saves it (compileSucceeded, saveSucceeded); a taken path is ALREADY_EXISTS. set_parent_class reparents it to a UserWidget class or Widget Blueprint, refreshes, compiles and saves (INVALID_CLASS, CYCLIC_PARENT). rename_widget renames slotName to newName and carries graph references, bindings, animations and navigation along (ALREADY_EXISTS, NAME_CONFLICT). reparent_widget moves slotName under the panel newParent at index (default last; the same parent reorders), keeping its slot layout; a move under its own child is INVALID_PARENT.',
    topics: ['widget blueprint', 'umg', 'rename widget', 'reparent widget', 'widget parent class', 'rename button', 'widget name'],
    members: { create: 'create_widget_blueprint', set_parent_class: 'set_widget_parent_class', rename_widget: 'rename_widget', reparent_widget: 'reparent_widget' },
  },
  {
    primary: 'add_panel_widget', selector: 'widgetKind',
    summary: 'Add a panel widget to a Widget Blueprint: canvas, overlay, boxes, grids, border, scroll/size/scale box, spacer, safe zone, switcher, wrap box, or a whole layout of panels, text and images with their looks in one call (widget_tree).',
    topics: ['canvas panel', 'scroll list', 'vertical box', 'horizontal box', 'overlay', 'scroll box', 'grid panel', 'widget switcher', 'umg panel', 'row of buttons', 'stack buttons vertically', 'buttons side by side'],
    members: byTarget('add_', ['add_canvas_panel', 'add_overlay', 'add_vertical_box', 'add_horizontal_box', 'add_grid_panel', 'add_uniform_grid',
      'add_wrap_box', 'add_border', 'add_scroll_box', 'add_size_box', 'add_scale_box', 'add_spacer', 'add_safe_zone', 'add_widget_switcher', 'add_widget_tree']),
  },
  {
    primary: 'add_content_widget', selector: 'widgetKind',
    summary: 'Add a content widget to a Widget Blueprint: text, rich text, image, button, check box, combo box, slider, spin box, progress bar, text input, list/tree view, or any widget class.',
    topics: ['text block', 'button', 'restart button', 'quit button', 'image', 'progress bar', 'slider', 'check box', 'combo box', 'umg widget', 'checkbox', 'dropdown', 'icon'],
    members: {
      ...byTarget('add_', ['add_text_block', 'add_rich_text_block', 'add_image', 'add_button', 'add_check_box', 'add_combo_box', 'add_slider',
        'add_spin_box', 'add_progress_bar', 'add_text_input', 'add_list_view', 'add_tree_view']),
      component: 'add_widget_component',
    },
  },
  {
    primary: 'set_font', selector: 'textStyle',
    summary: 'Style one widget of a Widget Blueprint (widgetPath; slotName is the widget name), chosen by textStyle. font sets the font asset (font, a Font asset path) and the fontSize of a Text Block, Rich Text Block (its default text), Editable Text or Editable Text Box; what is left out keeps its current value, and another widget type is UNSUPPORTED_WIDGET. margin sets left, top, right and bottom (each 0 when left out) as the padding of a widget in a Horizontal Box, Vertical Box or Overlay slot, or of a Border; any other slot, such as a Canvas Panel one, is INVALID_SLOT. The Blueprint is compiled and saved (saved says whether it was written).',
    members: { font: 'set_font', margin: 'set_margin' },
  },
  {
    primary: 'set_widget_layout', selector: 'layoutProperty',
    summary: 'Set a widget slot layout property (anchor, position, size (canvas {x,y}; box child sizeRule Auto/Fill and fillValue weight), alignment, padding, z-order, visibility, clipping, render transform) or its style: the text of a text block or button label, text justification, color, font size, font face, family and letter spacing (or the look of another text block), image texture, opacity, rounded corners, button hover/press sounds.',
    topics: ['widget anchor', 'button hover effect', 'widget position', 'move widget', 'widget size', 'resize widget', 'make widget bigger', 'widget padding', 'widget visibility', 'z order', 'render transform', 'widget style',
      'change widget text', 'button label', 'widget image', 'widget color', 'button sound', 'click sound', 'hover sound', 'center text', 'text alignment',
      'bold text', 'make text bold', 'change button text', 'hide widget', 'show widget', 'font face', 'letter spacing', 'copy text style', 'fill the screen', 'expand to fill', 'stretch widget', 'tint image', 'make text red', 'align widget', 'bring widget to front', 'rounded corners', 'text outline', 'outline text', 'add outline to text', 'text shadow', 'drop shadow on text'],
    members: byTarget('set_', ['set_anchor', 'set_position', 'set_size', 'set_alignment', 'set_padding', 'set_z_order', 'set_visibility',
      'set_clipping', 'set_render_transform', 'set_style']),
  },
  {
    primary: 'bind_widget', selector: 'bindingKind',
    summary: 'Bind a widget property to a variable or pure function (text, color, enabled, visibility, progress bar percent or slider value; the converting getter is generated), wire an event to a function (on-clicked, hovered, value-changed; the function is created when missing), or bind localized text or a localization key.',
    topics: ['bind text', 'bind visibility', 'bind progress bar', 'on clicked', 'button click', 'button click event', 'click event', 'slider changed', 'property binding', 'localized text', 'localization key', 'handle button click', 'call function on button click', 'button click opens a level', 'bind health bar', 'localize text'],
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
    summary: 'Build a widget animation, chosen by edit, each saved: create makes a named animation of a given length (a taken name is refused), add_track gives one widget an opacity, color or render-transform track, add_keyframe keys a value at a time on a widget (adding the track and binding when missing) or a whole animation across several widgets in one call (keys, all checked before any is written). get_widget_animation reads it back.',
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
