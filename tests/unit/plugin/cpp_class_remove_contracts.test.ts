// system_control remove_cpp_class deletes a project module's class files (Domains/SystemControl/McpSystemCppClassRemove.cpp).
// It deletes source code, so the order of its checks is pinned here; the refusals are integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DIR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'SystemControl');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(DIR, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('remove_cpp_class', () => {
  const remove = code('McpSystemCppClassRemove.cpp');

  it('is routed by system_control', () => {
    const handlers = code('McpSystemHandlers.cpp');
    expect(handlers).toContain('Lower != TEXT("remove_cpp_class")');
    expect(handlers).toContain('return McpSystemControlHandlers::HandleRemoveCppClass(this, RequestId, Payload,');
  });

  // A class still placed, derived from, pointed at or named in other code would break that user on the next build.
  it('names every user and deletes nothing while there is one', () => {
    const refusal = remove.indexOf('TEXT("CLASS_IN_USE")');
    expect(refusal).toBeGreaterThan(remove.indexOf('EngineUsers(Class, Registry, Users);'));
    expect(refusal).toBeGreaterThan(remove.indexOf('SourceUsers(BaseName, Prefixed, Own, Users);'));
    expect(refusal).toBeLessThan(remove.indexOf('IFileManager::Get().Delete('));
  });

  // The registry knows which module a package imports from, not which of its types: the packages are searched.
  it('loads the packages that use the module and searches them for the class or its defaults', () => {
    expect(remove).toContain('Registry.GetReferencers(FName(*(TEXT("/Script/") + ModuleName)), Referencers);');
    expect(remove).toContain('FReferenceFinder Finder(Referenced);');
    expect(remove).toContain('Referenced.Contains(Class) || (Defaults && Referenced.Contains(Defaults))');
    // 5.8 deprecates the bool form; the two-argument call includes nested objects in every 5.x.
    expect(remove).toMatch(/ForEachObjectWithPackage\(Package, \[&\]\(UObject\* Object\) \{[\s\S]*?\}\);/u);
  });

  it('never forces a read-only file and never takes other types with a header', () => {
    expect(remove).toContain('IFileManager::Get().Delete(*File, true, false, true)');
    expect(remove.indexOf('TEXT("FILE_HOLDS_MORE")')).toBeLessThan(remove.indexOf('IFileManager::Get().Delete('));
  });
});
