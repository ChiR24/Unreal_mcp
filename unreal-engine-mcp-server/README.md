# Unreal Engine 5.6 MCP Server

An MCP server that connects AI assistants (Claude, Cursor, etc.) to Unreal Engine 5.6 via the Remote Control API (WebSocket + HTTP). Optional Python Script Plugin support will be added for complex operations.

## Prerequisites
- Node.js >= 18
- Unreal Engine 5.6 Editor
- UE Plugins enabled: Remote Control, Python, Editor Scripting Utilities

## Configure Unreal Remote Control
- Project Settings -> Plugins -> Remote Control
- Enable Allow Remote Control
- WebSocket Port: 30010
- HTTP Port: 30020

## Run the server

Install deps and build:

```powershell
npm install
npm run build
```

Run:

```powershell
node dist/cli.js
```

Or add to Claude/Cursor MCP config:

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "node",
      "args": ["X:/Newfolder(2)/MCP/Unreal 4.1/unreal-engine-mcp-server/dist/cli.js"],
      "env": {
        "UE_HOST": "127.0.0.1",
        "UE_RC_WS_PORT": "30010",
        "UE_RC_HTTP_PORT": "30020"
      }
    }
  }
}
```

## Current Endpoints
- Resource: `assets/list` params: `{ dir?: string; recursive?: boolean }`
- More tools/resources/prompts will be added in subsequent phases.
