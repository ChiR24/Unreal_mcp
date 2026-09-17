import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { Logger } from '../../../src/utils/logging/logger.js';
import { resolveAutomationBridgeConfig } from '../../../src/automation/bridge-config.js';

const logger = new Logger('bridge-config-port-test', 'error');

const PORT_ENV_KEYS = [
    'MCP_AUTOMATION_CLIENT_PORT',
    'MCP_AUTOMATION_WS_PORT',
    'MCP_AUTOMATION_PORT',
    'MCP_AUTOMATION_WS_PORTS',
    'UE_PROJECT_PATH'
] as const;

const savedEnv = new Map<string, string | undefined>();
const tempDirs: string[] = [];

function makeProject(options: { defaultGameIni?: string; savedGameIni?: string }): string {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'bridge-port-'));
    tempDirs.push(dir);

    const write = (relative: string, content: string): void => {
        const file = path.join(dir, relative);
        fs.mkdirSync(path.dirname(file), { recursive: true });
        fs.writeFileSync(file, content, 'utf-8');
    };

    if (options.defaultGameIni !== undefined) write(path.join('Config', 'DefaultGame.ini'), options.defaultGameIni);
    if (options.savedGameIni !== undefined) write(path.join('Saved', 'Config', 'WindowsEditor', 'Game.ini'), options.savedGameIni);

    return dir;
}

function bridgeSettings(ports: string): string {
    return `[/Script/McpAutomationBridge.McpAutomationBridgeSettings]\nListenPorts=${ports}\n`;
}

beforeEach(() => {
    for (const key of PORT_ENV_KEYS) {
        savedEnv.set(key, process.env[key]);
        delete process.env[key];
    }
});

afterEach(() => {
    for (const [key, value] of savedEnv) {
        if (value === undefined) delete process.env[key];
        else process.env[key] = value;
    }
    savedEnv.clear();

    for (const dir of tempDirs.splice(0)) {
        fs.rmSync(dir, { recursive: true, force: true });
    }
});

describe('bridge port resolution', () => {
    it('uses the first ListenPorts token from the project config when no port is configured', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('8092,8093') });

        const resolved = resolveAutomationBridgeConfig({}, logger);

        expect(resolved.port).toBe(8092);
        expect(resolved.clientPort).toBe(8092);
        expect(resolved.ports).toEqual([8092]);
    });

    it('keeps the built-in default when the project sets no ListenPorts', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: '[/Script/Other.Settings]\nListenPorts=8092\n' });

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8090);
    });

    it('keeps the built-in default when no project path is known', () => {
        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8090);
    });

    it('reads the saved per-platform config when the Default config is absent', () => {
        process.env.UE_PROJECT_PATH = makeProject({ savedGameIni: bridgeSettings('8094') });

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8094);
    });

    it('accepts a .uproject path as well as a directory', () => {
        const projectDir = makeProject({ defaultGameIni: bridgeSettings('8092') });
        process.env.UE_PROJECT_PATH = path.join(projectDir, 'Engine_Test_Cell.uproject');

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8092);
    });

    it('lets an explicit environment port win over the project config', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('8092') });
        process.env.MCP_AUTOMATION_PORT = '8099';

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8099);
    });

    it('lets an explicit option win over the project config', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('8092') });

        expect(resolveAutomationBridgeConfig({ port: 8097 }, logger).clientPort).toBe(8097);
    });

    it('does not consult the project config when a port list is configured explicitly', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('8092') });
        process.env.MCP_AUTOMATION_WS_PORTS = '8090,8091';

        const resolved = resolveAutomationBridgeConfig({}, logger);

        expect(resolved.clientPort).toBe(8090);
        expect(resolved.ports).toEqual([8090, 8091]);
    });

    it('still uses the project config when an explicit scalar port is unusable', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('8092') });
        process.env.MCP_AUTOMATION_PORT = 'not-a-port';

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8092);
    });

    it('falls back to the default when the project ListenPorts value is unusable', () => {
        process.env.UE_PROJECT_PATH = makeProject({ defaultGameIni: bridgeSettings('not-a-port') });

        expect(resolveAutomationBridgeConfig({}, logger).clientPort).toBe(8090);
    });
});
