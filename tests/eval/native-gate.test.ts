// tests/eval/native-gate.test.ts
// The retrieval gate for the native gateway door (the plugin's HTTP /mcp search),
// measured through its TypeScript copy in native-ranker.ts. The copy reads its word
// lists from the C++ source and is pinned to its weights first, so a weight changed
// on one side fails here.
// ponytail: only constants are pinned; a change to the C++ scoring LOGIC must be
// copied into native-ranker.ts by hand.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { measureRetrieval } from './measure-retrieval.js';
import {
  NATIVE_ACTION_COVERED_BONUS, NATIVE_DELETE_INTENT_BONUS, NATIVE_FULL_COVERAGE_BONUS, NATIVE_GENERIC_WORD_TOOLS, NATIVE_PHRASE_TOPIC_BONUS, NATIVE_READ_INTENT_BONUS,
  NATIVE_RULE_WEIGHTS, NATIVE_TYPO_LONG_LENGTH, NATIVE_TYPO_MIN_LENGTH, NATIVE_TYPO_SWAP_LENGTH, NATIVE_WORD_COVERAGE_BONUS,
  NATIVE_JOINED_SPAN_WORDS, nativeCorrectQuery, nativeEditDistance, nativeGenericWords, nativeScored, nativeSearchRanker, nativeSplitCamelCase,
  nativeWholeActions,
} from './native-ranker.js';

const GATEWAY = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Gateway');
const nativeMatch = readFileSync(join(GATEWAY, 'McpNativeGatewaySearchMatch.cpp'), 'utf8');
const nativeHeader = readFileSync(join(GATEWAY, 'McpNativeGatewaySearch.h'), 'utf8');
const nativeTypos = readFileSync(join(GATEWAY, 'Search', 'McpSearchTypos.cpp'), 'utf8');
const nativeIntent = readFileSync(join(GATEWAY, 'Search', 'McpSearchIntent.cpp'), 'utf8');

const headerConstant = (name: string): number => Number(nativeHeader.match(new RegExp(`constexpr int32 ${name} = (\\d+);`, 'u'))?.[1]);

