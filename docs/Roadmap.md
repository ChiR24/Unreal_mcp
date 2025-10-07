# Roadmap for Making MCP server smarter

## Phase 1: Living documents - current progress

The Living Documentation system isn't a single thing; it's a synergistic combination of three layers, two of which are built directly into your MCP's responses.
Layer 1: The Static "Rulebook" (The Foundation)
This is what we've already established. It's the consolidatedToolDefinitions JSON that you provide in the system prompt.
Its Job: To tell the LLM the fundamental laws of its universe. It defines what is possible and what is not.
Analogy: This is the game's instruction manual. It explains the pieces and the basic moves.
Limitation: A rulebook doesn't know the current state of the game board. It can't tell you where the pieces are right now.

Of course. Let's pull out the microscope and go deep into the engineering of each layer. We will discuss the specific logic, the data flow, and where these changes would live within your existing, well-structured TypeScript application.

### Deep Dive: Layer 2 - The "World Encyclopedia" (Dynamic Inquiry)

This is the most important layer to get right. It transforms your `inspect` tool from a simple data-fetcher into the core of the LLM's "understanding."

**Goal:** When the LLM calls `inspect({ action: 'inspect_object' })`, it should receive a response that is not just data, but curated, clean, and actionable documentation about that object's capabilities.

**Ground Zero:** The implementation logic lives in `src/tools/consolidated-tool-handlers.ts`, inside the main `switch (name)` block, under `case 'inspect':`.

---

#### Implementation Details within `handleConsolidatedToolCall`

Let's trace the execution for an `inspect_object` call:

**1. The Initial Call:**
The handler receives `name: 'inspect'` and `args: { action: 'inspect_object', objectPath: '...' }`.

**2. The Bridge to Reality:**
The handler needs to get the raw property data from Unreal. It will call a function on your `unreal-bridge.ts`. Let's assume you have or will create a helper method for this.

```typescript
// Inside handleConsolidatedToolCall, case 'inspect':
if (args.action === 'inspect_object') {
  const rawProperties = await tools.bridge.getObjectProperties(args.objectPath);
  // ... next steps ...
}
```

**3. The Raw Data Problem:**
The `getObjectProperties` function will return what the Unreal Remote Control API provides: a massive, unfiltered JSON object. It will be noisy and difficult for an LLM to parse. It might look something like this:

```json
// Raw, messy response from Unreal
{
  "RelativeLocation": { "X": 100, "Y": 50, "Z": 20 },
  "bHiddenInGame": false,
  "CreationMethod": "UserPlaced",
  "ComponentVelocity": { "X": 0, "Y": 0, "Z": 0 },
  "LightComponent": {
    "objectPath": "/Game/Maps/MyLevel.MyLevel:PersistentLevel.SpotLight_Main.SpotLightComponent_0",
    "Intensity": 5000.0,
    "LightColor": { "R": 255, "G": 255, "B": 255, "A": 255 }
  },
  "SomeInternalEngineProperty": null
  // ... and hundreds more properties
}
```
If you return this directly, the LLM will get confused. It's filled with read-only data, internal engine details, and has a complex nested structure.

**4. The Intelligence Layer: Curation and Transformation (This is the Deep Dive)**
This is where your MCP becomes "smart." Before returning, you process `rawProperties`. You will build a new function, maybe in `src/tools/inspect.ts` or a utility file, called `curateObjectProperties`.

```typescript
// Inside handleConsolidatedToolCall, case 'inspect':
if (args.action === 'inspect_object') {
  const rawProperties = await tools.bridge.getObjectProperties(args.objectPath);
  const curatedInfo = curateObjectProperties(rawProperties); // The magic happens here
  return { success: true, info: curatedInfo };
}
```

Here is what the `curateObjectProperties` function must do:

*   **A. Flatten the Structure:** The LLM works best with simple key-value pairs. Flatten the nested structure into a dot-notation format. `LightComponent.LightColor` is much easier to use as a `propertyName` than `{ "LightComponent": { "LightColor": ... } }`.

*   **B. Filter Out the Noise:** You need a set of rules to remove useless properties.
    *   **Keyword Filtering:** Ignore any property containing "Internal", "Transient", or starting with "bHidden".
    *   **Type Filtering:** Ignore complex, non-serializable types like object pointers or delegates, unless they are common structs like Vectors or Colors.
    *   **Read-Only Filtering:** The Remote Control API can tell you if a property is read-only. Discard these for `set_property` actions, but maybe keep them for pure inspection.

