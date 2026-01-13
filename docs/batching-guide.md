# Batch Execution Guide

## Overview

The `batch_execute` command allows the MCP server to send multiple automation requests to the Unreal Engine bridge in a single WebSocket message. This significantly reduces network overhead and improves performance for bulk operations, such as validating hundreds of assets.

## How It Works

The `system_control` tool exposes a `batch_execute` action. This action takes an array of `requests`, wraps them in a single JSON payload, and sends them to the C++ Automation Bridge. The C++ plugin processes them sequentially (or in parallel depending on the implementation) and returns an array of results.

### Protocol

**Request Payload:**
```json
{
  "action": "batch_execute",
  "requests": [
    {
      "action": "validate_asset",
      "requestId": "0",
      "assetPath": "/Game/Assets/MyAsset1"
    },
    {
      "action": "validate_asset",
      "requestId": "1",
      "assetPath": "/Game/Assets/MyAsset2"
    }
  ]
}
```

**Response Payload:**
```json
{
  "results": [
    {
      "requestId": "0",
      "success": true
    },
    {
      "requestId": "1",
      "success": false,
      "error": "Asset contains invalid data"
    }
  ]
}
```

## Using `batch_execute` in Handlers

To refactor a sequential loop into a batch request, follow these steps:

1.  **Construct Requests:** Map your input array to an array of request objects. Ensure each request has a unique `requestId` (usually the index).
2.  **Execute Batch:** Call `executeAutomationRequest` with `action: 'batch_execute'`.
3.  **Map Results:** Iterate through the returned `results` array and map them back to your original input order using `requestId`.

### Example: Refactoring `validate_assets`

**Before (Sequential Loop):**
```typescript
const results = [];
for (const path of paths) {
  // 1. Network round-trip per asset
  const res = await tools.assetTools.validate({ assetPath: path });
  results.push(res);
}
return { results };
```

**After (Batch Execution):**
```typescript
// 1. Construct batch requests
const requests = paths.map((path, index) => ({
  action: 'validate_asset',
  requestId: String(index),
  assetPath: path
}));

// 2. Single network round-trip
const batchRes = await executeAutomationRequest(tools, 'system_control', {
  action: 'batch_execute',
  requests
});

// 3. Map results back to order
const resultsMap = new Map();
if (Array.isArray(batchRes.results)) {
  for (const r of batchRes.results) {
    resultsMap.set(r.requestId, r);
  }
}

const results = requests.map((req, index) => {
  const r = resultsMap.get(String(index));
  return {
    assetPath: req.assetPath,
    success: r?.success ?? false,
    error: r?.error
  };
});

return { results };
```

## Best Practices

*   **Request IDs:** Always provide a `requestId` to correlate responses, as the bridge might not guarantee order preservation in future async implementations.
*   **Error Handling:** Handle cases where a specific request in the batch fails (returns `success: false`) vs. the entire batch failing (network error).
*   **Chunking:** For extremely large batches (e.g., >10,000 items), consider chunking them into groups of 500-1000 to avoid timeouts or excessive memory usage.

## Client Usage

Clients (LLMs or scripts) can use `batch_execute` indirectly via tools like `system.validate_assets` which implement this logic internally. Direct usage of `batch_execute` via `system_control` is also possible if the client constructs the raw request payload manually.
