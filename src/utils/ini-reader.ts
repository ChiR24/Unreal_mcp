import fs from 'fs/promises';
import path from 'path';

export async function readIniFile(filePath: string): Promise<Record<string, Record<string, string>>> {
  try {
    const content = await fs.readFile(filePath, 'utf-8');
    const result: Record<string, Record<string, string>> = {};
    let currentSection = '';

    const lines = content.split(/\r?\n/);
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith(';') || trimmed.startsWith('#')) {
        continue;
      }

      if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
        currentSection = trimmed.substring(1, trimmed.length - 1);
        result[currentSection] = {};
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
  } catch (error) {
    throw new Error(`Failed to read INI file at ${filePath}: ${error instanceof Error ? error.message : String(error)}`);
  }
}

export async function getProjectSetting(projectPath: string, category: string, sectionName: string, key?: string): Promise<Record<string, unknown> | string | null> {
    // Normalize project path to directory
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

    const candidates = [
        path.join(dirPath, 'Config', `Default${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'WindowsEditor', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Windows', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Mac', `${cleanCategory}.ini`),
        path.join(dirPath, 'Saved', 'Config', 'Linux', `${cleanCategory}.ini`)
    ];

    for (const configPath of candidates) {
        try {
            const iniData = await readIniFile(configPath);
            // If we successfully read the file, check for the section
            if (sectionName) {
                const section = iniData[sectionName];
                if (section) {
                    if (key) {
                        return section[key];
                    }
                    return section;
                }
                // If section not found in this file, continue to next candidate? 
                // Usually we want the most authoritative, but if it's missing the section, maybe it's in another?
                // For now, if we find the file, we return the data from it or null if section missing.
                // Merging is complex without a proper config hierarchy implementation.
                // We will assume if the file exists, it's the one we want, or if section is missing, we fail for this file.
                // But 'Default' might lack user overrides.
                // Given this is a simple reader, we'll return the first match that contains the section, 
                // or if sectionName is empty, the first file found.
            } else {
                if (Object.keys(iniData).length > 0) {
                    return iniData;
                }
            }
        } catch (_e) {
            // Continue to next candidate
        }
    }

    return null;
}
