#include "UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    FString MakeGuardrailsPlugin()
    {
        return FString()
            + TEXT("// unreal_agent_studio_kit_version: 1\n")
            + TEXT("import type { Plugin } from \"@opencode-ai/plugin\"\n\n")
            + TEXT("const SECRET_PATTERNS = [\n")
            + TEXT("  /x-mcp-capability-token/iu,\n")
            + TEXT("  /authorization\\s*:/iu,\n")
            + TEXT("  /bearer\\s+[a-z0-9._\\-]+/iu,\n")
            + TEXT("  /api[_-]?key/iu,\n")
            + TEXT("  /access[_-]?token/iu,\n")
            + TEXT("  /refresh[_-]?token/iu,\n")
            + TEXT("  /password/iu,\n")
            + TEXT("  /secret/iu,\n")
            + TEXT("]\n\n")
            + TEXT("function redact(value: unknown): unknown {\n")
            + TEXT("  if (typeof value === \"string\") {\n")
            + TEXT("    if (SECRET_PATTERNS.some((pattern) => pattern.test(value))) return \"[REDACTED]\"\n")
            + TEXT("    return value\n")
            + TEXT("  }\n")
            + TEXT("  if (Array.isArray(value)) return value.map(redact)\n")
            + TEXT("  if (value && typeof value === \"object\") {\n")
            + TEXT("    return Object.fromEntries(Object.entries(value as Record<string, unknown>).map(([key, item]) => {\n")
            + TEXT("      if (SECRET_PATTERNS.some((pattern) => pattern.test(key))) return [key, \"[REDACTED]\"]\n")
            + TEXT("      return [key, redact(item)]\n")
            + TEXT("    }))\n")
            + TEXT("  }\n")
            + TEXT("  return value\n")
            + TEXT("}\n\n")
            + TEXT("export const UnrealAgentGuardrails: Plugin = async () => ({\n")
            + TEXT("  \"tool.execute.before\": async (_input, output) => {\n")
            + TEXT("    if (output && typeof output === \"object\" && \"args\" in output) {\n")
            + TEXT("      ;(output as { args?: unknown }).args = redact((output as { args?: unknown }).args)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("  \"tool.execute.after\": async (_input, output) => {\n")
            + TEXT("    const redacted = redact(output)\n")
            + TEXT("    if (output && typeof output === \"object\" && redacted && typeof redacted === \"object\") {\n")
            + TEXT("      Object.assign(output as Record<string, unknown>, redacted as Record<string, unknown>)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("  \"experimental.session.compacting\": async (_input, output) => {\n")
            + TEXT("    const reminder = \"Unreal Agent reminder: inspect before live editor claims, prefer reversible changes, validate with evidence, and do not expose capability tokens or secrets.\"\n")
            + TEXT("    if (output && typeof output === \"object\" && Array.isArray((output as { context?: unknown }).context)) {\n")
            + TEXT("      ;((output as { context: string[] }).context).push(reminder)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("})\n\n")
            + TEXT("export default UnrealAgentGuardrails\n");
    }

    FString MakeOpenCodeConfig()
    {
        return FString()
            + TEXT("// unreal_agent_studio_kit_version: 1\n")
            + TEXT("{\n")
            + TEXT("  \"$schema\": \"https://opencode.ai/config.json\",\n")
            + TEXT("  \"permission\": {\n")
            + TEXT("    \"read\": \"allow\",\n")
            + TEXT("    \"glob\": \"allow\",\n")
            + TEXT("    \"grep\": \"allow\",\n")
            + TEXT("    \"list\": \"allow\",\n")
            + TEXT("    \"edit\": \"ask\",\n")
            + TEXT("    \"bash\": \"ask\",\n")
            + TEXT("    \"skill\": {\n")
            + TEXT("      \"unreal-*\": \"allow\"\n")
            + TEXT("    }\n")
            + TEXT("  }\n")
            + TEXT("}\n");
    }

    FString MakeLegacyOpenCodeConfig()
    {
        FString Config = MakeOpenCodeConfig();
        Config.RemoveFromStart(TEXT("// unreal_agent_studio_kit_version: 1\n"));
        return Config;
    }

    FString MakeEvidenceReadme()
    {
        return FString()
            + TEXT("# Unreal Agent Evidence\n\n")
            + FString::Printf(TEXT("%s\n\n"), StudioKitVersionMarker)
            + TEXT("This folder is managed by the Unreal Agent editor plugin. It stores compact validation events, decisions, and release evidence so the agent can report what was actually checked instead of guessing.\n");
    }

    void AppendConfigTemplates(TArray<FStudioKitTemplateFile>& Templates)
    {
        AddTemplate(Templates, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"), MakeGuardrailsPlugin());
        AddTemplate(Templates, TEXT(".opencode/opencode.json"), MakeOpenCodeConfig());
        AddTemplate(Templates, TEXT("Saved/UnrealAgent/evidence/README.md"), MakeEvidenceReadme());
    }
}
