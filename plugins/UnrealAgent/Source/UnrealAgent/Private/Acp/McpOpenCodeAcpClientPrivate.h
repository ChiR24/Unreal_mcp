#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "Templates/SharedPointer.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

namespace UnrealAgent::OpenCodeAcp
{
    inline constexpr double ClientLifecycleRequestTimeoutSeconds = 120.0;
    inline constexpr double ClientConfigRequestTimeoutSeconds = 30.0;
    inline constexpr double ClientProcessShutdownWaitSeconds = 2.0;
    inline constexpr int32 SupportedAcpProtocolVersion = 1;
    inline constexpr int32 MaxOutputBufferBytes = 1024 * 1024;
    inline constexpr int32 MaxRecentErrorOutputChars = 4096;
    inline constexpr int32 MaxPermissionDescriptionChars = 4096;
    inline constexpr int32 MaxToolActivityDetailChars = 2048;
    inline constexpr const TCHAR* UnrealAgentId = TEXT("unreal-agent");
    inline constexpr const TCHAR* UnrealMcpServerName = TEXT("unreal-engine");
    inline constexpr const TCHAR* AutomationBridgeSettingsSection = TEXT("/Script/McpAutomationBridge.McpAutomationBridgeSettings");

    TSharedPtr<FJsonObject> MakeObject();
    TSharedPtr<FJsonValue> CloneJsonId(const TSharedPtr<FJsonObject>& Message);
    FString GetStringFieldOrEmpty(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);
    FString GetConfigOptionId(const TSharedPtr<FJsonObject>& Object);
    FString GetConfigOptionValue(const TSharedPtr<FJsonObject>& Object);
    bool IsAgentConfigOption(const TSharedPtr<FJsonObject>& Option);
    bool IsModelConfigOption(const TSharedPtr<FJsonObject>& Option);
    bool IsThinkingConfigOption(const TSharedPtr<FJsonObject>& Option);
    FString NormalizeMcpHostForUrl(FString Host);
    FString GetModelProviderField(const TSharedPtr<FJsonObject>& Object);
    int32 GetModelContextWindowTokens(const TSharedPtr<FJsonObject>& Object);

    bool IsSafeProcessArgumentValue(const FString& Value);
    bool IsAbsoluteExistingExecutable(const FString& Path);
    FString NormalizeExecutablePath(const FString& Path);
    void TerminateAndCloseProcess(FProcHandle& ProcessHandle);

    FString TruncateForDisplay(const FString& Text, int32 MaxChars);
    FString GetJsonScalarDisplayText(const TSharedPtr<FJsonValue>& Value);
    FString ExtractToolActivityDetail(const TSharedPtr<FJsonObject>& Update);
    bool IsFinalToolActivityStatus(const FString& Status);

    class FScopedOpenCodeSignalLaunchGuard
    {
    public:
        FScopedOpenCodeSignalLaunchGuard();
        ~FScopedOpenCodeSignalLaunchGuard();

    private:
#if PLATFORM_LINUX && defined(SIGPWR)
        struct sigaction PreviousSigPwrAction;
        bool bRestoreSigPwr = false;
#endif
    };
}
