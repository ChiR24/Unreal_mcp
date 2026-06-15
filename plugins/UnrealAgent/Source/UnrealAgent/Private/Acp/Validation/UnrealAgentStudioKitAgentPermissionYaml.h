#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::Validation::Yaml
{
    FString UnquoteScalar(FString Value);
    int32 CountIndent(const FString& Line);
    bool SplitField(const FString& Text, FString& OutKey, FString& OutValue);
    bool HasUnsupportedKeySyntax(const FString& Text);
    bool HasUnsupportedReferenceSyntax(const FString& Text);
    bool ContainsAllowValue(const FString& Text);
    bool HasUnsupportedScalarSyntax(const FString& Text);
    void SplitFlowFields(const FString& Text, TArray<FString>& OutFields);
}
