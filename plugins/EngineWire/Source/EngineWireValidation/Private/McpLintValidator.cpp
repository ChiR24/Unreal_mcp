// Copyright (c) 2025 EngineWire Contributors

#include "McpLintValidator.h"

#include "Engine/Blueprint.h"
#include "McpBlueprintLint.h"

namespace
{
FText McpLintMessage(const FMcpLintIssue& Issue)
{
	return FText::FromString(FString::Printf(TEXT("%s%s: %s (graph %s, node %s)"), MCP_LINT_MESSAGE_PREFIX, *Issue.Rule,
	                                         *Issue.Detail, *Issue.Graph, *Issue.Node));
}
} // namespace

EDataValidationResult UMcpLintValidator::ValidateWithLint(UObject* InAsset, TFunctionRef<void(const FText&)> Fail)
{
	bool bFailed = false;
	for (const FMcpLintIssue& Issue : McpLintBlueprint(Cast<UBlueprint>(InAsset)))
	{
		if (Issue.Severity == TEXT("error"))
		{
			Fail(McpLintMessage(Issue));
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

#if MCP_HAS_VALIDATOR_ASSET_DATA_SIGNATURE
bool UMcpLintValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return Cast<UBlueprint>(InObject) != nullptr;
}

EDataValidationResult UMcpLintValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	return ValidateWithLint(InAsset, [this, InAsset](const FText& Message) { AssetFails(InAsset, Message); });
}
#else
bool UMcpLintValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return Cast<UBlueprint>(InAsset) != nullptr;
}

EDataValidationResult UMcpLintValidator::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	return ValidateWithLint(InAsset, [this, InAsset, &ValidationErrors](const FText& Message) { AssetFails(InAsset, Message, ValidationErrors); });
}
#endif
