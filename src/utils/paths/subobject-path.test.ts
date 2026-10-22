import { describe, expect, it } from 'vitest';
import { normalizeSubobjectPath } from './subobject-path.js';
import { validateArgsSecurity } from '../../tools/handlers/foundation/arguments/handler-argument-validation.js';

const component = '/Game/TeamA/Maps/Test.Test:PersistentLevel.Actor_0.PCGComponent_0';

describe('component object-path syntax at the bridge boundary', () => {
  it('keeps full canonical locators rather than dropping subobject identity', () => {
    expect(normalizeSubobjectPath(component)).toBe(component);
    expect(() => validateArgsSecurity({ componentPath: component })).not.toThrow();
  });

  it('normalizes supported aliases in the same order as the native helper', () => {
    expect(normalizeSubobjectPath('/Content/TeamA/Maps/Test.Test:PersistentLevel.Actor_0.PCGComponent_0')).toBe(component);
    expect(normalizeSubobjectPath('\\Content\\TeamA\\Maps\\Test.Test:PersistentLevel.Actor_0.PCGComponent_0')).toBe(component);
  });

  it.each([
    '/Game/TeamA/../TeamB/Test.Test:PersistentLevel.Actor.Component',
    '/Game/TeamA/Test.Test:PersistentLevel.Actor..Component',
    '/Game/TeamA/Test.Test:PersistentLevel.Actor:Component',
    '/Game/TeamA/Test.Test:', '/Game/TeamA/Test.:Actor.Component',
    '/Game/TeamA/Test:PersistentLevel.Actor.Component',
    '/Game/TeamA/Test.Test:PersistentLevel.Actor.Component/Other',
    '/Game/TeamA/Test.Test:PersistentLevel.Actor.Component?Other',
    '/Game/TeamA/Test.Test:PersistentLevel.Actor.Component\n',
    '//Game/TeamA/Test.Test:PersistentLevel.Actor.Component',
    '/Game//TeamA/Test.Test:PersistentLevel.Actor.Component',
    'C:/TeamA/Test.Test:PersistentLevel.Actor.Component',
    '\\\\host\\share\\Test.Test:PersistentLevel.Actor.Component',
    '/home/Test.Test:PersistentLevel.Actor.Component',
    `${component}\0Hidden`,
  ])('refuses the full malformed address %j', value => {
    expect(() => normalizeSubobjectPath(value)).toThrow();
    expect(() => validateArgsSecurity({ componentPath: value })).toThrow();
  });

  it('applies the same syntax validation to nested unusual-key values', () => {
    expect(() => validateArgsSecurity({ settings: { unusualKey: `${component}:Other` } })).toThrow();
    expect(() => validateArgsSecurity({ settings: { unusualKey: [component] } })).not.toThrow();
  });

  it('preserves legacy component names and reflection-member references', () => {
    expect(() => validateArgsSecurity({ action: 'execute_pcg_graph', componentPath: 'Owner.PCGComponent' })).not.toThrow();
    expect(() => validateArgsSecurity({ objectPath: '/Script/Engine.Actor::StaticClass' })).not.toThrow();
  });

  it('does not treat ordinary colon-bearing text as an object path', () => {
    expect(() => validateArgsSecurity({ description: 'Note: this is not a path' })).not.toThrow();
  });
});
