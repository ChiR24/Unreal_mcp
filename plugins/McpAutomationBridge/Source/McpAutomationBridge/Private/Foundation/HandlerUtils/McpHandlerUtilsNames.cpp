#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
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
    return SanitizeAssetName(Input);
}

FString MakeUniqueAssetName(const FString& BaseName, const FString& PackagePath)
{
#if WITH_EDITOR
    FString TestName = ToSafeAssetName(BaseName);
    FString TestPath = PackagePath / TestName;

    if (!UEditorAssetLibrary::DoesAssetExist(TestPath))
    {
        return TestName;
    }

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
