#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

namespace McpHandlerUtils
{

FString ToSafeAssetName(const FString& Input)
{
    if (Input.IsEmpty())
    {
        return TEXT("Asset");
    }

    FString Sanitized = Input.TrimStartAndEnd();

    // Replace SQL injection patterns
    Sanitized = Sanitized.Replace(TEXT(";"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("'"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("\""), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("--"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("`"), TEXT("_"));

    // Replace invalid characters for Unreal asset names
    const TArray<TCHAR> InvalidChars = {
        TEXT('@'), TEXT('#'), TEXT('%'), TEXT('$'), TEXT('&'), TEXT('*'),
        TEXT('('), TEXT(')'), TEXT('+'), TEXT('='), TEXT('['), TEXT(']'),
        TEXT('{'), TEXT('}'), TEXT('<'), TEXT('>'), TEXT('?'), TEXT('|'),
        TEXT('\\'), TEXT(':'), TEXT('~'), TEXT('!'), TEXT(' ')
    };

    for (TCHAR C : InvalidChars)
    {
        TCHAR CharStr[2] = { C, TEXT('\0') };
        Sanitized = Sanitized.Replace(CharStr, TEXT("_"));
    }

    // Remove consecutive underscores
    while (Sanitized.Contains(TEXT("__")))
    {
        Sanitized = Sanitized.Replace(TEXT("__"), TEXT("_"));
    }

    // Remove leading/trailing underscores
    while (Sanitized.StartsWith(TEXT("_")))
    {
        Sanitized.RemoveAt(0);
    }
    while (Sanitized.EndsWith(TEXT("_")))
    {
        Sanitized.RemoveAt(Sanitized.Len() - 1);
    }

    // If empty after sanitization, use default
    if (Sanitized.IsEmpty())
    {
        return TEXT("Asset");
    }

    // Ensure name starts with letter or underscore
    if (!FChar::IsAlpha(Sanitized[0]) && Sanitized[0] != TEXT('_'))
    {
        Sanitized = TEXT("Asset_") + Sanitized;
    }

    // Truncate to reasonable length
    if (Sanitized.Len() > 64)
    {
        Sanitized = Sanitized.Left(64);
    }

    return Sanitized;
}

FString MakeUniqueAssetName(const FString& BaseName, const FString& PackagePath)
{
#if WITH_EDITOR
    FString TestName = ToSafeAssetName(BaseName);
    FString TestPath = PackagePath / TestName;

    // Check if the name is already unique
    if (!UEditorAssetLibrary::DoesAssetExist(TestPath))
    {
        return TestName;
    }

    // Append number suffix until we find a unique name
    int32 Suffix = 1;
    while (Suffix < 10000) // Safety limit
    {
        FString Candidate = FString::Printf(TEXT("%s_%d"), *TestName, Suffix);
        TestPath = PackagePath / Candidate;

        if (!UEditorAssetLibrary::DoesAssetExist(TestPath))
        {
            return Candidate;
        }
        Suffix++;
    }
#endif

    // Fallback
    return FString::Printf(TEXT("%s_%d"), *ToSafeAssetName(BaseName), FMath::Rand());
}
}
