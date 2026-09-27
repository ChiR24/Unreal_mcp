import fs from 'node:fs';
import path from 'node:path';

/** UE_PROJECT_PATH may name the .uproject file or its folder; this is the folder. */
function projectRoot(projectPath: string): string {
    return projectPath.toLowerCase().endsWith('.uproject') ? path.dirname(projectPath) : projectPath;
}

/** The project folder from UE_PROJECT_PATH, read at call time; undefined when unset. */
export function projectRootFromEnv(): string | undefined {
    const projectPath = process.env.UE_PROJECT_PATH;
    return projectPath ? projectRoot(projectPath) : undefined;
}

function readIniSection(filePath: string, sectionName: string): Record<string, string> | undefined {
    let content: string;
    try {
        content = fs.readFileSync(filePath, 'utf-8');
    } catch {
        return undefined;
    }

    let section: Record<string, string> | undefined;
    let inSection = false;
    for (const line of content.split(/\r?\n/)) {
        const trimmed = line.trim();
        if (!trimmed || trimmed.startsWith(';') || trimmed.startsWith('#')) continue;
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inSection = trimmed.slice(1, -1) === sectionName;
            if (inSection) section ??= Object.create(null) as Record<string, string>;
            continue;
        }
        const eq = trimmed.indexOf('=');
        if (inSection && section && eq > 0) section[trimmed.slice(0, eq).trim()] = trimmed.slice(eq + 1).trim();
    }
    return section;
}

/**
 * One key from a project's config, checked in Unreal's precedence order
 * (Config/Default<Category>.ini, then the per-platform Saved/Config copies).
 * Synchronous: callers resolve it before the first await (bridge port selection).
 */
export function readProjectIniValue(projectPath: string, category: string, sectionName: string, key: string): string | undefined {
    const dirPath = projectRoot(projectPath);
    const cleanCategory = category.replace(/^Default/, '');
    // The category becomes part of a file name, so refuse anything but a plain identifier.
    if (!/^[a-zA-Z0-9_-]+$/.test(cleanCategory)) return undefined;

    for (const configPath of [
        path.join(dirPath, 'Config', `Default${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'WindowsEditor', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Windows', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Mac', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Linux', `${cleanCategory}.ini`)
    ]) {
        const value = readIniSection(configPath, sectionName)?.[key];
        if (value !== undefined) return value;
    }
    return undefined;
}
