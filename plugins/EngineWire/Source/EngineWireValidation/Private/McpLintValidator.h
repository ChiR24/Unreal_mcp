// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"

#include "McpLintValidator.generated.h"

/**
 * lint_blueprint's rules on every Blueprint Data Validation checks: a While Loop that never ends fails the asset, every
 * other finding (unreachable nodes, heavy work on Tick, reads that are always or possibly None, one random roll read as
 * several) warns. 5.4 replaced the validator's overrides with ones taking the asset data and a context; each engine gets
 * the pair it calls (the older pair still runs on 5.4+, but through a path that fires an ensure).
 */
UCLASS()
class UMcpLintValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
#else
	virtual bool CanValidateAsset_Implementation(UObject* InAsset) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors) override;
#endif
};
