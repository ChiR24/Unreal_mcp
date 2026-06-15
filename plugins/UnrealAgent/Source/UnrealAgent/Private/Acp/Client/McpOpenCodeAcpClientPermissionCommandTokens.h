#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionCommandTokens
{
    void TokenizeQuotedCommand(
        const FString& Command,
        TArray<FString>& OutTokens);
    void TokenizeQuotedCommandPreservingSyntax(
        const FString& Command,
        TArray<FString>& OutTokens);
}
