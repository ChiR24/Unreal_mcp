import fs from 'node:fs/promises';
import fsSync from 'node:fs';
import path from 'node:path';

function parseIniContent(content: string): Record<string, Record<string, string>> {
    const result: Record<string, Record<string, string>> = Object.create(null);
    let currentSection = '';

    const lines = content.split(/\r?\n/);
    for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed || trimmed.startsWith(';') || trimmed.startsWith('#')) {
            continue;
        }

        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            currentSection = trimmed.substring(1, trimmed.length - 1);
            result[currentSection] = Object.create(null) as Record<string, string>;
        } else if (currentSection) {
            const parts = trimmed.split('=');
            if (parts.length >= 2) {
                const key = parts[0].trim();
                const value = parts.slice(1).join('=').trim();
                result[currentSection][key] = value;
            }
        }
    }

    return result;
}

export async function readIniFile(filePath: string): Promise<Record<string, Record<string, string>>> {
  try {
    const content = await fs.readFile(filePath, 'utf-8');
    return parseIniContent(content);
  } catch (error) {
    throw new Error(`Failed to read INI file at ${filePath}: ${error instanceof Error ? error.message : String(error)}`);
  }
}

/** Sync counterpart of {@link readIniFile} for callers that resolve during construction. */
export function readIniFileSync(filePath: string): Record<string, Record<string, string>> {
  try {
    const content = fsSync.readFileSync(filePath, 'utf-8');
    return parseIniContent(content);
  } catch (error) {
    throw new Error(`Failed to read INI file at ${filePath}: ${error instanceof Error ? error.message : String(error)}`);
  }
}

function projectConfigCandidates(projectPath: string, category: string): string[] | null {
    let dirPath = projectPath;
    if (dirPath.toLowerCase().endsWith('.uproject')) {
        dirPath = path.dirname(dirPath);
    }

    // Possible file names/locations in order of preference (Config/DefaultX.ini, Saved/Config/WindowsEditor/X.ini)
    // category is usually 'Project', 'Engine', 'Game', 'Input', etc.
    const cleanCategory = category.replace(/^Default/, ''); // If caller passed 'DefaultEngine', normalize to 'Engine'

    // Security check: category should not contain path traversal characters
    // We strictly allow alphanumeric, underscore, and hyphen characters for categories.
    // Unreal categories are typically "Engine", "Game", "Input", "Editor", "Scalability", etc.
    if (!/^[a-zA-Z0-9_-]+$/.test(cleanCategory)) {
        return null;
    }

    return [
        path.join(dirPath, 'Config', `Default${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'WindowsEditor', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Windows', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Mac', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Linux', `${cleanCategory}.ini`)
    ];
}

function pickProjectSetting(
    iniData: Record<string, Record<string, string>> | null,
    sectionName: string,
    key?: string
): Record<string, unknown> | string | null {
    if (!iniData) return null;

    if (sectionName) {
        const section = iniData[sectionName];
        if (section) {
            if (key) {
                // Only return if the key actually exists (not undefined)
                if (key in section) {
                    return section[key];
                }
                // Key not found in this section, continue to next candidate
            } else {
                return section;
            }
        }
        // If section not found in this file, continue to next candidate
    } else if (Object.keys(iniData).length > 0) {
        return iniData;
    }

    return null;
}

export async function getProjectSetting(projectPath: string, category: string, sectionName: string, key?: string): Promise<Record<string, unknown> | string | null> {
    const candidates = projectConfigCandidates(projectPath, category);
    if (!candidates) return null;

    // Use Promise.all for parallel file reads - significantly faster than sequential
    // We read all candidates in parallel, then pick the highest-priority one that succeeds
    const readPromises = candidates.map(async (configPath, index) => {
        try {
            const iniData = await readIniFile(configPath);
            return { index, iniData, success: true };
        } catch {
            return { index, iniData: null, success: false };
        }
    });

    const results = await Promise.all(readPromises);

    // Process results in priority order (candidates array order)
    for (const result of results.sort((a, b) => a.index - b.index)) {
        if (!result.success || !result.iniData) continue;
        const picked = pickProjectSetting(result.iniData, sectionName, key);
        if (picked !== null) return picked;
    }

    return null;
}

/**
 * Sync counterpart of {@link getProjectSetting}. Only for values that must be
 * resolved before the first await (for example bridge port selection), since it
 * blocks the event loop on file IO.
 */
export function getProjectSettingSync(projectPath: string, category: string, sectionName: string, key?: string): Record<string, unknown> | string | null {
    const candidates = projectConfigCandidates(projectPath, category);
    if (!candidates) return null;

    for (const configPath of candidates) {
        let iniData: Record<string, Record<string, string>>;
        try {
            iniData = readIniFileSync(configPath);
        } catch {
            continue;
        }
        const picked = pickProjectSetting(iniData, sectionName, key);
        if (picked !== null) return picked;
    }

    return null;
}
