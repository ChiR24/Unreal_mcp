// Blackboard keys and defaults, a Behavior Tree's Blackboard binding, and sound class properties and parents are one undo
// step each. They called Modify with no transaction open, which records nothing, so Ctrl+Z undid the edit before them
// and the reply said nothing about undo. Behaviour belongs to the integration cases in
// tests/mcp-tools/gameplay/manage-ai.test.mjs, utility/behavior-tree-get-tree.test.mjs and utility/manage-audio.test.mjs,
// which assert undo.undoable on each.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DOMAINS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...parts: string[]): string =>
  readFileSync(join(DOMAINS, ...parts), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const EDITS: Record<string, readonly string[]> = {
  [join('AI', 'Blackboard', 'McpAIBlackboardKeys.cpp')]: ['Add Blackboard Key', 'Set Key Instance Synced'],
  [join('AI', 'Blackboard', 'McpAIBlackboardValues.cpp')]: ['Set Blackboard Key Default'],
  [join('AI', 'Blackboard', 'McpAIBlackboardAssignment.cpp')]: ['Assign Blackboard'],
  [join('AudioAuthoring', 'McpAudioSoundClasses.cpp')]: ['Set Sound Class Properties', 'Set Sound Class Parent'],
};

describe('blackboard and sound class edits are one undo step', () => {
  for (const [file, titles] of Object.entries(EDITS)) {
    it(`${file.split(/[\\/]/u).pop()}: ${titles.join(', ')}`, () => {
      const source = read(file);
      for (const title of titles) {
        expect(source).toMatch(new RegExp(`const FMcpAssetEdit Edit\\(TEXT\\("${title}"\\)`, 'u'));
      }
      // A Modify with no transaction open records nothing: every one belongs to an edit.
      expect(source).not.toMatch(/(Blackboard|BT|BTGraph|RootNode|SoundClass|OldParent|NewParent)->Modify\(\);/u);
    });
  }

  it('makes a key transactional, so its default is recorded when it changes', () => {
    expect(read('AI', 'Blackboard', 'McpAIBlackboardKeys.cpp')).toContain('NewObject<UBlackboardKeyType>(Blackboard, KeyClass, NAME_None, RF_Transactional)');
    expect(read('AI', 'Blackboard', 'McpAIBlackboardValues.cpp'))
      .toMatch(/Key->KeyType->SetFlags\(RF_Transactional\);\s*const FMcpAssetEdit Edit\(TEXT\("Set Blackboard Key Default"\), \{Blackboard, Key->KeyType\}/u);
  });

  it('saves a sound class\'s parents only after the step closes', () => {
    const classes = read('AudioAuthoring', 'McpAudioSoundClasses.cpp');
    const parent = classes.slice(classes.indexOf('TEXT("set_class_parent")'));
    expect(parent.indexOf('LinkSoundClass(SoundClass, ParentClass);')).toBeLessThan(parent.indexOf('SaveAudioAsset(Parent, bSave);'));
    expect(parent.indexOf('RefuseCyclicParent(')).toBeLessThan(parent.indexOf('const FMcpAssetEdit Edit('));
  });
});
