#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    FString MakeOpenCodeConfig()
    {
        return FString()
            + TEXT("// unreal_agent_studio_kit_version: 1\n")
            + TEXT("{\n")
            + TEXT("  \"$schema\": \"https://opencode.ai/config.json\",\n")
            + TEXT("  \"permission\": {\n")
            + TEXT("    \"*\": \"ask\",\n")
            + TEXT("    \"read\": \"ask\",\n")
            + TEXT("    \"glob\": \"ask\",\n")
            + TEXT("    \"grep\": \"ask\",\n")
            + TEXT("    \"list\": \"ask\",\n")
            + TEXT("    \"edit\": \"ask\",\n")
            + TEXT("    \"write\": \"ask\",\n")
            + TEXT("    \"patch\": \"ask\",\n")
            + TEXT("    \"apply_patch\": \"ask\",\n")
            + TEXT("    \"bash\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_tools\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_asset\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_blueprint\": \"ask\",\n")
            + TEXT("    \"unreal-engine_control_actor\": \"ask\",\n")
            + TEXT("    \"unreal-engine_control_editor\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_level\": \"ask\",\n")
            + TEXT("    \"unreal-engine_build_environment\": \"ask\",\n")
            + TEXT("    \"unreal-engine_animation_physics\": \"ask\",\n")
            + TEXT("    \"unreal-engine_system_control\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_sequence\": \"ask\",\n")
            + TEXT("    \"unreal-engine_inspect\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_audio\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_geometry\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_pcg\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_effect\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_gas\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_character\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_combat\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_ai\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_inventory\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_interaction\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_networking\": \"ask\",\n")
            + TEXT("    \"unreal-engine_manage_level_structure\": \"ask\",\n")
            + TEXT("    \"skill\": {\n")
            + TEXT("      \"unreal-*\": \"allow\"\n")
            + TEXT("    }\n")
            + TEXT("  }\n")
            + TEXT("}\n");
    }

    FString MakeLegacyOpenCodeConfig()
    {
        // Returns the true pre-tightening legacy OpenCode config shape
        // (read/glob/grep/list = "allow"). This is what users had on disk
        // before the policy tightening in MakeOpenCodeConfig() switched
        // those four operations to "ask" and added the explicit
        // `unreal-engine_*` allowlist. Must stay byte-identical to the
        // LegacyConfig constant in LooksLikeLegacyOpenCodeConfig() so the
        // upgrade path recognizes the prior shape.
        return FString()
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

    bool LooksLikeLegacyOpenCodeConfig(const FString& ExistingText)
    {
        const FString LegacyConfig = FString()
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
        return ExistingText == LegacyConfig || ExistingText == MakeLegacyOpenCodeConfig();
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
