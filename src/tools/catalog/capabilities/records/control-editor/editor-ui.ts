/**
 * Editor UI records (2): read_editor_ui, use_editor_ui. The editor's own Slate UI read as an outline and used
 * widget by widget (native Domains/ControlEditor/EditorUi), the last resort for editor features with no API. Input is
 * delivered as a Slate virtual user down the widget's own path: the system cursor never moves and no window is activated.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const F = 'editor_ui';
const D = 'editor';
const WIDGET = {
  type: 'object',
  additionalProperties: false,
  properties: {
    ref: { type: 'string', description: 'Its ref.' },
    type: { type: 'string', description: 'Its Slate type (SButton, SEditableTextBox...).' },
    text: { type: 'string', description: 'What it says.' },
  },
} as const;

export const EDITOR_UI_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'read_editor_ui', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['read editor ui', 'editor window widgets', 'what does the editor show', 'find a button in the editor', 'editor panel contents', 'inspect editor ui'],
    summary: 'Read the editor\'s own UI: its windows (windows: index, title, ref; menus and popups are windows too) and an outline of their widgets, one a line, indented by nesting: e12 SButton "Save All" [disabled]. A widget is listed when it says something (its text, or an icon-only control\'s tooltip), takes keyboard focus or is a control (buttons, check boxes [checked]/[unchecked], combo boxes, spin boxes, sliders, text fields, tabs); layout-only widgets fold away. Each ref names its widget for use_editor_ui for the rest of the editor session (WIDGET_GONE once it is closed). Narrow a big outline with window (an index or part of a title, WINDOW_NOT_FOUND otherwise), from (a ref: only that branch), text (only branches holding those words, with the widgets above them) and depth (nesting levels, 1-40, default 12); maxWidgets (1-1000, default 300) caps the lines, truncated says more were left out.',
    whenToUse: [
      'An editor feature has no capability, and its panel, button or field must be found to use it with use_editor_ui.',
      'What an editor window or dialog shows must be read (a settings panel, a tab, a menu that opened).',
    ],
    whenNotToUse: [
      'A running game\'s UMG widgets are read (use simulate_input with type widget_list).',
      'A capability does the job (search first: this reads the UI, not the data behind it).',
    ],
    inputProps: {
      window: { type: 'string', description: 'A window by its index in windows or part of its title (default every window).' },
      from: { type: 'string', description: 'A ref: only that widget\'s branch.' },
      text: { type: 'string', description: 'Only branches holding these words (any case), with the widgets above them.' },
      depth: { type: 'number', minimum: 1, maximum: 40, description: 'Nesting levels of listed widgets (default 12).' },
      maxWidgets: { type: 'number', minimum: 1, maximum: 1000, description: 'Most lines in the outline (default 300).' },
    },
    required: [],
    outputProps: {
      windows: { type: 'array', description: 'Every editor window.', items: { type: 'object', additionalProperties: false, properties: {
        index: { type: 'number', description: 'What window takes.' },
        title: { type: 'string', description: 'Its title; empty for a menu or popup.' },
        ref: { type: 'string', description: 'Its ref.' },
      } } },
      outline: { type: 'string', description: 'The widgets, one a line: ref, type, "text" and [disabled] [focused] [checked]/[unchecked].' },
      listed: { type: 'number', description: 'Lines in the outline.' },
      truncated: { type: 'boolean', description: 'More widgets were left out past maxWidgets.' },
    },
    effect: 'read',
    exampleInput: { action: 'read_editor_ui', window: '0', text: 'Content Browser' },
    exampleOutput: { success: true, windows: [{ index: 0, title: 'McpTest58 - Unreal Editor', ref: 'e1' }], outline: 'e1 SWindow "McpTest58 - Unreal Editor"\n  e40 SDockTab "Content Browser"', listed: 2, truncated: false },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'use_editor_ui', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['click editor button', 'click a button in the editor', 'type into editor field', 'press editor menu item', 'use the editor ui', 'operate editor panel'],
    summary: 'Use one widget of the editor\'s own UI, by a ref read_editor_ui gave: do click, double_click, right_click, hover, focus, type (text, then Enter when submit), press_key (key: Enter, Escape, F5, Ctrl+S...) or check / uncheck (a check box; nothing when it already is). Delivered as a Slate virtual user with its own pointer and focus, down the widget\'s own path: the system cursor never moves, no window is activated, and a minimized editor works. A dialog a click would open modally is cancelled, never waited on. The reply says what happened: handled (the widget took it), widget (its text after), focus (where that user\'s focus went) and windowsOpened / windowsClosed (titles; an untitled one is a menu or popup, read it with read_editor_ui window). Refusals do nothing: WIDGET_NOT_FOUND, WIDGET_GONE (closed since it was read), WIDGET_NOT_SHOWN (no window holds it on screen now), WIDGET_DISABLED, INVALID_ARGUMENT. It can do whatever the editor\'s UI can, so it asks for elevated consent; use it only where no capability does the job.',
    whenToUse: ['An editor feature with no capability must be operated through its own panel, menu or field.'],
    whenNotToUse: [
      'A running game\'s UI is driven (use simulate_input).',
      'A capability does the job (search first).',
    ],
    inputProps: {
      ref: { type: 'string', description: 'The widget, as read_editor_ui named it (e12).' },
      do: { type: 'string', enum: ['click', 'double_click', 'right_click', 'hover', 'focus', 'type', 'press_key', 'check', 'uncheck'], description: 'What to do with it.' },
      text: { type: 'string', description: 'type: the text to type.' },
      key: { type: 'string', description: 'press_key: the key, with Ctrl+, Shift+, Alt+ or Cmd+ as needed (Enter, Escape, Delete, F5, Ctrl+S).' },
      submit: { type: 'boolean', description: 'type: press Enter after the text (default false).' },
    },
    required: ['ref', 'do'],
    outputProps: {
      did: { type: 'string', description: 'What was done ("nothing: check already" when a check box already was).' },
      widget: { ...WIDGET, description: 'The widget, as it is after.' },
      handled: { type: 'boolean', description: 'The widget (or one around it) took the input.' },
      focus: { ...WIDGET, description: 'Where the virtual user\'s keyboard focus is after.' },
      windowsOpened: { type: 'array', items: { type: 'string' }, description: 'Windows that opened: titles; an untitled menu or popup by its ref.' },
      windowsClosed: { type: 'array', items: { type: 'string' }, description: 'Windows that closed.' },
    },
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    exampleInput: { action: 'use_editor_ui', ref: 'e57', do: 'type', text: 'Rock', submit: true },
    exampleOutput: { success: true, did: 'type', handled: true, widget: { ref: 'e57', type: 'SSearchBox', text: 'Rock' }, windowsOpened: [], windowsClosed: [] },
  }),
];
