// McpNativeGatewayDescribeOverview.cpp — bare describe overview and schema hygiene.
//
// describe {} used to answer UNKNOWN_TOOL although nothing else enumerates the
// canonical parents (dogfood #1); and every capability inputSchema leaked the
// envelope key `action` as a required parameter (dogfood #78/#110).
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

TSharedPtr<FJsonObject> McpGatewayDescribeToolOverview(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store)
{
	const TArray<FString> Parents = Store.GetParents();
	const int32 Limit = FMath::Clamp(Input.Limit, 1, 50);
	const int32 Offset = FMath::Clamp(Input.Offset, 0, Parents.Num());
	TArray<TSharedPtr<FJsonValue>> Tools;
	for (int32 Index = Offset; Index < Parents.Num() && Tools.Num() < Limit; ++Index)
	{
		TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
		Tool->SetStringField(TEXT("tool"), Parents[Index]);
		Tool->SetNumberField(TEXT("actionCount"), Store.GetRecordsForParent(Parents[Index]).Num());
		Tool->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), Parents[Index], FString(), FString()));
		Tools.Add(MakeShared<FJsonValueObject>(Tool));
	}
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("scope"), TEXT("catalog"));
	Out->SetStringField(TEXT("catalogRevision"), Store.GetCatalogRevision());
	Out->SetArrayField(TEXT("tools"), Tools);
	Out->SetNumberField(TEXT("toolCount"), Parents.Num());
	Out->SetNumberField(TEXT("toolOffset"), Offset);
	Out->SetNumberField(TEXT("toolLimit"), Limit);
	Out->SetBoolField(TEXT("toolHasMore"), Offset + Tools.Num() < Parents.Num());
	Out->SetStringField(TEXT("message"),
		TEXT("Canonical parent tools. Pass tool to list its actions, tool + action for one exact contract, or query to search."));
	return Out;
}

TSharedPtr<FJsonObject> McpStripActionFromInputSchema(const TSharedPtr<FJsonObject>& Schema)
{
	if (!Schema.IsValid())
	{
		return Schema;
	}
	TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
	Copy->Values = Schema->Values;
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Copy->TryGetObjectField(TEXT("properties"), Properties) && Properties && (*Properties)->HasField(TEXT("action")))
	{
		TSharedPtr<FJsonObject> PropertiesCopy = MakeShared<FJsonObject>();
		PropertiesCopy->Values = (*Properties)->Values;
		PropertiesCopy->RemoveField(TEXT("action"));
		Copy->SetObjectField(TEXT("properties"), PropertiesCopy);
	}
	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if (Copy->TryGetArrayField(TEXT("required"), Required) && Required)
	{
		TArray<TSharedPtr<FJsonValue>> Kept;
		for (const TSharedPtr<FJsonValue>& Value : *Required)
		{
			if (Value.IsValid() && Value->Type == EJson::String && Value->AsString() == TEXT("action"))
			{
				continue;
			}
			Kept.Add(Value);
		}
		Copy->SetArrayField(TEXT("required"), Kept);
	}
	return Copy;
}
