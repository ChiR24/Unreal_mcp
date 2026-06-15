#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/Context/UnrealAgentEditorContext.h"
#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitRedactionContextChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        const FString LegacyConfigDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitLegacyConfigTest")));
        IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
        const FString LegacyConfigPath =
            FPaths::Combine(LegacyConfigDirectory, TEXT(".opencode/opencode.json"));
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

        const FString Redacted = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("Authorization: Bearer abc123\nX-MCP-Capability-Token: |\nopaque:value==\nQUJDREVGR0g=\nsafe: value"));
        bPassed &= Test.TestFalse(TEXT("Redaction removes bearer token"), Redacted.Contains(TEXT("abc123")));
        bPassed &= Test.TestFalse(TEXT("Redaction removes capability token"), Redacted.Contains(TEXT("test-capability-token")));
        bPassed &= Test.TestFalse(TEXT("Redaction removes colon-bearing continuation"), Redacted.Contains(TEXT("opaque:value")));
        bPassed &= Test.TestFalse(TEXT("Redaction removes padded base64 continuation"), Redacted.Contains(TEXT("QUJDREVGR0g")));
        bPassed &= Test.TestTrue(TEXT("Redaction keeps safe lines"), Redacted.Contains(TEXT("safe: value")));
        FUnrealAgentRedactionState ChunkRedactionState;
        const FString RedactedChunkStart = FUnrealAgentStudioKit::RedactSensitiveText(
            TEXT("Authorization: Bearer chunk-secret-"),
            ChunkRedactionState);
        const FString RedactedChunkEnd = FUnrealAgentStudioKit::RedactSensitiveText(
            TEXT("suffix\n"),
            ChunkRedactionState);
        const FString VisibleAfterCompletedSecret =
            FUnrealAgentStudioKit::RedactSensitiveText(
                TEXT("ordinary assistant output\n"),
                ChunkRedactionState);
        bPassed &= Test.TestFalse(
            TEXT("Mid-value secret chunks never expose either value fragment"),
            RedactedChunkStart.Contains(TEXT("chunk-secret"))
                || RedactedChunkEnd.Contains(TEXT("suffix")));
        bPassed &= Test.TestTrue(
            TEXT("Incomplete sensitive chunks retain continuation redaction"),
            RedactedChunkStart.Contains(TEXT("[REDACTED]"))
                && RedactedChunkEnd.Contains(TEXT("[REDACTED]")));
        bPassed &= Test.TestTrue(
            TEXT("Completed sensitive lines clear continuation redaction"),
            VisibleAfterCompletedSecret.Contains(TEXT("ordinary assistant output")));
        const FString CamelCaseRedacted = FUnrealAgentStudioKit::RedactSensitiveText(
            TEXT("accessToken: camel-access-secret\nrefreshToken=camel-refresh-secret\n"));
        bPassed &= Test.TestFalse(TEXT("Camel-case access token is redacted"), CamelCaseRedacted.Contains(TEXT("camel-access-secret")));
        bPassed &= Test.TestFalse(TEXT("Camel-case refresh token is redacted"), CamelCaseRedacted.Contains(TEXT("camel-refresh-secret")));
        FUnrealAgentRedactionState CamelChunkState;
        const FString CamelChunkStart = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("access"), CamelChunkState);
        const FString CamelChunkEnd = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("Token: camel-chunk-secret\n"), CamelChunkState);
        bPassed &= Test.TestFalse(
            TEXT("Split camel-case token marker is redacted"),
            (CamelChunkStart + CamelChunkEnd).Contains(TEXT("camel-chunk-secret")));
        const FString PromptRedacted =
            FUnrealAgentStudioKit::RedactPromptSensitiveText(
                TEXT("stderr secret exit path\ncapability_token: prompt-secret\nAuthorization: Bearer auth-secret\ncurl -H 'X-MCP-Capability-Token: embedded-secret'"));
        bPassed &= Test.TestTrue(
            TEXT("Prompt redaction preserves harmless secret terminology"),
            PromptRedacted.Contains(TEXT("stderr secret exit path")));
        bPassed &= Test.TestFalse(
            TEXT("Prompt redaction removes structured capability tokens"),
            PromptRedacted.Contains(TEXT("prompt-secret")));
        bPassed &= Test.TestFalse(
            TEXT("Prompt redaction removes bearer credentials"),
            PromptRedacted.Contains(TEXT("auth-secret")));
        bPassed &= Test.TestFalse(
            TEXT("Prompt redaction removes embedded capability-token fields"),
            PromptRedacted.Contains(TEXT("embedded-secret")));
        FUnrealAgentRedactionState EmptyValueState;
        const FString EmptyValueHeader = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("X-MCP-Capability-Token:\n"), EmptyValueState);
        const FString EmptyValueContinuation = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("opaque:value==\nQUJDREVGR0g=\n"), EmptyValueState);
        const FString EmptyValueBoundary = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("safe: diagnostic\n"), EmptyValueState);
        bPassed &= Test.TestFalse(
            TEXT("Empty sensitive values redact multiline continuation content"),
            (EmptyValueHeader + EmptyValueContinuation).Contains(TEXT("opaque:value"))
                || EmptyValueContinuation.Contains(TEXT("QUJDREVGR0g")));
        bPassed &= Test.TestTrue(TEXT("Trusted boundaries end empty-value multiline redaction"), EmptyValueBoundary.Contains(TEXT("safe: diagnostic")));
        const FString SplitMarkerSecret = TEXT("X-MCP-Capability-Token: boundary-secret\n");
        for (int32 SplitIndex = 1; SplitIndex < SplitMarkerSecret.Len(); ++SplitIndex)
        {
            FUnrealAgentRedactionState BoundaryState;
            const FString FirstPart =
                FUnrealAgentStudioKit::RedactSensitiveText(SplitMarkerSecret.Left(SplitIndex), BoundaryState);
            const FString SecondPart =
                FUnrealAgentStudioKit::RedactSensitiveText(SplitMarkerSecret.Mid(SplitIndex), BoundaryState);
            bPassed &= Test.TestFalse(
                FString::Printf(TEXT("Sensitive marker split at character %d does not expose its value"), SplitIndex),
                (FirstPart + SecondPart).Contains(TEXT("boundary-secret")));
        }

        const FUnrealAgentEditorContextSnapshot Context =
            FUnrealAgentEditorContext::Capture(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Editor context envelope is produced"), Context.Envelope.Contains(TEXT("<unreal_editor_context")));
        bPassed &= Test.TestTrue(TEXT("Editor context has privacy guidance"), Context.Envelope.Contains(TEXT("Sensitive credential values")));
        bPassed &= Test.TestFalse(TEXT("Editor context does not expose test secrets"), Context.Envelope.Contains(TEXT("test-capability-token")));
        bPassed &= Test.TestFalse(TEXT("Editor context does not expose absolute project directory"), Context.Envelope.Contains(TestDirectory));
        bPassed &= Test.TestTrue(TEXT("Editor context redacts project directory"), Context.Envelope.Contains(TEXT("projectDir: [redacted project root]")));
        bPassed &= Test.TestTrue(TEXT("Standalone editor context does not assume MCP configuration"), Context.Envelope.Contains(TEXT("unrealMcpConfiguredForSession: false")));
        bPassed &= Test.TestTrue(TEXT("Editor context requires MCP tool-response evidence"), Context.Envelope.Contains(TEXT("availability still requires a successful tool response")));
        bPassed &= Test.TestTrue(TEXT("Editor context includes Unreal production preflight"), Context.Envelope.Contains(TEXT("mcpPreflight:")) && Context.Envelope.Contains(TEXT("validationRoute:")));

        IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
        return bPassed;
    }
}

#endif
