// Fold specs for manage_level. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_LEVEL_FOLDS: readonly FoldSpec[] = [
  { primary: 'load', summary: 'Load a level in the editor.', members: ['load_level'] },
  {
    primary: 'save', selector: 'saveMode',
    summary: 'Save the current or a named level, or save it under a new path.',
    topics: [],
    members: { save: 'save', as: 'save_as' },
    aliasMembers: ['save_level', 'save_level_as'],
  },
  {
    primary: 'duplicate_level', selector: 'levelOp',
    summary: 'Duplicate a level to a new path, or rename it.',
    members: { duplicate: 'duplicate_level', rename: 'rename_level' },
  },
  { primary: 'delete', summary: 'Delete one or more level assets.', members: ['delete_level'] },
  {
    primary: 'get_summary', selector: 'info',
    summary: 'Read a level summary, or the currently loaded level.',
    topics: ['level summary', 'current level', 'what is the current level'],
    members: { summary: 'get_summary', current_level: 'get_current_level' },
  },
  {
    primary: 'stream', selector: 'streamOp',
    summary: 'Stream a sublevel in or out, or unload it.',
    members: { stream: 'stream', unload: 'unload' },
  },
];