describe('the TypeScript copy matches the native source', () => {
  it('has the same rule weights', () => {
    const start = nativeMatch.indexOf('MatchRules[RuleCount]');
    const block = nativeMatch.slice(start, nativeMatch.indexOf('};', start));
    const weights = Object.fromEntries([...block.matchAll(/TEXT\("([a-z-]+)"\), (\d+)/gu)].map((match) => [match[1], Number(match[2])]));
    expect(weights).toEqual(NATIVE_RULE_WEIGHTS);
  });

  it('has the same bonuses', () => {
    expect(headerConstant('McpSearchWordCoverageBonus')).toBe(NATIVE_WORD_COVERAGE_BONUS);
    expect(headerConstant('McpSearchActionCoveredBonus')).toBe(NATIVE_ACTION_COVERED_BONUS);
    expect(headerConstant('McpSearchFullCoverageBonus')).toBe(NATIVE_FULL_COVERAGE_BONUS);
    expect(headerConstant('McpSearchPhraseTopicBonus')).toBe(NATIVE_PHRASE_TOPIC_BONUS);
    expect(nativeMatch).toContain('Score += MatchRules[RuleTopic].Weight + McpSearchPhraseTopicBonus;');
    expect(headerConstant('McpSearchReadIntentBonus')).toBe(NATIVE_READ_INTENT_BONUS);
    expect(headerConstant('McpSearchDeleteIntentBonus')).toBe(NATIVE_DELETE_INTENT_BONUS);
  });

  it('corrects typos within the same limits', () => {
    const typoConstant = (name: string): number => Number(nativeTypos.match(new RegExp(`constexpr int32 ${name} = (\\d+);`, 'u'))?.[1]);
    expect(typoConstant('McpTypoSwapLength')).toBe(NATIVE_TYPO_SWAP_LENGTH);
    expect(typoConstant('McpTypoMinLength')).toBe(NATIVE_TYPO_MIN_LENGTH);
    expect(typoConstant('McpTypoLongLength')).toBe(NATIVE_TYPO_LONG_LENGTH);
    expect(nativeTypos).toContain('AreNeighbourKeys(A[I - 1], B[J - 1]) ? 1 : 2');
  });

  it('ties a lead verb to its object under the same conditions', () => {
    expect(nativeMatch).toContain('(!bTieLead || McpSearchLeadTied(Record, ContentWords))');
    expect(nativeMatch).toContain('const bool bTieLead = (bLeadIsAction && !bLeadNamed) ||');
    expect(nativeIntent).toContain('if (Words.Num() == 1)');
  });

  it('scores one alias at a time under the same rule', () => {
    expect(nativeMatch).toContain('Hits[RuleId] = ScoredWords.Contains(Word);');
    expect(nativeMatch).toContain('if (bAny || bActionWord) ++Matched;');
    expect(nativeIntent).toContain('if (bOwn || (bAlias && Subject.Contains(Word))) OutScored.Add(Word);');
    expect(nativeIntent).toContain('if (Added.Num() > Best.Num()) Best = MoveTemp(Added);');
  });

  it('reads a phrase on word boundaries under the same rule', () => {
    expect(nativeMatch).toContain('if ((Hit == Start || !IsAsciiAlnum(Hit[-1])) && !IsAsciiAlnum(Hit[Phrase.Len()])) return true;');
    expect(nativeMatch).toContain('if (ContainsPhrase(Topic.ToLower(), Query)) return true;');
    expect(nativeMatch).toContain('if (ContainsPhrase(Record.Summary.ToLower(), Query))');
  });

  it('reads CamelCase under the same rules', () => {
    const nativeSearch = readFileSync(join(GATEWAY, 'McpNativeGatewaySearch.cpp'), 'utf8');
    const camelCase = readFileSync(join(GATEWAY, 'Search', 'McpSearchCamelCase.cpp'), 'utf8');
    const camelHeader = readFileSync(join(GATEWAY, 'Search', 'McpSearchCamelCase.h'), 'utf8');
    expect(nativeSearch).toContain('McpSplitCamelCase(Input.Query.Left(McpSearchMaxQueryChars).TrimStartAndEnd()).ToLower()');
    expect(camelCase).toContain('? !(LowerIn(Run, 0, At, true) && !LowerIn(Run, At, Run.Len(), false))');
    expect(camelCase).toContain(": IsUpper(Prev) && IsLower(Next) && At - Start > 1 && !(Next == TEXT('s') && At + 2 == Run.Len());");
    expect(Number(camelHeader.match(/constexpr int32 McpSearchJoinedSpanWords = (\d+);/u)?.[1])).toBe(NATIVE_JOINED_SPAN_WORDS);
    expect(nativeMatch).toContain('Hits[RuleTopic] = AnyTopicHolds(Record.Topics, Word, Spans);');
    expect(nativeMatch).toContain('Hits[RuleSummary] = McpSearchFieldHolds(Record.Summary, Word, Spans, true);');
    expect(nativeMatch).toContain('if (McpSearchFieldHolds(Topic, Word, Spans, false)) return true;');
    expect(nativeMatch).toContain('McpSearchJoinedSpans(Query, ContentWords), Out);');
    expect(camelCase).toContain('if (End > Index && IsUpper(Text[Index]))');
    expect(nativeMatch).toContain('Hits[RuleFamily] = ContainsWord(Record.Family, Word);');
    const split = (text: string): string => nativeSplitCamelCase(text);
    expect(split('create GameplayEffect')).toBe('create Gameplay Effect');
    expect(split('DSLParser PCGGraph')).toBe('DSL Parser PCG Graph');
    expect(split('Texture2D K2Node UE5')).toBe('Texture2D K2Node UE5');
    expect(split('LODs NPCs UVs')).toBe('LODs NPCs UVs');
    expect(split('sRGB iOS macOS')).toBe('sRGB iOS macOS');
    expect(split('UObject AActor FVector')).toBe('UObject AActor FVector');
    expect(split('manage_gas.create_gas_asset')).toBe('manage_gas.create_gas_asset');
  });

  it('ranks an engine name typed whole as it ranks the name spaced', () => {
    for (const [whole, spaced] of [['create PlayerController', 'create player controller'], ['create SoundCue', 'create sound cue'],
      ['spawn PostProcessVolume', 'spawn post process volume'], ['add StaticMeshComponent', 'add static mesh component']]) {
      expect(nativeSearchRanker(String(whole), 1)).toEqual(nativeSearchRanker(String(spaced), 1));
    }
  });

  it('asks an alias for a word that sets it apart under the same rule', () => {
    expect(headerConstant('McpSearchGenericWordTools')).toBe(NATIVE_GENERIC_WORD_TOOLS);
    expect(nativeMatch).toContain('McpSearchActionWords(Record, ContentWords, bLeadIsAction, GenericWords, HeldWords, ScoredWords);');
    expect(nativeIntent).toContain('const bool bAsked = !bLeadIsAction || AliasAsked(Words, Own, ContentWords, GenericWords);');
    expect(nativeIntent).toContain('if (Words.Contains(Word) && !OutScored.Contains(Word) && (bAsked || Word == Words[0])) Added.Add(Word);');
    expect(nativeIntent).toContain('if (GenericWords.Contains(Word) || (Own.Num() > 0 && Word == Own[0]))');
    expect(nativeIntent).toContain('if (Entry.Value.Num() >= McpSearchGenericWordTools) Out.Add(Entry.Key);');
  });
});

