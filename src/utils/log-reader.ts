import { promises as fs } from 'node:fs';
import path from 'node:path';
import { loadEnv } from '../types/env.js';
import { Logger } from './logger.js';

type ReadLogParams = {
  filterCategory?: string[];
  filterLevel?: 'Error' | 'Warning' | 'Log' | 'Verbose' | 'VeryVerbose' | 'All';
  lines?: number;
  logPath?: string;
  includePrefixes?: string[];
  excludeCategories?: string[];
};

type LogEntry = {
  timestamp?: string;
  category?: string;
  level?: string;
  message: string;
};

const log = new Logger('LogReader');
let cachedLogPath: string | undefined;

function getProjectRoot(projectPath: string | undefined): string | undefined {
  const trimmed = typeof projectPath === 'string' ? projectPath.trim() : '';
  if (trimmed.length === 0) return undefined;
  return trimmed.toLowerCase().endsWith('.uproject') ? path.dirname(trimmed) : trimmed;
}

async function getRealAllowedLogDirs(): Promise<string[]> {
  const allowedDirs = [path.resolve(path.join(process.cwd(), 'Saved', 'Logs'))];
  const projectRoot = getProjectRoot(loadEnv().UE_PROJECT_PATH);
  if (projectRoot) {
    allowedDirs.push(path.resolve(path.join(projectRoot, 'Saved', 'Logs')));
  }

  return (await Promise.all(allowedDirs.map(dir => realpathIfReadable(dir))))
    .filter((dir): dir is string => dir !== undefined);
}

async function resolveAllowedLogFile(candidatePath: string): Promise<string | undefined> {
  const realCandidate = await realpathIfReadable(path.resolve(candidatePath));
  if (!realCandidate || !realCandidate.toLowerCase().endsWith('.log')) return undefined;

  const realAllowedDirs = await getRealAllowedLogDirs();
  if (!realAllowedDirs.some(dir => isPathWithin(realCandidate, dir))) return undefined;

  const st = await fs.stat(realCandidate);
  return st.isFile() ? realCandidate : undefined;
}

function isPathWithin(candidate: string, directory: string): boolean {
  const directoryWithSep = directory.endsWith(path.sep) ? directory : directory + path.sep;
  return candidate === directory || candidate.startsWith(directoryWithSep);
}

async function realpathIfReadable(targetPath: string): Promise<string | undefined> {
  try {
    return await fs.realpath(targetPath);
  } catch {
    return undefined;
  }
}

async function resolveLogPath(override?: string): Promise<string | undefined> {
  if (override && typeof override === 'string' && override.trim()) {
    if (!override.toLowerCase().endsWith('.log')) {
      log.warn(`Blocked attempt to read non-log file: ${override}`);
      return undefined;
    }

    const realOverride = await resolveAllowedLogFile(override);
    if (!realOverride) {
      log.warn(`Blocked attempt to read log from unauthorized location: ${override}`);
      return undefined;
    }

    cachedLogPath = realOverride;
    return cachedLogPath;
  }

  if (cachedLogPath) {
    const safeCachedPath = await resolveAllowedLogFile(cachedLogPath);
    if (safeCachedPath) {
      cachedLogPath = safeCachedPath;
      return cachedLogPath;
    }
    cachedLogPath = undefined;
  }

  const projectRoot = getProjectRoot(loadEnv().UE_PROJECT_PATH);
  if (projectRoot) {
    const envLog = await findLatestLogInDir(path.join(projectRoot, 'Saved', 'Logs'));
    if (envLog) {
      return envLog;
    }
  }

  return await findLatestLogInDir(path.join(process.cwd(), 'Saved', 'Logs'));
}

async function findLatestLogInDir(dir: string): Promise<string | undefined> {
  if (!dir) return undefined;
  try {
    const realDir = await realpathIfReadable(dir);
    if (!realDir) return undefined;

    const entries = await fs.readdir(realDir);
    const logFiles = entries.filter(name => name.toLowerCase().endsWith('.log'));
    if (logFiles.length === 0) return undefined;

    const stats = await Promise.all(logFiles.map(async name => {
      const fp = path.join(realDir, name);
      try {
        const realFile = await realpathIfReadable(fp);
        if (!realFile || !isPathWithin(realFile, realDir)) return null;

        const st = await fs.stat(realFile);
        if (!st.isFile()) return null;

        return { p: realFile, m: st.mtimeMs };
      } catch {
        return null;
      }
    }));
    const candidates = stats.filter((s): s is { p: string; m: number } => s !== null);

    if (candidates.length) {
      candidates.sort((a, b) => b.m - a.m);
      cachedLogPath = candidates[0].p;
      return cachedLogPath;
    }
  } catch (error) {
    log.debug('Unable to scan log directory', { dir, error });
  }
  return undefined;
}

