// Copyright (c) 2024 EngineWire Contributors

#include "McpLintValidator.h"

#include "Engine/Blueprint.h"
#include "McpBlueprintLint.h"

namespace
{
FText McpLintMessage(const FMcpLintIssue& Issue)
{
	return FText::FromString(FString::Printf(TEXT("EngineWire lint %s: %s (graph %s, node %s)"), *Issue.Rule, *Issue.Detail,
	                                         *Issue.Graph, *Issue.Node));
}
} // namespace

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
bool UMcpLintValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return Cast<UBlueprint>(InObject) != nullptr;
}

EDataValidationResult UMcpLintValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	bool bFailed = false;
	for (const FMcpLintIssue& Issue : McpLintBlueprint(Cast<UBlueprint>(InAsset)))
	{
		if (Issue.Severity == TEXT("error"))
		{
			AssetFails(InAsset, McpLintMessage(Issue));
			bFailed = true;
		}
		else
		{
			AssetWarning(InAsset, McpLintMessage(Issue));
		}
	}
	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return bFailed ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#else
bool UMcpLintValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return Cast<UBlueprint>(InAsset) != nullptr;
}

EDataValidationResult UMcpLintValidator::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	bool bFailed = false;
	for (const FMcpLintIssue& Issue : McpLintBlueprint(Cast<UBlueprint>(InAsset)))
	{
		if (Issue.Severity == TEXT("error"))
		{
			AssetFails(InAsset, McpLintMessage(Issue), ValidationErrors);
			bFailed = true;
		}
		else
		{
			AssetWarning(InAsset, McpLintMessage(Issue));
		}
	}
	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return bFailed ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
