#include "Domains/Environment/Runtime/McpAutomationBridge_EnvironmentAssetValidation.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Misc/PackageName.h"

namespace McpEnvironmentHandlers {

bool McpFailEnvironmentAction(FString &OutMessage, FString &OutErrorCode, const FString &Message, const TCHAR *ErrorCode)
{
    OutMessage = Message;
    OutErrorCode = ErrorCode;
    return false;
}

bool McpBuildValidatedEnvironmentAssetPath(const FString &RequestedPath, const FString &AssetName,
                                           const TCHAR *AssetLabel, FString &OutPackagePath,
                                           FString &OutMessage, FString &OutErrorCode)
{
    const FString SafePath = SanitizeProjectRelativePath(RequestedPath);
    if (SafePath.IsEmpty())
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Invalid %s path: %s"), AssetLabel, *RequestedPath), TEXT("SECURITY_VIOLATION"));
    }

    const FString TrimmedName = AssetName.TrimStartAndEnd();
    if (TrimmedName.IsEmpty() || TrimmedName != AssetName)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Invalid %s name '%s': leading or trailing whitespace is not allowed"), AssetLabel, *AssetName), TEXT("INVALID_ARGUMENT"));
    }
    if (AssetName.Contains(TEXT("..")) || AssetName.Contains(TEXT("/")) || AssetName.Contains(TEXT("\\")))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Invalid %s name '%s': path separators and traversal sequences are not allowed"), AssetLabel, *AssetName), TEXT("INVALID_ARGUMENT"));
    }

    FText ObjectNameReason;
    if (!FName(*AssetName).IsValidObjectName(ObjectNameReason))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Invalid %s name '%s': %s"), AssetLabel, *AssetName, *ObjectNameReason.ToString()), TEXT("INVALID_ARGUMENT"));
    }

    OutPackagePath = SafePath;
    if (!FPaths::GetBaseFilename(OutPackagePath).Equals(AssetName, ESearchCase::IgnoreCase))
    {
        OutPackagePath = SafePath / AssetName;
    }
    FText PackageValidationReason;
    if (!FPackageName::IsValidLongPackageName(OutPackagePath, true, &PackageValidationReason))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Invalid %s package path '%s': %s"), AssetLabel, *OutPackagePath, *PackageValidationReason.ToString()), TEXT("INVALID_ARGUMENT"));
    }
    return true;
}

}