async function tailFile(filePath: string, maxLines: number): Promise<string> {
  const handle = await fs.open(filePath, 'r');
  try {
    const stat = await handle.stat();
    const chunkSize = 128 * 1024;
    let position = stat.size;
    let remaining = '';
    const lines: string[] = [];
    while (position > 0 && lines.length < maxLines) {
      const readSize = Math.min(chunkSize, position);
      position -= readSize;
      const buf = Buffer.alloc(readSize);
      await handle.read(buf, 0, readSize, position);
      remaining = buf.toString('utf8') + remaining;
      const parts = remaining.split(/\r?\n/);
      remaining = parts.shift() || '';
      while (parts.length) {
        const line = parts.pop();
        if (line === undefined) break;
        if (line.length === 0) continue;
        lines.unshift(line);
        if (lines.length >= maxLines) break;
      }
    }
    if (lines.length < maxLines && remaining) {
      lines.unshift(remaining);
    }
    return lines.slice(0, maxLines).join('\n');
  } finally {
    try {
      await handle.close();
    } catch (error) {
      log.debug('Failed to close log file handle', { filePath, error });
    }
  }
}

function parseLine(line: string): LogEntry {
  const m1 = line.match(/^\[?(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d+)\]?\s*\[(.*?)\]\s*(.*)$/);
  if (m1) {
    const rest = m1[3];
    const m2 = rest.match(/^(\w+):\s*(Error|Warning|Display|Log|Verbose|VeryVerbose):\s*(.*)$/);
    if (m2) {
      return { timestamp: m1[1], category: m2[1], level: m2[2] === 'Display' ? 'Log' : m2[2], message: m2[3] };
    }
    const m3 = rest.match(/^(\w+):\s*(.*)$/);
    if (m3) {
      return { timestamp: m1[1], category: m3[1], level: 'Log', message: m3[2] };
    }
    return { timestamp: m1[1], message: rest };
  }
  const m = line.match(/^(\w+):\s*(Error|Warning|Display|Log|Verbose|VeryVerbose):\s*(.*)$/);
  if (m) {
    return { category: m[1], level: m[2] === 'Display' ? 'Log' : m[2], message: m[3] };
  }
  const mAlt = line.match(/^(\w+):\s*(.*)$/);
  if (mAlt) {
    return { category: mAlt[1], level: 'Log', message: mAlt[2] };
  }
  return { message: line };
}

function isInternalLogEntry(entry: LogEntry): boolean {
  const category = entry.category?.toLowerCase() || '';
  const message = entry.message?.trim() || '';
  return (category === 'logpython' && message.startsWith('RESULT:')) ||
    (!entry.category && message.startsWith('[') && message.includes('LogPython: RESULT:'));
}

export async function readOutputLog(params: ReadLogParams) {
  const target = await resolveLogPath(params.logPath);
  if (!target) {
    return { success: false, error: 'Log file not found' };
  }
  const maxLines = typeof params.lines === 'number' && params.lines > 0 ? Math.min(params.lines, 2000) : 200;
  let text = '';
  try {
    text = await tailFile(target, maxLines);
  } catch (error: unknown) {
    const errObj = error as Record<string, unknown> | null;
    return { success: false, error: String(errObj?.message || error) };
  }
  const parsed = text.split(/\r?\n/).filter(line => line.length > 0).map(line => parseLine(line));
  const mappedLevel = params.filterLevel || 'All';
  const includeCats = Array.isArray(params.filterCategory) && params.filterCategory.length ? new Set(params.filterCategory) : undefined;
  const includePrefixes = Array.isArray(params.includePrefixes) && params.includePrefixes.length ? params.includePrefixes : undefined;
  const excludeCats = Array.isArray(params.excludeCategories) && params.excludeCategories.length ? new Set(params.excludeCategories) : undefined;
  const filtered = parsed.filter(entry => {
    if (mappedLevel && mappedLevel !== 'All') {
      const level = entry.level || 'Log';
      if (level === 'Display') {
        if (mappedLevel !== 'Log') return false;
      } else if (level !== mappedLevel) {
        return false;
      }
    }
    if (includeCats && entry.category && !includeCats.has(entry.category)) return false;
    if (includePrefixes && includePrefixes.length && entry.category) {
      if (!includePrefixes.some(prefix => (entry.category ?? '').startsWith(prefix))) return false;
    }
    if (excludeCats && entry.category && excludeCats.has(entry.category)) return false;
    return true;
  });
  const includeInternal = Boolean(
    (includeCats && includeCats.has('LogPython')) ||
    (includePrefixes && includePrefixes.some(prefix => 'LogPython'.startsWith(prefix)))
  );
  const sanitized = includeInternal ? filtered : filtered.filter(entry => !isInternalLogEntry(entry));
  return { success: true, logPath: target.replace(/\\/g, '/'), entries: sanitized, filteredCount: sanitized.length };
}
