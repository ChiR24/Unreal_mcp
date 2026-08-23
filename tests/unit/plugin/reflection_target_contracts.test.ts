import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Enforcement-wiring contracts for the generic property-reflection surface.
//
// `get_object_property` / `set_object_property` and the array/map/set element
// handlers take a caller-supplied `objectPath` and then read or write ANY
// reflected property on whatever it resolves to. Object resolution accepts
// `/Script/` paths, and a `/Script/` path names a native class object or its CDO
// — including `/Script/McpAutomationBridge.Default__McpAutomationBridgeSettings`,
// which carries `CapabilityToken`, `ScopedCapabilityTokens`,
// `bRequireCapabilityToken` and `bAllowNonLoopback` as reflected UPROPERTYs and is
// re-read by the authorization path on every connection.
//
// Left open, `inspect.set_property` (write / consent none) could turn the
// plugin's own token requirement off and persist it through PostEditChange(), and
// `inspect.get_property` (read / consent none) could export the Admin token. The
// predicate lives in Safety/McpSafeReflectionTarget.h; these assertions prove
// every entry point CALLS it, because a passing predicate nobody invokes is not a
// control.

const PRIVATE_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);

const privateSource = (...parts: string[]): string =>
  readFileSync(resolve(PRIVATE_ROOT, ...parts), 'utf8');

const guard = (): string => privateSource('Safety', 'McpSafeReflectionTarget.h');
const objectGet = (): string =>
  privateSource('Domains', 'Property', 'McpAutomationBridge_PropertyHandlersObjectGet.cpp');
const objectSet = (): string =>
  privateSource('Domains', 'Property', 'McpAutomationBridge_PropertyHandlersObjectSet.cpp');

const propertyDomainFiles = (): string[] =>
  readdirSync(resolve(PRIVATE_ROOT, 'Domains/Property'), { encoding: 'utf8' }).filter((entry) =>
    entry.endsWith('.cpp')
  );