*   **C. Enrich the Data:** This is the most crucial step. You will add descriptive metadata to the remaining properties. You would maintain a "Property Dictionary" within your MCP.

    ```typescript
    // A file like src/tools/property-dictionary.ts
    export const propertyDictionary = {
      'Intensity': {
        description: 'The brightness of the light in lumens or lux.',
        commonValues: [1000, 5000, 10000],
        typeHint: 'float'
      },
      'LightColor': {
        description: 'The color of the light.',
        typeHint: 'FColor (object with R, G, B, A from 0-255)'
      },
      // ... hundreds more common properties
    };
    ```

*   **D. Construct the "Living Documentation" Payload:** The `curateObjectProperties` function iterates through the flattened, filtered properties and builds the final, clean response using the dictionary for enrichment.

The final, curated output is the beautiful, self-documenting JSON we discussed previously. This entire process—**Fetch Raw -> Flatten -> Filter -> Enrich -> Format**—is the technical implementation of creating a "World Encyclopedia" entry on the fly.

---

### Deep Dive: Layer 3 - The "Helpful Tutor" (Interactive Elicitation)

**Goal:** When the LLM calls a tool with missing required parameters, the MCP should not fail. It should ask a clarifying question that teaches the LLM the tool's requirements.

**Ground Zero:** This logic lives at the very top of your `handleConsolidatedToolCall` function, before the main `switch` statement, working in tandem with your `response-validator.ts`.

#### Implementation Details

**1. The Validation Trigger:**
Your AJV-based `responseValidator` is the key. When you validate the incoming `args` against the `inputSchema`, AJV produces an array of `ErrorObject`s if validation fails.

```typescript
// At the top of handleConsolidatedToolCall in consolidated-tool-handlers.ts
const validationErrors = validate(args); // Your validation function call

if (validationErrors) {
  // We have errors! Now, let's see if we can turn them into a lesson.
  // ... next steps ...
}
```

**2. The Elicitation Logic:**
You will now inspect `validationErrors` to see if it's a "teachable" moment. The most common teachable moment is a missing required property.

```typescript
// Inside the if (validationErrors) block
const requiredError = validationErrors.find(e => e.keyword === 'required');

if (requiredError) {
  const missingPropertyName = requiredError.params.missingProperty;

  // Now we build the elicitation response...
  // ...
} else {
  // It's a different, non-elicitable error (e.g., wrong data type).
  // Return a standard validation error response.
  return createValidationErrorResponse(validationErrors);
}
```

**3. Constructing the "Tutor's" Response:**
This is where you build the special elicitation payload. The goal is to give the LLM everything it needs to ask the user a good question.

*   **A. Find the Schema Definition:** You need to get the description and type of the property that's missing. You already have this! It's in the tool's `inputSchema`. You'll need a helper function to look it up.

    ```typescript
    // Continuing the if (requiredError) block
    const toolDefinition = consolidatedToolDefinitions.find(t => t.name === name);
    const missingPropertySchema = toolDefinition.inputSchema.properties[missingPropertyName];

    const elicitationPayload = {
      status: 'elicit_parameter',
      missing_parameter: {
        name: missingPropertyName,
        type: missingPropertySchema.type,
        description: missingPropertySchema.description,
        enum: missingPropertySchema.enum || undefined // Include enum if it exists!
      },
      message: `The action '${args.action}' requires the parameter '${missingPropertyName}'. ${missingPropertySchema.description}. What value should I use?`
    };

    return elicitationPayload; // Return this special object instead of an error
    ```

**4. The Client-Side Contract:**
This is crucial. The code that controls the LLM (the client) **must** be programmed to recognize the `{ "status": "elicit_parameter" }` response. When it receives this, it must not treat it as a final answer. Its logic should be:

1.  Receive the response.
2.  Check `if (response.status === 'elicit_parameter')`.
3.  If true, present the `response.message` to the end-user (or use the LLM to re-phrase it).
4.  Once the user provides the missing information, re-submit the *original* tool call, but this time with the missing parameter included.

This completes the loop, turning a hard failure into a smooth, interactive, and educational conversation.