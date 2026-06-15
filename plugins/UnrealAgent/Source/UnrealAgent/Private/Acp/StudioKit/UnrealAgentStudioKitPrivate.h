#pragma once

#include "Acp/StudioKit/UnrealAgentStudioKit.h"

namespace UnrealAgentStudioKit
{
    constexpr const TCHAR* StudioKitVersionMarker = TEXT("unreal_agent_studio_kit_version: 1");
    constexpr const TCHAR* PromptVersionMarker = TEXT("unreal_agent_prompt_version: 2");

    struct FStudioKitTemplateFile
    {
        FString RelativePath;
        FString Content;
        bool bOverwriteLegacyPrompt = false;
    };

    enum class EStudioKitAtomicWriteFailurePoint : uint8
    {
        CommitDirectorySync,
        BackupRemoval,
        Rollback
    };

    enum class ETemplateWriteResult : uint8
    {
        Written,
        OwnershipChanged,
        Failed
    };

    void AddTemplate(TArray<FStudioKitTemplateFile>& Templates, const FString& RelativePath, FString Content, bool bOverwriteLegacyPrompt = false);

    FString MakeSkillMarkdown(const FString& Name, const FString& Description, const FString& Body);
    FString MakeToolPlaybookSkill();
    FString MakeBootstrapSkill();
    FString MakeMcpRouteCardSkill();
    FString MakePieSieSafetySkill();
    FString MakeEditorControlDisciplineSkill();
    FString MakeContentBrowserAssetDisciplineSkill();
    FString MakeBlueprintCompileDisciplineSkill();
    FString MakeLevelActorDisciplineSkill();
    FString MakeWorldBuildingDisciplineSkill();
    FString MakeGameplayInputDisciplineSkill();
    FString MakeAnimationPhysicsDisciplineSkill();
    FString MakeVfxMaterialDisciplineSkill();
    FString MakeAudioDisciplineSkill();
    FString MakeCinematicSequenceDisciplineSkill();
    FString MakeNetworkingGasDisciplineSkill();
    FString MakeCharacterSystemsDisciplineSkill();
    FString MakeUiHudDisciplineSkill();
    FString MakeAiNavigationDisciplineSkill();
    FString MakeDataSaveAccessibilityDisciplineSkill();
    FString MakeSourceControlCollaborationDisciplineSkill();
    FString MakePerformanceInsightsDisciplineSkill();
    FString MakeSystemProjectDisciplineSkill();
    FString MakeProjectSetupTemplateDisciplineSkill();
    FString MakeDiagnosticsCrashRecoveryDisciplineSkill();
    FString MakeGuardrailsPlugin();
    FString MakeGuardrailsCoreSection();
    FString MakeGuardrailsPreflightStateSection();
    FString MakeGuardrailsMutationAdmissionSection();
    FString MakeGuardrailsCommandSafetySection();
    FString MakeGuardrailsLocalToolSection();
    FString MakeGuardrailsLocalPathSection();
    FString MakeGuardrailsLocalShellSection();
    FString MakeGuardrailsLocalMutationSection();
    FString MakeOfficialGettingStartedSkill();
    FString MakeEditorOrientationSkill();
    FString MakeFirstPlayableLoopSkill();
    FString MakePrototypeSkill();
    FString MakeValidationSkill();
    FString MakeReleaseSkill();
    FString MakeDebugFixSkill();
    FString MakeCppUObjectLifecycleSkill();
    FString MakeCommandMarkdown(const FString& Description, const FString& Body);
    void AppendAgentTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendSkillTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendCommandTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendConfigTemplates(TArray<FStudioKitTemplateFile>& Templates);
    TArray<FStudioKitTemplateFile> BuildTemplateFiles();

    FString MakeLegacyOpenCodeConfig();
    bool LooksLikeLegacyOpenCodeConfig(const FString& ExistingText);
    bool LooksLikeLegacyManagedPrompt(const FString& ExistingText);
    bool ShouldFailStudioKitAtomicWriteForTest(
        const FString& Path,
        EStudioKitAtomicWriteFailurePoint FailurePoint);
    bool IsTemplatePathFreeOfLinks(
        const FString& ProjectDirectory,
        const FString& Path);
    ETemplateWriteResult WriteStudioKitTemplateUnix(
        const FString& ProjectDirectory,
        const FStudioKitTemplateFile& TemplateFile,
        const FString& Path,
        const FString* ExpectedExistingText);
    ETemplateWriteResult WriteStudioKitTemplateWindows(
        const FString& ProjectDirectory,
        const FStudioKitTemplateFile& TemplateFile,
        const FString& Path,
        const FString* ExpectedExistingText);
    bool WriteTemplateFile(const FString& ProjectDirectory, const FStudioKitTemplateFile& TemplateFile, FUnrealAgentStudioKitResult& Result);

#if WITH_DEV_AUTOMATION_TESTS
    extern TFunction<void(const FString&)> GBeforeStudioKitTemplateWriteForTest;
    extern TFunction<void(const FString&)> GBeforeStudioKitTemplateAtomicWriteForTest;
    extern TFunction<bool(
        const FString&,
        EStudioKitAtomicWriteFailurePoint)>
        GShouldFailStudioKitAtomicWriteForTest;
#endif
}
