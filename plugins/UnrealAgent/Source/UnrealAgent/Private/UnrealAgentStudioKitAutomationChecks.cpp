#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationTestDelegates.h"

#include "Acp/UnrealAgentEditorContext.h"
#include "Acp/UnrealAgentEvidenceLedger.h"
#include "Acp/UnrealAgentStudioKit.h"
#include "Acp/UnrealAgentValidationRunner.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitAndContextTest(FAutomationTestBase& Test)
    {
        const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitTest")));
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
        {
            Test.AddError(FString::Printf(TEXT("Failed to create Studio Kit test directory: %s"), *TestDirectory));
            return false;
        }

        bool bPassed = true;
        const FUnrealAgentStudioKitResult KitResult = FUnrealAgentStudioKit::EnsureForProject(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Studio Kit generation succeeds"), KitResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Studio Kit writes multiple OpenCode files"), KitResult.FilesWritten >= 10);

        FString PrimaryAgent;
        const FString PrimaryAgentPath = FPaths::Combine(TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(TEXT("Primary Unreal Agent file exists"), FPaths::FileExists(PrimaryAgentPath) && FFileHelper::LoadFileToString(PrimaryAgent, *PrimaryAgentPath));
        bPassed &= Test.TestTrue(TEXT("Primary agent has prompt marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetPromptVersionMarker()));
        bPassed &= Test.TestTrue(TEXT("Primary agent has Studio Kit marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes specialist roles"), PrimaryAgent.Contains(TEXT("unreal-technical-director")) && PrimaryAgent.Contains(TEXT("unreal-qa-release")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes MCP tool playbook"), PrimaryAgent.Contains(TEXT("MCP tool playbook")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes release workflow"), PrimaryAgent.Contains(TEXT("Shipping: confirm packaging readiness")));

        const FString SkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"));
        const FString PluginPath = FPaths::Combine(TestDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
        const FString CommandPath = FPaths::Combine(TestDirectory, TEXT(".opencode/commands/unreal-ship-check.md"));
        const FString ConfigPath = FPaths::Combine(TestDirectory, TEXT(".opencode/opencode.json"));
        bPassed &= Test.TestTrue(TEXT("Validation skill is generated"), FPaths::FileExists(SkillPath));
        bPassed &= Test.TestTrue(TEXT("Guardrails plugin is generated"), FPaths::FileExists(PluginPath));
        bPassed &= Test.TestTrue(TEXT("Ship check command is generated"), FPaths::FileExists(CommandPath));
        bPassed &= Test.TestTrue(TEXT("OpenCode config is generated"), FPaths::FileExists(ConfigPath));
        FString OpenCodeConfigText;
        bPassed &= Test.TestTrue(TEXT("OpenCode config is readable"), FFileHelper::LoadFileToString(OpenCodeConfigText, *ConfigPath));
        bPassed &= Test.TestTrue(TEXT("OpenCode config has Studio Kit comment marker"), OpenCodeConfigText.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
        bPassed &= Test.TestFalse(TEXT("OpenCode config does not contain unknown Studio Kit keys"), OpenCodeConfigText.Contains(TEXT("\"unreal_agent_studio_kit_version\"")));

        const FString LegacyConfigDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitLegacyConfigTest")));
        IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
        const FString LegacyConfigPath = FPaths::Combine(LegacyConfigDirectory, TEXT(".opencode/opencode.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(LegacyConfigPath), true);
        const FString LegacyConfigText = FString()
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
        bPassed &= Test.TestTrue(TEXT("Legacy generated OpenCode config is seeded"), FFileHelper::SaveStringToFile(LegacyConfigText, *LegacyConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FUnrealAgentStudioKit::EnsureForProject(LegacyConfigDirectory);
        FString UpgradedLegacyConfigText;
        bPassed &= Test.TestTrue(TEXT("Legacy generated OpenCode config remains readable"), FFileHelper::LoadFileToString(UpgradedLegacyConfigText, *LegacyConfigPath));
        bPassed &= Test.TestTrue(TEXT("Legacy generated OpenCode config is upgraded with marker"), UpgradedLegacyConfigText.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));

        const FString Redacted = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("Authorization: Bearer abc123\nX-MCP-Capability-Token: fake-capability-token\nsafe: value"));
        bPassed &= Test.TestFalse(TEXT("Redaction removes bearer token"), Redacted.Contains(TEXT("abc123")));
        bPassed &= Test.TestFalse(TEXT("Redaction removes capability token"), Redacted.Contains(TEXT("fake-capability-token")));
        bPassed &= Test.TestTrue(TEXT("Redaction keeps safe lines"), Redacted.Contains(TEXT("safe: value")));

        const FUnrealAgentEditorContextSnapshot Context = FUnrealAgentEditorContext::Capture(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Editor context envelope is produced"), Context.Envelope.Contains(TEXT("<unreal_editor_context")));
        bPassed &= Test.TestTrue(TEXT("Editor context has privacy guidance"), Context.Envelope.Contains(TEXT("Sensitive credential values")));
        bPassed &= Test.TestFalse(TEXT("Editor context does not expose fake secrets"), Context.Envelope.Contains(TEXT("fake-capability-token")));
        bPassed &= Test.TestFalse(TEXT("Editor context does not expose absolute project directory"), Context.Envelope.Contains(TestDirectory));
        bPassed &= Test.TestTrue(TEXT("Editor context redacts project directory"), Context.Envelope.Contains(TEXT("projectDir: [redacted project root]")));

        const FUnrealAgentValidationResult ValidationResult = FUnrealAgentValidationRunner::RunFastValidation(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Fast validation passes after Studio Kit generation"), ValidationResult.bPassed);
        bPassed &= Test.TestTrue(TEXT("Validation records evidence"), FPaths::FileExists(ValidationResult.EvidencePath));

        FString FirstEvidencePath;
        FString SecondEvidencePath;
        bPassed &= Test.TestTrue(TEXT("First same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &FirstEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Second same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &SecondEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Same-second evidence events use distinct paths"), !FirstEvidencePath.IsEmpty() && !SecondEvidencePath.IsEmpty() && FirstEvidencePath != SecondEvidencePath && FPaths::FileExists(FirstEvidencePath) && FPaths::FileExists(SecondEvidencePath));

        const FString MissingDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentMissingValidationTest")));
        IFileManager::Get().DeleteDirectory(*MissingDirectory, false, true);
        const FUnrealAgentValidationResult MissingValidationResult = FUnrealAgentValidationRunner::RunFastValidation(MissingDirectory);
        bPassed &= Test.TestFalse(TEXT("Validation fails for missing project directory"), MissingValidationResult.bPassed);
        bPassed &= Test.TestTrue(TEXT("Validation reports missing project directory"), !MissingValidationResult.Errors.IsEmpty() && MissingValidationResult.Errors[0].Contains(TEXT("Project directory does not exist")));
        bPassed &= Test.TestTrue(TEXT("Missing project validation does not write evidence"), MissingValidationResult.EvidencePath.IsEmpty() && !FPaths::DirectoryExists(FPaths::Combine(MissingDirectory, TEXT("Saved/UnrealAgent"))));

        const FString BrokenDecisionsDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenDecisionsTest")));
        IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
        FUnrealAgentEvidenceLedger::EnsureLedger(BrokenDecisionsDirectory);
        const FString BrokenDecisionsPath = FPaths::Combine(BrokenDecisionsDirectory, TEXT("Saved/UnrealAgent/decisions.md"));
        IFileManager::Get().Delete(*BrokenDecisionsPath);
        IFileManager::Get().MakeDirectory(*BrokenDecisionsPath, true);
        Test.AddExpectedErrorPlain(TEXT("UnrealAgentBrokenDecisionsTest"), EAutomationExpectedErrorFlags::Contains, 2);
        FString BrokenDecisionsEvidencePath = TEXT("stale-evidence-path");
        bPassed &= Test.TestFalse(TEXT("Evidence recording fails when decisions ledger cannot be appended"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenDecisionsDirectory, TEXT("broken-decisions"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenDecisionsEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Decision write failure clears stale success path"), BrokenDecisionsEvidencePath.IsEmpty());

        const FString BrokenStateDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenStateTest")));
        IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
        FUnrealAgentEvidenceLedger::EnsureLedger(BrokenStateDirectory);
        const FString BrokenStatePath = FPaths::Combine(BrokenStateDirectory, TEXT("Saved/UnrealAgent/state.json"));
        IFileManager::Get().Delete(*BrokenStatePath);
        IFileManager::Get().MakeDirectory(*BrokenStatePath, true);
        Test.AddExpectedErrorPlain(TEXT("UnrealAgentBrokenStateTest"), EAutomationExpectedErrorFlags::Contains, 2);
        FString BrokenStateEvidencePath = TEXT("stale-evidence-path");
        bPassed &= Test.TestFalse(TEXT("Evidence recording fails when state ledger cannot be written"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenStateDirectory, TEXT("broken-state"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenStateEvidencePath));
        bPassed &= Test.TestTrue(TEXT("State write failure clears stale success path"), BrokenStateEvidencePath.IsEmpty());

        const FString CustomDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitCustomTest")));
        IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
        const FString CustomAgentPath = FPaths::Combine(CustomDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        const FString CustomConfigPath = FPaths::Combine(CustomDirectory, TEXT(".opencode/opencode.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(CustomAgentPath), true);
        const FString CustomAgentText = TEXT("custom user-owned Unreal Agent prompt");
        const FString CustomConfigText = TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\n    \"read\": \"deny\"\n  }\n}\n");
        FFileHelper::SaveStringToFile(CustomAgentText, *CustomAgentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FFileHelper::SaveStringToFile(CustomConfigText, *CustomConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FUnrealAgentStudioKit::EnsureForProject(CustomDirectory);
        FString PreservedAgentText;
        FString PreservedConfigText;
        bPassed &= Test.TestTrue(TEXT("Custom prompt remains readable"), FFileHelper::LoadFileToString(PreservedAgentText, *CustomAgentPath));
        bPassed &= Test.TestEqual(TEXT("Custom unmarked prompt is preserved"), PreservedAgentText, CustomAgentText);
        bPassed &= Test.TestTrue(TEXT("Custom OpenCode config remains readable"), FFileHelper::LoadFileToString(PreservedConfigText, *CustomConfigPath));
        bPassed &= Test.TestEqual(TEXT("Custom unmarked OpenCode config is preserved"), PreservedConfigText, CustomConfigText);

        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
        return bPassed;
    }
}

#endif
