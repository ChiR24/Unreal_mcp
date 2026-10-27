#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

// Sets a console variable as the console does and reads it back. IConsoleVariable::Set drops a write with only a log
// line when the variable is read-only, or when a source of higher priority holds it (a value typed in the console
// beats one set by code), so a write that was only asked for read as applied. Empty when the value took; otherwise
// why it did not.
// Priority defaults to the console's, the highest there is, so this and console_command replace each other, last one
// wins; a project setting passes ECVF_SetByProjectSetting, which a value set from the console or by code outranks.
inline FString McpSetConsoleVariable(IConsoleVariable& Variable, const FString& Value,
                                     EConsoleVariableFlags Priority = ECVF_SetByConsole)
{
    if (Variable.TestFlags(ECVF_ReadOnly))
    {
        return TEXT("read-only: fixed at startup, set it in the project's config");
    }
    Variable.Set(*Value, Priority);
    // Read back: the variable's own change callback can still clamp or reset the value.
    const float Wanted = FCString::Atof(*Value);
    const bool bTook = Value.IsNumeric()
        ? FMath::IsNearlyEqual(Variable.GetFloat(), Wanted, FMath::Max(1e-4f, FMath::Abs(Wanted) * 1e-5f))
        : Variable.GetString().Equals(Value, ESearchCase::IgnoreCase);
    return bTook ? FString() : FString::Printf(TEXT("stays %s: held by a higher-priority source"), *Variable.GetString());
}
