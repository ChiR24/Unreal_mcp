/// <reference types="node" />
import { describe, expect, it } from 'vitest';
import { closestMatches } from '../../src/server/gateway/gateway-guidance.js';
import {
  closestMatches as nativeClosestMatches,
  ordinalCompare
} from './plugin/gateway/native-discovery-model.js';

describe('gateway-ordinal-parity', () => {
  describe('closestMatches tie-breaking', () => {
    it('Given a forced tie when scores and prefix are equal then production returns ordinal order', () => {
      const result = closestMatches('a', ['a_', 'a.'], 2);
      expect(result).toEqual(['a.', 'a_']);
    });

    it('Given a forced tie when scores and prefix are equal then native returns ordinal order', () => {
      const result = nativeClosestMatches('a', ['a_', 'a.'], 2);
      expect(result).toEqual(['a.', 'a_']);
    });

    it('Given identical scoring candidates then production and native produce identical orderings', () => {
      const candidates = ['a_', 'a.', 'a_b', 'a.b'];
      const prodResult = closestMatches('a', candidates, 4);
      const nativeResult = nativeClosestMatches('a', candidates, 4);
      expect(prodResult).toEqual(nativeResult);
    });
  });

  describe('production-vs-native corpus probes', () => {
    const capabilityIdCorpus = [
      'asset.import', 'asset.load', 'asset.save',
      'blueprint.create', 'blueprint.modify',
      'level.load', 'level.save',
      'actor.spawn', 'actor.destroy', 'actor.transform',
    ];

    const inputSchemaPropertyCorpus = [
      'assetPath', 'packageName', 'actorClass', 'transform',
      'levelName', 'objectName', 'propertyName',
      'tagName', 'componentClass', 'widgetClass',
    ];

    it('Given capability IDs when probing then production and native agree on all orderings', () => {
      for (const target of capabilityIdCorpus) {
        const prodResult = closestMatches(target, capabilityIdCorpus, capabilityIdCorpus.length);
        const nativeResult = nativeClosestMatches(target, capabilityIdCorpus, capabilityIdCorpus.length);
        expect(prodResult).toEqual(nativeResult);
      }
    });

    it('Given input-schema property names when probing then production and native agree on all orderings', () => {
      for (const target of inputSchemaPropertyCorpus) {
        const prodResult = closestMatches(target, inputSchemaPropertyCorpus, inputSchemaPropertyCorpus.length);
        const nativeResult = nativeClosestMatches(target, inputSchemaPropertyCorpus, inputSchemaPropertyCorpus.length);
        expect(prodResult).toEqual(nativeResult);
      }
    });

    it('Given mixed corpus when target matches prefix then both return same prioritized order', () => {
      const mixed = [...capabilityIdCorpus.slice(0, 5), ...inputSchemaPropertyCorpus.slice(0, 5)];
      const target = 'asset';
      const prodResult = closestMatches(target, mixed, mixed.length);
      const nativeResult = nativeClosestMatches(target, mixed, mixed.length);
      expect(prodResult).toEqual(nativeResult);
    });
  });

  describe('ordinalCompare contract', () => {
    it('Given two ASCII strings then ordinalCompare returns -1 when a < b', () => {
      expect(ordinalCompare('a.', 'a_')).toBe(-1);
      expect(ordinalCompare('abc', 'abd')).toBe(-1);
    });

    it('Given two ASCII strings then ordinalCompare returns 1 when a > b', () => {
      expect(ordinalCompare('a_', 'a.')).toBe(1);
      expect(ordinalCompare('abd', 'abc')).toBe(1);
    });

    it('Given two identical ASCII strings then ordinalCompare returns 0', () => {
      expect(ordinalCompare('a.', 'a.')).toBe(0);
      expect(ordinalCompare('abc', 'abc')).toBe(0);
    });
  });
});
