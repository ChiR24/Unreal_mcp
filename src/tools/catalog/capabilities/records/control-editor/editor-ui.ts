/**
 * Editor UI record (1): read_editor_ui. The editor's own Slate UI read as an outline (native
 * Domains/ControlEditor/EditorUi): what an editor window, panel or dialog shows. It only reads; the capabilities do
 * the editor's work.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const F = 'editor_ui';
const D = 'editor';

export const EDITOR_UI_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'read_editor_ui', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    topics: ['read editor ui', 'editor window widgets', 'what does the editor show', 'find a button in the editor', 'editor panel contents', 'inspect editor ui'],
    summary: 'Read the editor\'s own UI: its windows (windows: index, title, ref; menus and popups are windows too) and an outline of their widgets, one a line, indented by nesting: e12 SButton "Save All" [disabled]. A widget is listed when it says something (its text, or an icon-only control\'s tooltip), takes keyboard focus or is a control (buttons, check boxes [checked]/[unchecked], combo boxes, spin boxes, sliders, text fields, tabs); layout-only widgets fold away. Each ref names its widget for the rest of the editor session. Narrow a big outline with window (an index or part of a title, WINDOW_NOT_FOUND otherwise), from (a ref: only that branch; WIDGET_GONE once its widget is closed), text (only branches holding those words, with the widgets above them) and depth (nesting levels, 1-40, default 12); maxWidgets (1-1000, default 300) caps the lines, truncated says more were left out. It only reads: to change something, search for the capability that does it.',
    whenToUse: [
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
];
