#pragma once

#include "Engine/DataAsset.h"

#include "McpGenericDataAsset.generated.h"

// What the inventory actions save an item, a category, a recipe or a loot table as: UDataAsset and
// UPrimaryDataAsset are abstract. Saved assets name it /Script/EngineWire.McpGenericDataAsset.
UCLASS(BlueprintType)
class ENGINEWIRE_API UMcpGenericDataAsset : public UDataAsset
{
  GENERATED_BODY()

public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  FString ItemName;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  FString Description;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
  TMap<FString, FString> Properties;
};
