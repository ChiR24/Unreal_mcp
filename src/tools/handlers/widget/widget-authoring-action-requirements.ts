const WIDGET_PATH_ONLY_ACTIONS = [
  'add_canvas_panel',
  'add_horizontal_box',
  'add_vertical_box',
  'add_overlay',
  'add_grid_panel',
  'add_uniform_grid',
  'add_wrap_box',
  'add_scroll_box',
  'add_size_box',
  'add_scale_box',
  'add_border',
  'add_text_block',
  'add_rich_text_block',
  'add_image',
  'add_button',
  'add_check_box',
  'add_slider',
  'add_progress_bar',
  'add_text_input',
  'add_combo_box',
  'add_spin_box',
  'add_list_view',
  'add_tree_view',
  'create_main_menu',
  'create_pause_menu',
  'create_hud_widget',
  'add_health_bar',
  'add_ammo_counter',
  'add_minimap',
  'add_crosshair',
  'add_compass',
  'add_interaction_prompt',
  'add_objective_tracker',
  'add_damage_indicator',
  'get_widget_info',
  'preview_widget'
] as const;

const WIDGET_SLOT_ACTIONS = [
  'set_anchor',
  'set_alignment',
  'set_position',
  'set_size',
  'set_padding',
  'set_z_order',
  'set_render_transform',
  'set_visibility',
  'set_style',
  'set_clipping',
  'bind_text',
  'bind_visibility',
  'bind_color',
  'bind_enabled',
  'bind_on_clicked',
  'bind_on_hovered',
  'bind_on_value_changed'
] as const;

const NAME_ONLY_TEMPLATE_ACTIONS = [
  'create_settings_menu',
  'create_loading_screen',
  'create_inventory_ui',
  'create_dialog_widget',
  'create_radial_menu'
] as const;

const WIDGET_ACTION_REQUIREMENTS: Readonly<Record<string, readonly string[]>> = {
  create_widget_blueprint: ['name'],
  set_widget_parent_class: ['widgetPath', 'parentClass'],
  create_property_binding: ['widgetPath', 'slotName', 'propertyName'],
  create_widget_animation: ['widgetPath', 'animationName'],
  add_animation_track: ['widgetPath', 'animationName', 'slotName', 'trackType'],
  add_animation_keyframe: ['widgetPath', 'animationName'],
  set_animation_loop: ['widgetPath', 'animationName'],
  // The canonical record declares add_quest_tracker under manage_blueprint with
  // required ['action','widgetPath'], but it was absent here, so the handler
  // refused it with UNKNOWN_ACTION - an advertised action no input could reach.
  add_quest_tracker: ['widgetPath'],
  ...Object.fromEntries(WIDGET_PATH_ONLY_ACTIONS.map((action) => [action, ['widgetPath']])),
  ...Object.fromEntries(WIDGET_SLOT_ACTIONS.map((action) => [action, ['widgetPath', 'slotName']])),
  ...Object.fromEntries(NAME_ONLY_TEMPLATE_ACTIONS.map((action) => [action, ['name']]))
};

export function getWidgetActionRequiredFields(action: string): readonly string[] | undefined {
  return WIDGET_ACTION_REQUIREMENTS[action];
}
