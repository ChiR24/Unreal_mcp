import fs from 'node:fs';
import path from 'node:path';
import { maskCppLiteralsAndComments } from '../native-mcp-source-parser.mjs';

function recursiveSourceFiles(root) {
  return fs.readdirSync(root, { withFileTypes: true })
    .flatMap((entry) => {
      const entryPath = path.join(root, entry.name);
      return entry.isDirectory() ? recursiveSourceFiles(entryPath) : [entryPath];
    })
    .filter((filePath) => filePath.endsWith('.cpp') || filePath.endsWith('.h'))
    .sort();
}

function includedPaths(filePath, source, nativeMcpRoot, nativeToolsRoot) {
  const paths = [];
  for (const match of source.matchAll(/^\s*#include\s+"([^"]+)"/gm)) {
    const include = match[1];
    const candidates = [
      path.resolve(path.dirname(filePath), include),
      path.resolve(path.dirname(nativeMcpRoot), include)
    ];
    const resolved = candidates.find((candidate) =>
      candidate.startsWith(nativeToolsRoot)
      && fs.existsSync(candidate)
      && fs.statSync(candidate).isFile()
    );
    if (resolved) paths.push(resolved);
  }
  return paths;
}

function qualifiedCalls(source) {
  const maskedSource = maskCppLiteralsAndComments(source);
  return [...new Set(
    [...maskedSource.matchAll(/\b([A-Za-z_]\w*(?:::\w+)+)\s*\(/g)]
      .map((match) => match[1])
  )];
}

function containsFunctionInNamespace(source, qualifiedCall) {
  const parts = qualifiedCall.split('::');
  const functionName = parts.pop();
  const namespaceName = parts.join('::');
  if (!functionName || !namespaceName) return false;
  const maskedSource = maskCppLiteralsAndComments(source);
  const namespacePattern = new RegExp(
    `\\bnamespace\\s+${namespaceName.replaceAll('::', '\\s*::\\s*')}\\s*\\{`
  );
  const namespaceMatch = namespacePattern.exec(maskedSource);
  if (!namespaceMatch) return false;
  const remaining = maskedSource.slice(namespaceMatch.index + namespaceMatch[0].length);
  return new RegExp(`\\b${functionName}\\s*\\(`).test(remaining);
}

export function reachableNativeSchemaSources({
  filePath,
  toolSource,
  nativeMcpRoot,
  nativeToolsRoot
}) {
  const allSourcePaths = recursiveSourceFiles(nativeToolsRoot);
  const sourceCache = new Map();
  const readSource = (sourcePath) => {
    if (!sourceCache.has(sourcePath)) {
      sourceCache.set(sourcePath, fs.readFileSync(sourcePath, 'utf8'));
    }
    return sourceCache.get(sourcePath);
  };
  const selectedSources = [toolSource];
  const selectedPaths = new Set();
  const pendingPaths = [filePath];
  const pendingCalls = qualifiedCalls(toolSource);
  const resolvedCalls = new Set();

  while (pendingPaths.length > 0 || pendingCalls.length > 0) {
    const currentPath = pendingPaths.shift();
    if (currentPath && !selectedPaths.has(currentPath)) {
      selectedPaths.add(currentPath);
      const source = readSource(currentPath);
      if (currentPath !== filePath) {
        selectedSources.push(source);
        pendingCalls.push(...qualifiedCalls(source));
      }
      pendingPaths.push(
        ...includedPaths(currentPath, source, nativeMcpRoot, nativeToolsRoot)
          .filter((includePath) => !selectedPaths.has(includePath))
      );
    }

    const call = pendingCalls.shift();
    if (!call || resolvedCalls.has(call)) continue;
    resolvedCalls.add(call);
    for (const candidatePath of allSourcePaths) {
      if (selectedPaths.has(candidatePath)) continue;
      const candidateSource = readSource(candidatePath);
      if (containsFunctionInNamespace(candidateSource, call)) {
        pendingPaths.push(candidatePath);
      }
    }
  }

  return selectedSources;
}
