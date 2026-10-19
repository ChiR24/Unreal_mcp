// Fold specs for manage_level. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_LEVEL_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'load', members: ['load_level'],
    summary: 'Open a level in the editor in place of the open one (levelPath: a /Game package or object path, or a bare name tried under /Game/Maps), or with streaming:true add it to the open level as a sublevel instead (add_sublevel answers). Open levels with unsaved changes refuse the load (DIRTY_PACKAGES, listed in unsavedPackages) until saveDirtyPackages saves them or discardUnsaved drops them; the level already open is not reloaded (alreadyLoaded, with its unsaved state); a missing map answers FILE_NOT_FOUND with both paths it checked. The reply waits for the level to settle (settledSeconds), so a screenshot right after shows it drawn.',
  },
  {
    primary: 'save', selector: 'saveMode',
    summary: 'Save the current or a named level, or save it under a new path.',
    topics: [],
    members: { save: 'save', as: 'save_as' },
    aliasMembers: ['save_level', 'save_level_as'],
  },
  {
    primary: 'duplicate_level', topics: ['copy level', 'copy map'], selector: 'levelOp',
    summary: 'Copy a level map to destinationPath (levelOp duplicate, the default), or rename or move it (rename: newName in the same folder, or destinationPath). Both copy the saved files (the .umap, its built data and its external actor and object files), so unsaved changes to an open level are not in the copy: save first. A destination that exists is refused (DESTINATION_EXISTS) unless overwrite is true, which backs it up and restores it if a step fails; one loaded in the editor is refused (DESTINATION_LOADED). Rename then deletes the source files, and refuses a level loaded in the editor, open or as a sublevel (LEVEL_IN_USE, LEVEL_LOADED). Neither opens the result.',
    members: { duplicate: 'duplicate_level', rename: 'rename_level' },
  },
  { primary: 'delete', summary: 'Delete one level map for good: its .umap, built data and external actor and object files, after taking it out of the open level\'s streaming list. The open level, or one still loaded, is refused (LEVEL_LOADED): open another map first. No undo and no backup; deleted says whether every file went.', members: ['delete_level'] },
  {
    primary: 'get_summary', selector: 'info',
    summary: 'Read a level (path, name, actor count, asset data) or the one open in the editor, and whether it has unsaved changes, with every unsaved level and asset listed.',
    topics: ['level summary', 'current level', 'what is the current level', 'unsaved changes'],
    members: { summary: 'get_summary', current_level: 'get_current_level' },
  },
  {
    primary: 'stream', topics: ['load sublevel', 'unload sublevel', 'hide sublevel'], selector: 'streamOp',
    summary: 'Stream in a sublevel of the open level (streamOp stream, the default) or stream it out (unload). During Play it streams in the running game, as the Load and Unload Stream Level nodes do, and settles before the reply; otherwise the editor, which keeps every sublevel loaded, shows or hides it as the Levels panel does, and unload hides it with a warning. The reply says which world it acted on (play or editor) and whether the level is loaded and visible now. levelPath is the full package path or the part after its last slash; STREAMING_LEVEL_NOT_FOUND means it is not a sublevel yet (add_sublevel), AMBIGUOUS_LEVEL that two sublevels share the name (listed in matches), LEVEL_NOT_LOADED that its map failed to load.',
    members: { stream: 'stream', unload: 'unload' },
  },
];
