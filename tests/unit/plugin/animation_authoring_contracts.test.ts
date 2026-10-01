// set_anim_graph_node_value could not find a node by the name create_node and add_state report (its object name or its
// GUID), picked the first of two Sequence Players that share a title, and wrote a setting of the node's embedded Node struct
// through the graph node, so a value was stored at the struct's offset inside the wrong object and the reply still said
// success. Wiring contracts only; behaviour needs an editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

const DOMAINS = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(DOMAINS, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const compact = (source: string): string => source.split(/\s+/u).join(' ');

const handler = (): string => code('AnimationAuthoring', 'McpAutomationBridge_AnimationAuthoringHandlersBlueprintNodeValues.cpp');
const lookup = (): string => code('Animation', 'Blueprints', 'McpAutomationBridge_AnimationGraphNodeLookup.cpp');

describe('set_anim_graph_node_value finds the node it was handed', () => {
  it('looks in the AnimGraph and every graph under it, where a state\'s Sequence Player lives', () => {
    const source = lookup();

    expect(source).toMatch(/Graphs\.Add\(AnimGraph\);\s*AnimGraph->GetAllChildrenGraphs\(Graphs\);/u);
    expect(source).toContain('for (UEdGraph* Graph : AnimGraphsOf(AnimBP))');
  });

  it('takes the object name or the GUID outright, a custom name before a shared title, and refuses a name two nodes share', () => {
    const find = sliceBetween(lookup(), 'UEdGraphNode* FindAnimGraphNode(', 'FProperty* ResolveAnimNodeProperty(');
    const flat = compact(find);

    expect(flat).toContain('const bool bIsGuid = FGuid::Parse(Requested, Guid);');
    expect(flat).toContain('if (Node->GetName().Equals(Requested, ESearchCase::IgnoreCase) || (bIsGuid && Node->NodeGuid == Guid)) { return Node; }');
    expect(flat).toContain('if (Node->NodeComment.Contains(Requested)) { ByComment.Add(Node); }');
    expect(flat).toContain('else if (Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(Requested)) { ByTitle.Add(Node); }');
    expect(flat).toContain('const TArray<UEdGraphNode*>& Candidates = ByComment.Num() > 0 ? ByComment : ByTitle; if (Candidates.Num() == 1) { return Candidates[0]; } OutAmbiguous = Candidates; return nullptr;');
  });

  it('the handler takes nodeId as nodeName, names the candidates of a shared name and lists the nodes of a miss', () => {
    const flat = compact(handler());

    expect(flat).toContain('if (NodeName.IsEmpty()) { NodeName = GetJsonStringField(Params, TEXT("nodeId"), TEXT("")); }');
    expect(flat).toContain('UEdGraphNode* FoundNode = FindAnimGraphNode(AnimBP, NodeName, Ambiguous);');
    expect(flat).toContain('Response->SetArrayField(TEXT("candidates"), Candidates);');
    expect(flat).toContain('TEXT("AMBIGUOUS_NODE")');
    expect(flat).toContain('Response->SetArrayField(TEXT("nodes"), ListAnimGraphNodes(AnimBP, 60));');
    expect(flat).toContain('TEXT("NODE_NOT_FOUND")');
    expect(flat, 'no first-match loop over the AnimGraph left behind').not.toContain('AnimGraph->Nodes');
  });
});

describe('set_anim_graph_node_value stores what it was given, where the node keeps it', () => {
  it('writes through the memory the property lives in: the embedded Node struct for its settings, not the graph node', () => {
    const resolve = sliceBetween(lookup(), 'FProperty* ResolveAnimNodeProperty(', 'bool IsAnimPlayerAssetProperty(');
    const flat = compact(handler());

    expect(compact(resolve)).toContain('OutContainer = NodeStruct->ContainerPtrToValuePtr<void>(Node); Scope = NodeStruct->Struct;');
    expect(compact(resolve)).toContain('OutContainer = Inner->ContainerPtrToValuePtr<void>(OutContainer); Scope = Inner->Struct;');
    expect(flat).toContain('ResolveAnimNodeProperty(FoundNode, PropertyName, Container, Missing);');
    expect(flat).toContain('ApplyJsonValueToProperty(Container, Property, ValueField, SetError)');
    expect(flat, 'the container is not the graph node').not.toContain('ApplyJsonValueToProperty(FoundNode');
  });

  it('reads the stored value back, and the reply names the node it found', () => {
    const flat = compact(handler());

    expect(flat).toContain('Property->ExportText_Direct(Stored, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);');
    expect(flat).toContain('Response->SetStringField(TEXT("value"), Stored);');
    expect(flat).toContain('Response->SetStringField(TEXT("nodeName"), FoundNode->GetName());');
    expect(flat).toContain('Response->SetStringField(TEXT("nodeId"), FoundNode->NodeGuid.ToString());');
  });

  it('an asset player\'s asset goes through the node\'s own setter, only for a class it plays, and fails when it did not take', () => {
    const flat = compact(handler());
    const setter = compact(sliceBetween(lookup(), 'bool SetAnimPlayerAsset(', 'zzz-end'));
    const supports = setter.indexOf('Player->SupportsAssetClass(Asset->GetClass()) == EAnimAssetHandlerType::NotSupported');
    const set = setter.indexOf('Player->SetAnimationAsset(Asset);');

    expect(flat).toContain('if (IsAnimPlayerAssetProperty(FoundNode, Property)) {');
    expect(flat).toContain('SetAnimPlayerAsset(FoundNode, ValueField, Stored, SetError, SetErrorCode)');
    expect(compact(lookup())).toContain('Cast<UAnimGraphNode_AssetPlayerBase>(Node) && ObjectProperty && ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(UAnimationAsset::StaticClass())');
    expect(supports, 'the class check').toBeGreaterThan(-1);
    expect(set, 'the setter runs after it').toBeGreaterThan(supports);
    expect(setter).toContain('Player->Modify(); Player->SetAnimationAsset(Asset); if (Player->GetAnimationAsset() != Asset) {');
    expect(setter).toContain('OutErrorCode = TEXT("PROPERTY_SET_FAILED");');
    expect(setter).toContain('OutErrorCode = TEXT("ASSET_TYPE_MISMATCH");');
    expect(setter).toContain('OutErrorCode = TEXT("ASSET_NOT_FOUND");');
  });
});

describe('the set_anim_graph_node_value record says how to name a node', () => {
  const record = () => capabilityIndex().byId.get('animation_physics.configure_anim_graph_node');
  const description = (name: string): string => {
    const properties = record()?.schemas.input.properties;
    const entry = isRecord(properties) ? properties[name] : undefined;
    return isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';
  };

  it('nodeName takes the object name or the GUID, and nodeId is the GUID alone, read by this variant only', () => {
    expect(description('nodeName')).toMatch(/object name \(AnimGraphNode_SequencePlayer_0\), its GUID/u);
    expect(description('nodeName')).toMatch(/NODE_NOT_FOUND listing every node/u);
    expect(description('nodeId')).toMatch(/GUID/u);
    expect(record()?.routing.dispatchBy?.declaredBy?.nodeId).toEqual(['set_value']);
    expect(record()?.routing.dispatchBy?.declaredBy?.nodeName).toEqual(['set_value']);
  });

  it('propertyName says an asset property is set through the node and read back', () => {
    expect(description('propertyName')).toMatch(/asset property \(Sequence, BlendSpace\) is written through the node's own setter and read back/u);
  });
});