describe('CamelCase query normalization', () => {
  it.each([
    ['[GameplayEffect]/K2Node', '[Gameplay Effect]/K2Node'],
    ['PCGGraphNode, Texture2DArray', 'PCG Graph Node, Texture2D Array'],
    ['sRGB (UObjectFactory)', 'sRGB (UObject Factory)'],
    ['MakeUObject/MakeLODs/NPCSpawner', 'Make UObject/Make LODs/NPC Spawner'],
    ['xInput—GameplayEffect\tPCGGraph', 'x Input—Gameplay Effect\tPCG Graph'],
  ])('splits %s without changing punctuation or protected boundaries', (input, expected) => {
    expect(nativeSplitCamelCase(input)).toBe(expected);
  });

  it('preserves the ranking and scores when a CamelCase name follows a typo', () => {
    const spaced = nativeScored('create gameplay effect');
    expect(spaced[0]?.id).toBe('manage_gas.create_gas_asset');
    expect(nativeScored('craete GameplayEffect')).toEqual(spaced);
  });

  it('scores SoundCue in a summary only for adjacent words in the right order', () => {
    // The editor preview summary includes SoundCue and the standalone word sound.
    // Both queries match sound; only the first also earns summary and coverage points for cue.
    const previewScore = (query: string) => nativeScored(query).find(({ id }) => id === 'system_control.play_sound')?.score;
    const forward = previewScore('sound cue');
    const reversed = previewScore('cue sound');
    expect(reversed).toBeDefined();
    expect(forward).toBe((reversed ?? 0) + NATIVE_RULE_WEIGHTS.summary + NATIVE_WORD_COVERAGE_BONUS);
  });

  it('does not score a lower camel case field in summary prose as a joined name', () => {
    // The same preview summary has soundPath, but the field should add no points for path.
    const previewScore = (query: string) => nativeScored(query).find(({ id }) => id === 'system_control.play_sound')?.score;
    const reversed = previewScore('path sound');
    expect(reversed).toBeDefined();
    expect(previewScore('sound path')).toBe(reversed);
  });
});

// A verb that is some record's whole action leads the request; a record that names it only in a topic, or does not
// delete and is asked to, covers the request only where one of its names or topics holds the verb and the object
// (EngineWire.Gateway.SearchIntent holds the C++ to the same).
describe('lead verbs', () => {
  it('reads these words as whole actions: a single-word alias joins the list and moves rankings', () => {
    expect([...nativeWholeActions()].sort()).toEqual([
      'activate', 'attach', 'bend', 'bevel', 'blur', 'bridge', 'chamfer', 'cleanup', 'compile', 'create', 'cylindrify',
      'deactivate', 'delete', 'desaturate', 'detach', 'difference', 'duplicate', 'eject', 'exist', 'explain', 'export',
      'extrude', 'find', 'get', 'import', 'inset', 'invert', 'list', 'load', 'loft', 'mirror', 'morphology', 'move',
      'niagara', 'open', 'outset', 'particle', 'pause', 'play', 'playtest', 'poke', 'possess', 'profile', 'quadrangulate',
      'redo', 'reimport', 'relax', 'rename', 'reset', 'resume', 'revolve', 'save', 'screenshot', 'sculpt', 'set', 'sharpen',
      'shell', 'smooth', 'spawn', 'spherify', 'stop', 'stream', 'stretch', 'subdivide', 'subscribe', 'sweep', 'taper',
      'twist', 'undo', 'unload', 'unsubscribe', 'validate',
    ]);
  });

  it('keeps a verb named in passing from covering another object', () => {
    expect(nativeSearchRanker('move a vertex', 1)).toEqual(['manage_geometry.edit_dynamic_mesh']);
    expect(nativeSearchRanker('delete triangles', 1)).toEqual(['manage_geometry.edit_dynamic_mesh']);
    expect(nativeSearchRanker('delete the crate', 3)).not.toContain('manage_geometry.edit_dynamic_mesh');
  });
});

