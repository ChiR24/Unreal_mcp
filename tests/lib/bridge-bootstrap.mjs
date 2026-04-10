import net from "node:net";

const DEFAULT_AUTOMATION_HOST = "127.0.0.1";
const DEFAULT_AUTOMATION_PORTS = [8091, 8090];

function sanitizePort(value) {
  if (typeof value === "number" && Number.isInteger(value)) {
    return value > 0 && value <= 65535 ? value : null;
  }

  if (typeof value === "string" && value.trim().length > 0) {
    const parsed = Number.parseInt(value.trim(), 10);
    return Number.isInteger(parsed) && parsed > 0 && parsed <= 65535
      ? parsed
      : null;
  }

  return null;
}

function orderedUniquePorts(primaryPort, values) {
  const configuredPorts = Array.isArray(values)
    ? values.map((value) => sanitizePort(value)).filter((port) => port !== null)
    : [];
  const fallbackPorts =
    configuredPorts.length > 0
      ? configuredPorts
      : [...DEFAULT_AUTOMATION_PORTS];

  return Array.from(
    new Set([
      primaryPort,
      ...fallbackPorts.filter((port) => port !== primaryPort),
    ]),
  );
}

export function getBridgeBootstrapConfig(env = process.env) {
  const host =
    env.MCP_AUTOMATION_HOST ??
    env.MCP_AUTOMATION_WS_HOST ??
    DEFAULT_AUTOMATION_HOST;
  const primaryPort =
    sanitizePort(env.MCP_AUTOMATION_PORT ?? env.MCP_AUTOMATION_WS_PORT) ??
    DEFAULT_AUTOMATION_PORTS[0];
  const configuredPorts = env.MCP_AUTOMATION_WS_PORTS
    ? env.MCP_AUTOMATION_WS_PORTS.split(",")
        .map((token) => token.trim())
        .filter((token) => token.length > 0)
    : undefined;

  return {
    host,
    ports: orderedUniquePorts(primaryPort, configuredPorts),
  };
}

async function defaultProbePort(host, port) {
  try {
    await new Promise((resolve, reject) => {
      const sock = new net.Socket();
      let settled = false;

      sock.setTimeout(1000);
      sock.once("connect", () => {
        settled = true;
        sock.destroy();
        resolve(true);
      });
      sock.once("timeout", () => {
        if (!settled) {
          settled = true;
          sock.destroy();
          reject(new Error("timeout"));
        }
      });
      sock.once("error", () => {
        if (!settled) {
          settled = true;
          sock.destroy();
          reject(new Error("error"));
        }
      });

      sock.connect(port, host);
    });

    return true;
  } catch {
    return false;
  }
}

export function buildBridgeBootstrapError(host, ports, timeoutMs) {
  const attemptedTargets = ports.map((port) => `ws://${host}:${port}`);
  const guidance =
    "Start Unreal Editor with the MCP Automation Bridge plugin enabled, or override MCP_AUTOMATION_HOST, MCP_AUTOMATION_PORT, and MCP_AUTOMATION_WS_PORTS to match the active listener.";
  const error = new Error(
    `Unable to reach the automation bridge at ${attemptedTargets.join(" -> ")} within ${timeoutMs}ms. ${guidance}`,
  );

  error.code = "AUTOMATION_BRIDGE_UNAVAILABLE";
  error.host = host;
  error.ports = [...ports];
  error.timeoutMs = timeoutMs;
  error.guidance = guidance;
  error.attemptedTargets = attemptedTargets;

  return error;
}

export async function waitForAnyPort(
  host,
  ports,
  timeoutMs = 10000,
  probePort = defaultProbePort,
) {
  const orderedPorts =
    Array.isArray(ports) && ports.length > 0
      ? [...ports]
      : [...DEFAULT_AUTOMATION_PORTS];
  const start = Date.now();

  while (Date.now() - start < timeoutMs) {
    for (const port of orderedPorts) {
      const reachable = await probePort(host, port);
      if (reachable) {
        return {
          host,
          port,
          ports: [
            port,
            ...orderedPorts.filter((candidate) => candidate !== port),
          ],
        };
      }
    }

    await new Promise((resolve) => setImmediate(resolve));
  }

  throw buildBridgeBootstrapError(host, orderedPorts, timeoutMs);
}

export function prepareLiveServerEnv(baseEnv, listener) {
  const orderedPorts =
    Array.isArray(listener?.ports) && listener.ports.length > 0
      ? [
          listener.port,
          ...listener.ports.filter((port) => port !== listener.port),
        ]
      : [listener.port];

  return {
    ...baseEnv,
    MCP_AUTOMATION_HOST: listener.host,
    MCP_AUTOMATION_PORT: String(listener.port),
    MCP_AUTOMATION_WS_HOST: listener.host,
    MCP_AUTOMATION_WS_PORT: String(listener.port),
    MCP_AUTOMATION_WS_PORTS: orderedPorts.join(","),
    MCP_AUTOMATION_CLIENT_HOST: listener.host,
    MCP_AUTOMATION_CLIENT_PORT: String(listener.port),
  };
}
