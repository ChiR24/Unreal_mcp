import { UnrealBridge } from '../unreal-bridge.js'
import { loadEnv } from '../types/env.js'
import { Logger } from '../utils/logger.js'
import { promises as fs } from 'fs'
import path from 'path'

type ReadParams = {
  filterCategory?: string[]
  filterLevel?: 'Error' | 'Warning' | 'Log' | 'Verbose' | 'VeryVerbose' | 'All'
  lines?: number
  logPath?: string
  includePrefixes?: string[]
  excludeCategories?: string[]
}

type Entry = {
  timestamp?: string
  category?: string
  level?: string
  message: string
}

export class LogTools {
  private env = loadEnv()
  private log = new Logger('LogTools')
  private cachedLogPath?: string
  constructor(private bridge: UnrealBridge) {}

  async readOutputLog(params: ReadParams) {
    const target = await this.resolveLogPath(params.logPath)
    if (!target) {
      return { success: false, error: 'Log file not found' }
    }
    const maxLines = typeof params.lines === 'number' && params.lines > 0 ? Math.min(params.lines, 2000) : 200
    let text = ''
    try {
      text = await this.tailFile(target, maxLines)
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) }
    }
    const rawLines = text.split(/\r?\n/).filter(l => l.length > 0)
    const parsed: Entry[] = rawLines.map(l => this.parseLine(l))
    const mappedLevel = params.filterLevel || 'All'
    const includeCats = Array.isArray(params.filterCategory) && params.filterCategory.length ? new Set(params.filterCategory) : undefined
    const includePrefixes = Array.isArray(params.includePrefixes) && params.includePrefixes.length ? params.includePrefixes : undefined
    const excludeCats = Array.isArray(params.excludeCategories) && params.excludeCategories.length ? new Set(params.excludeCategories) : undefined
    const filtered = parsed.filter(e => {
      if (!e) return false
      if (mappedLevel && mappedLevel !== 'All') {
        const lv = (e.level || 'Log')
        if (lv === 'Display') {
          if (mappedLevel !== 'Log') return false
        } else if (lv !== mappedLevel) {
          return false
        }
      }
      if (includeCats && e.category && !includeCats.has(e.category)) return false
      if (includePrefixes && includePrefixes.length && e.category) {
        if (!includePrefixes.some(p => e.category!.startsWith(p))) return false
      }
      if (excludeCats && e.category && excludeCats.has(e.category)) return false
      return true
    })
    const includeInternal = Boolean(
      (includeCats && includeCats.has('LogPython')) ||
      (includePrefixes && includePrefixes.some(p => 'LogPython'.startsWith(p)))
    )
    const sanitized = includeInternal ? filtered : filtered.filter(entry => !this.isInternalLogEntry(entry))
    return { success: true, logPath: target.replace(/\\/g, '/'), entries: sanitized, filteredCount: sanitized.length }
  }

  private async resolveLogPath(override?: string): Promise<string | undefined> {
    if (override && typeof override === 'string' && override.trim()) {
      try {
        const st = await fs.stat(override)
        if (st.isFile()) {
          return this.cacheLogPath(path.resolve(override))
        }
      } catch {}
    }

    if (this.cachedLogPath && (await this.fileExists(this.cachedLogPath))) {
      return this.cachedLogPath
    }

    const envLog = await this.resolveFromProjectEnv()
    if (envLog) {
      return envLog
    }

    if (this.bridge.isConnected) {
      try {
        const script = `
import unreal, json, os
paths = []
try:
    d = unreal.Paths.project_log_dir()
    if d:
        paths.append(os.path.abspath(d))
except Exception:
    pass
try:
    sd = unreal.Paths.project_saved_dir()
    if sd:
        p = os.path.join(sd, 'Logs')
        paths.append(os.path.abspath(p))
except Exception:
    pass
try:
    pf = unreal.Paths.get_project_file_path()
    if pf:
        pd = os.path.dirname(pf)
        p = os.path.join(pd, 'Saved', 'Logs')
        paths.append(os.path.abspath(p))
except Exception:
    pass
all_logs = []
for base in paths:
    try:
        if os.path.isdir(base):
            for name in os.listdir(base):
                if name.lower().endswith('.log'):
                    fp = os.path.join(base, name)
                    try:
                        m = os.path.getmtime(fp)
                        all_logs.append({'p': fp, 'm': m})
                    except Exception:
                        pass
    except Exception:
        pass
all_logs.sort(key=lambda x: x['m'], reverse=True)
print('RESULT:' + json.dumps({'dirs': paths, 'logs': all_logs}))
`.trim()
        const res = await this.bridge.executePythonWithResult(script)
        const logs = Array.isArray(res?.logs) ? res.logs : []
        for (const entry of logs) {
          const p = typeof entry?.p === 'string' ? entry.p : undefined
          if (p && p.trim()) return this.cacheLogPath(p)
        }
      } catch {}
    }
    const fallback = await this.findLatestLogInDir(path.join(process.cwd(), 'Saved', 'Logs'))
    if (fallback) {
      return fallback
    }
    return undefined
  }

  private async resolveFromProjectEnv(): Promise<string | undefined> {
    const projectPath = this.env.UE_PROJECT_PATH
    if (projectPath && typeof projectPath === 'string' && projectPath.trim()) {
      const projectDir = path.dirname(projectPath)
      const logsDir = path.join(projectDir, 'Saved', 'Logs')
      const envLog = await this.findLatestLogInDir(logsDir)
      if (envLog) {
        return envLog
      }
    }
    return undefined
  }

  private async findLatestLogInDir(dir: string): Promise<string | undefined> {
    if (!dir) return undefined
    try {
      const entries = await fs.readdir(dir)
      const candidates: { p: string; m: number }[] = []
      for (const name of entries) {
        if (!name.toLowerCase().endsWith('.log')) continue
        const fp = path.join(dir, name)
        try {
          const st = await fs.stat(fp)
          candidates.push({ p: fp, m: st.mtimeMs })
        } catch {}
      }
      if (candidates.length) {
        candidates.sort((a, b) => b.m - a.m)
        return this.cacheLogPath(candidates[0].p)
      }
    } catch {}
    return undefined
  }

  private async fileExists(filePath: string): Promise<boolean> {
    try {
      const st = await fs.stat(filePath)
      return st.isFile()
    } catch {
      return false
    }
  }

  private cacheLogPath(p: string): string {
    this.cachedLogPath = p
    return p
  }

  private async tailFile(filePath: string, maxLines: number): Promise<string> {
    const handle = await fs.open(filePath, 'r')
    try {
      const stat = await handle.stat()
      const chunkSize = 128 * 1024
      let position = stat.size
      let remaining = ''
      const lines: string[] = []
      while (position > 0 && lines.length < maxLines) {
        const readSize = Math.min(chunkSize, position)
        position -= readSize
        const buf = Buffer.alloc(readSize)
        await handle.read(buf, 0, readSize, position)
        remaining = buf.toString('utf8') + remaining
        const parts = remaining.split(/\r?\n/)
        remaining = parts.shift() || ''
        while (parts.length) {
          const line = parts.pop() as string
          if (line === undefined) break
          if (line.length === 0) continue
          lines.unshift(line)
          if (lines.length >= maxLines) break
        }
      }
      if (lines.length < maxLines && remaining) {
        lines.unshift(remaining)
      }
      return lines.slice(0, maxLines).join('\n')
    } finally {
      try { await handle.close() } catch {}
    }
  }

  private parseLine(line: string): Entry {
    const m1 = line.match(/^\[?(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d+)\]?\s*\[(.*?)\]\s*(.*)$/)
    if (m1) {
      const rest = m1[3]
      const m2 = rest.match(/^(\w+):\s*(Error|Warning|Display|Log|Verbose|VeryVerbose):\s*(.*)$/)
      if (m2) {
        return { timestamp: m1[1], category: m2[1], level: m2[2] === 'Display' ? 'Log' : m2[2], message: m2[3] }
      }
      const m3 = rest.match(/^(\w+):\s*(.*)$/)
      if (m3) {
        return { timestamp: m1[1], category: m3[1], level: 'Log', message: m3[2] }
      }
      return { timestamp: m1[1], message: rest }
    }
    const m = line.match(/^(\w+):\s*(Error|Warning|Display|Log|Verbose|VeryVerbose):\s*(.*)$/)
    if (m) {
      return { category: m[1], level: m[2] === 'Display' ? 'Log' : m[2], message: m[3] }
    }
    const mAlt = line.match(/^(\w+):\s*(.*)$/)
    if (mAlt) {
      return { category: mAlt[1], level: 'Log', message: mAlt[2] }
    }
    return { message: line }
  }

  private isInternalLogEntry(entry: Entry): boolean {
    if (!entry) return false
    const category = entry.category?.toLowerCase() || ''
    const message = entry.message?.trim() || ''
    if (category === 'logpython' && message.startsWith('RESULT:')) {
      return true
    }
    if (!entry.category && message.startsWith('[') && message.includes('LogPython: RESULT:')) {
      return true
    }
    return false
  }
}