describe('the reflection surface refuses /Script targets', () => {
  it('the guard rejects script packages by OUTERMOST package, not by class or CDO flag', () => {
    const source = guard();
    // The rule must be the package, so a Blueprint CDO — a legitimate authoring
    // target living in a content package — keeps working while every native CDO
    // is refused. Keying on RF_ClassDefaultObject would break the first and keying
    // on a class name would miss every other /Script object.
    expect(source).toMatch(/StartsWith\(\s*TEXT\("\/Script\/"\)/u);
    expect(source).toMatch(/Equals\(\s*TEXT\("\/Script"\)/u);
    expect(source).toContain('GetOutermost()');
  });

  it('the guard fails closed on a null object and a missing package', () => {
    const source = guard();
    expect(source).toMatch(/Object\s*==\s*nullptr\s*\)\s*\{\s*return false;/u);
    expect(source).toMatch(/Outermost\s*==\s*nullptr\s*\)\s*\{\s*return false;/u);
  });

  it('get_object_property checks the target BEFORE exporting any property', () => {
    const source = objectGet();
    const guardAt = source.indexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    const exportAt = source.indexOf('ExportPropertyToJsonValue(');
    expect(guardAt, 'the read path must call the guard').toBeGreaterThan(-1);
    expect(exportAt).toBeGreaterThan(-1);
    expect(guardAt, 'a refusal after the export would already have read the secret').toBeLessThan(
      exportAt
    );
  });

  it('get_object_property re-asserts the guard after the component-template reassignment', () => {
    const source = objectGet();
    const firstGuardAt = source.indexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    // The Blueprint component-template branch re-points RootObject, so a second
    // guard must sit AFTER the reassignment and before ResolveProperty.
    const secondGuardAt = source.lastIndexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    const reassignAt = source.indexOf('RootObject = CompTemplate;');
    const resolveAt = source.indexOf('McpHandlerUtils::ResolveProperty(');
    expect(firstGuardAt).toBeGreaterThan(-1);
    expect(reassignAt, 'the component-template branch must still exist').toBeGreaterThan(-1);
    expect(secondGuardAt, 'the boundary must be re-asserted on the re-pointed target').toBeGreaterThan(
      firstGuardAt
    );
    expect(secondGuardAt).toBeGreaterThan(reassignAt);
    expect(secondGuardAt, 'the re-assertion must precede property resolution').toBeLessThan(
      resolveAt
    );
  });

  it('get_object_property echoes the RESOLVED canonical property name, not the caller string', () => {
    const source = objectGet();
    expect(
      source,
      'the sibling redactor judges the echoed name, so the raw caller spelling must not be echoed'
    ).toContain('PropResult.Property->GetName()');
    // The GENERAL reflection path (after ResolveProperty) must not echo the
    // untrusted PropertyName. The earlier Actor-transform branches are safe: they
    // only run when the caller named exactly ActorLocation/Rotation/Scale, which
    // cannot be a credential.
    const resolveAt = source.indexOf('McpHandlerUtils::ResolveProperty(');
    const generalEcho = source.indexOf('SetStringField(TEXT("propertyName"), PropertyName)', resolveAt);
    expect(resolveAt).toBeGreaterThan(-1);
    expect(generalEcho, 'the caller-supplied spelling must never reach the general response').toBe(-1);
  });

  it('set_object_property checks the target BEFORE applying any value', () => {
    const source = objectSet();
    const guardAt = source.indexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    const applyAt = source.indexOf('ApplyJsonValueToProperty(');
    const persistAt = source.indexOf('RootObject->PostEditChange()');
    expect(guardAt, 'the write path must call the guard').toBeGreaterThan(-1);
    expect(applyAt).toBeGreaterThan(-1);
    expect(guardAt).toBeLessThan(applyAt);
    // PostEditChange() is what writes DefaultGame.ini, so a refusal must land
    // before it or the hostile setting survives an editor restart.
    expect(persistAt).toBeGreaterThan(-1);
    expect(guardAt).toBeLessThan(persistAt);
  });

  it('set_object_property re-asserts the guard after the component-template reassignment', () => {
    const source = objectSet();
    const firstGuardAt = source.indexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    const secondGuardAt = source.lastIndexOf('McpSafeReflectionTarget::IsAddressable(RootObject)');
    const reassignAt = source.indexOf('RootObject = CompTemplate;');
    const modifyAt = source.indexOf('RootObject->Modify()');
    expect(firstGuardAt).toBeGreaterThan(-1);
    expect(reassignAt, 'the component-template branch must still exist').toBeGreaterThan(-1);
    expect(secondGuardAt, 'the boundary must be re-asserted on the re-pointed target').toBeGreaterThan(
      firstGuardAt
    );
    expect(secondGuardAt).toBeGreaterThan(reassignAt);
    expect(secondGuardAt, 'the re-assertion must precede Modify/Apply/PostEditChange').toBeLessThan(
      modifyAt
    );
  });

  it('both handlers report the refusal as a distinct code, not "not found"', () => {
    for (const source of [objectGet(), objectSet()]) {
      expect(source).toContain('McpSafeReflectionTarget::DenyCode()');
    }
    expect(guard()).toContain('OBJECT_NOT_ADDRESSABLE');
  });

  // The container handlers resolve with a bare FindObject rather than
  // ResolveObjectFromPath, so they were a second, independent route to the same
  // CDOs. Every one of them must now go through the narrowed resolver.
  it('no Property-domain handler resolves an objectPath with an unguarded FindObject', () => {
    const offenders = propertyDomainFiles().filter((file) =>
      /FindObject<UObject>\(\s*nullptr\s*,\s*\*ObjectPath\s*\)/u.test(
        privateSource('Domains', 'Property', file)
      )
    );
    expect(
      offenders,
      'use McpSafeReflectionTarget::FindAddressableObject so /Script targets cannot be addressed'
    ).toEqual([]);
  });

  it('every Property-domain file that resolves an objectPath includes the guard', () => {
    for (const file of propertyDomainFiles()) {
      const source = privateSource('Domains', 'Property', file);
      if (!source.includes('McpSafeReflectionTarget::')) continue;
      expect(source, `${file} uses the guard without including it`).toContain(
        '#include "Safety/McpSafeReflectionTarget.h"'
      );
    }
  });

  // The header exposes OutDenied precisely so a caller can tell "you may not
  // address that" from "there is nothing there". The get/set handlers report the
  // distinct OBJECT_NOT_ADDRESSABLE code; the container handlers must thread the
  // flag too, or a denied /Script target reads as a plain "Object not found".
  it('the container handlers thread OutDenied and report OBJECT_NOT_ADDRESSABLE for a denied target', () => {
    const containerFiles = propertyDomainFiles().filter(
      (file) =>
        file !== 'McpAutomationBridge_PropertyHandlersObjectGet.cpp' &&
        file !== 'McpAutomationBridge_PropertyHandlersObjectSet.cpp'
    );
    expect(containerFiles.length).toBeGreaterThan(0);
    for (const file of containerFiles) {
      const source = privateSource('Domains', 'Property', file);
      if (!source.includes('McpSafeReflectionTarget::FindAddressableObject(')) continue;
      expect(source, `${file} must pass OutDenied to the narrowed resolver`).toMatch(
        /FindAddressableObject\(\s*ObjectPath\s*,\s*&bObjectDenied\s*\)/u
      );
      expect(source, `${file} must surface the distinct denial code`).toContain(
        'McpSafeReflectionTarget::DenyCode()'
      );
      expect(source, `${file} must keep OBJECT_NOT_FOUND for a genuinely missing object`).toContain(
        'TEXT("OBJECT_NOT_FOUND")'
      );
    }
  });
});