// Words spread over several aliases name no single action: three members of a folded family (create_procedural_mesh,
// set_material_id, set_vertex_color) used to add up to a material request. A word the record's domain names too keeps
// its score beside another alias: find_actors_by_class and find_by_material still make "actors" and "material" a find
// (EngineWire.Gateway.SearchIntent holds the C++ to the same).
describe('one alias at a time', () => {
  it('keeps a folded family from collecting a request word by word', () => {
    expect(nativeSearchRanker('create a material with a red color', 1)).toEqual(['material.create_material']);
    expect(nativeSearchRanker('which actors use this material', 1)).toEqual(['control_actor.find']);
  });
});

// A request found inside a longer word is not the request: 'unlink pins' held "link pins" and 'delete c++ source files'
// held "c++ source file", so a delete answered a request to connect and one to create.
describe('phrases on word boundaries', () => {
  it('gives the phrase bonus only to a topic that holds the request as whole words', () => {
    expect(nativeSearchRanker('link pins', 1)).toEqual(['blueprint.edit_graph']);
    expect(nativeSearchRanker('c++ source file', 1)).toEqual(['system_control.create_cpp_class']);
  });
});

// A word the actions of many tools hold names no action by itself. In a request that opens with an action verb, an
// alias scores such words only when the request holds a word that sets the alias apart: create_spline_mesh_component
// held three of the four words of "create actor with mesh component" and none that says spline.
describe('generic action words', () => {
  it('reads these words as too common to set an action apart: a word joins when more tools name actions with it', () => {
    expect([...nativeGenericWords()].sort()).toEqual([
      'actor', 'add', 'asset', 'component', 'configure', 'create', 'delete', 'edit', 'get', 'info', 'list', 'mesh', 'property',
      'set', 'setting', 'to',
    ]);
  });

  it('keeps a member name from answering a request that holds only its generic words', () => {
    expect(nativeSearchRanker('create actor with mesh component', 1)).toEqual(['control_actor.edit_component']);
    expect(nativeSearchRanker('create mesh actor', 1)).toEqual(['control_actor.spawn']);
    expect(nativeSearchRanker('create spline mesh component', 1)).toEqual(['build_environment.create_spline']);
  });
});

// The known answers EngineWire.Gateway.SearchTypos holds the C++ to, on the copy.
describe('typo correction', () => {
  it('measures edits as the native door does', () => {
    expect(nativeEditDistance('spwan', 'spawn', 1)).toBe(1);
    expect(nativeEditDistance('delte', 'delete', 1)).toBe(1);
    expect(nativeEditDistance('lighr', 'light', 1)).toBe(1);
    expect(nativeEditDistance('carve', 'curve', 2)).toBe(2);
    expect(nativeEditDistance('abcdef', 'uvwxyz', 1)).toBe(2);
  });

  it('corrects a typo, a four-letter swap, and leaves correct and real words alone', () => {
    expect(nativeCorrectQuery('spwan actor')).toEqual({ query: 'spawn actor', corrected: [{ from: 'spwan', to: 'spawn' }] });
    expect(nativeCorrectQuery('lsit actors').query).toBe('list actors');
    expect(nativeCorrectQuery('spawn actor').corrected).toEqual([]);
    expect(nativeCorrectQuery('carve valleys').corrected).toEqual([]);
  });
});

describe('native retrieval', () => {
  const measured = measureRetrieval(nativeSearchRanker);

  it('top-K recall is at least 98%', () => {
    expect(measured.topKRecall, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.98);
  });

  it('top-1 accuracy is at least 90%', () => {
    expect(measured.top1Accuracy, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.9);
  });
});
