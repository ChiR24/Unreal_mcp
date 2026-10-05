// Describe and search replies as a client reads them; see McpJsonRpcReplyCompaction.h.

#include "MCP/Protocol/McpJsonRpcReplyFields.h"

namespace McpJsonRpcReplyFields
{
// Output properties every capability declares in the same words; the reply itself shows them.
const TCHAR* const StockOutputProperties[][2] = {
	{TEXT("success"), TEXT("Whether the action succeeded.")},
	{TEXT("message"), TEXT("Human-readable result message.")},
	{TEXT("details"), TEXT("Additional handler result fields not named by the contract.")}};

const TCHAR* const ToolEchoFields[] = {TEXT("parent"), TEXT("parentTool")};

TSharedPtr<FJsonValue> ListedDescription(const FJsonArray& Parameters, const FString& Name)
{
	for (const TSharedPtr<FJsonValue>& Parameter : Parameters)
	{
		const TSharedPtr<FJsonObject> Object = AsObject(Parameter);
		FString Listed;
		if (Object && Object->TryGetStringField(TEXT("name"), Listed) && Listed.Equals(Name, ESearchCase::CaseSensitive))
		{
			return Object->TryGetField(TEXT("description"));
		}
	}
	return nullptr;
}

// parameters[] already carries each top-level description; nested ones exist only here.
TSharedPtr<FJsonObject> CompactInputSchema(const TSharedPtr<FJsonObject>& Schema, const FJsonArray* Parameters)
{
	TSharedPtr<FJsonObject> Out = Without(Schema, TEXT("$schema"));
	const TSharedPtr<FJsonObject> Properties = ObjectField(Out, TEXT("properties"));
	if (!Properties || !Parameters) return Out;
	TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
	for (const auto& Property : Properties->Values)
	{
		const FString Name(Property.Key.Len(), *Property.Key);  // 5.8 keys are not FString
		const TSharedPtr<FJsonObject> Object = AsObject(Property.Value);
		if (Object && Same(Object->TryGetField(TEXT("description")), ListedDescription(*Parameters, Name)))
		{
			Kept->SetObjectField(Name, Without(Object, TEXT("description")));
		}
		else
		{
			Kept->SetField(Name, Property.Value);
		}
	}
	Out->SetObjectField(TEXT("properties"), Kept);
	return Out;
}

TSharedPtr<FJsonObject> CompactOutputSchema(const TSharedPtr<FJsonObject>& Schema)
{
	TSharedPtr<FJsonObject> Out = Without(Schema, TEXT("$schema"));
	TArray<FString> Removed;
	if (const TSharedPtr<FJsonObject> Properties = ObjectField(Out, TEXT("properties")))
	{
		TSharedPtr<FJsonObject> Kept = Copy(Properties);
		for (const auto& Stock : StockOutputProperties)
		{
			FString Description;
			const TSharedPtr<FJsonObject> Property = ObjectField(Kept, Stock[0]);
			if (Property && Property->TryGetStringField(TEXT("description"), Description) &&
				Description.Equals(Stock[1], ESearchCase::CaseSensitive))
			{
				Kept->RemoveField(Stock[0]);
				Removed.Add(Stock[0]);
			}
		}
		if (Kept->Values.Num() == 0) return nullptr;
		Out->SetObjectField(TEXT("properties"), Kept);
	}
	if (const FJsonArray* Required = ArrayField(Out, TEXT("required")))
	{
		FJsonArray Kept;
		for (const TSharedPtr<FJsonValue>& Name : *Required)
		{
			const bool bRemoved = Name.IsValid() && Name->Type == EJson::String && Removed.ContainsByPredicate(
				[&Name](const FString& Field) { return Field.Equals(Name->AsString(), ESearchCase::CaseSensitive); });
			if (!bRemoved) Kept.Add(Name);
		}
		if (Kept.Num() > 0) Out->SetArrayField(TEXT("required"), Kept);
		else Out->RemoveField(TEXT("required"));
	}
	return Out;
}

TSharedPtr<FJsonObject> CompactDescribe(const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = Copy(Reply);
	Out->RemoveField(TEXT("hashes"));
	Out->RemoveField(TEXT("exampleCount"));
	FString Scope;
	if (!Out->TryGetStringField(TEXT("scope"), Scope) || !Scope.Equals(TEXT("capability"), ESearchCase::CaseSensitive)) return Out;
	if (IsTrue(Out, TEXT("success"))) Out->RemoveField(TEXT("message"));
	for (const TCHAR* Field : ToolEchoFields)
	{
		if (SameField(Out, Field, Out, TEXT("tool"))) Out->RemoveField(Field);
	}
	const TSharedPtr<FJsonObject> Behavior = ObjectField(Out, TEXT("behavior"));
	if (SameField(Behavior, TEXT("effect"), Out, TEXT("effect"))) Out->SetObjectField(TEXT("behavior"), Without(Behavior, TEXT("effect")));
	if (const FJsonArray* Examples = ArrayField(Out, TEXT("examples")))
	{
		FJsonArray Kept;
		for (const TSharedPtr<FJsonValue>& Example : *Examples)
		{
			const TSharedPtr<FJsonObject> Object = AsObject(Example);
			if (SameField(Object, TEXT("title"), Out, TEXT("summary"))) Kept.Add(MakeShared<FJsonValueObject>(Without(Object, TEXT("title"))));
			else Kept.Add(Example);
		}
		Out->SetArrayField(TEXT("examples"), Kept);
	}
	if (const TSharedPtr<FJsonObject> Input = ObjectField(Out, TEXT("inputSchema")))
	{
		Out->SetObjectField(TEXT("inputSchema"), CompactInputSchema(Input, ArrayField(Out, TEXT("parameters"))));
	}
	if (const TSharedPtr<FJsonObject> Output = ObjectField(Out, TEXT("outputSchema")))
	{
		const TSharedPtr<FJsonObject> Shown = CompactOutputSchema(Output);
		if (Shown) Out->SetObjectField(TEXT("outputSchema"), Shown);
		else Out->RemoveField(TEXT("outputSchema"));
	}
	return Out;
}

TSharedPtr<FJsonObject> CompactSearch(const TSharedPtr<FJsonObject>& Reply)
{
	TSharedPtr<FJsonObject> Out = Without(Reply, TEXT("query"));
	if (IsTrue(Out, TEXT("success"))) Out->RemoveField(TEXT("message"));
	const FJsonArray* Rows = ArrayField(Out, TEXT("results"));
	if (!Rows) return Out;
	FJsonArray Kept;
	for (const TSharedPtr<FJsonValue>& Row : *Rows)
	{
		const TSharedPtr<FJsonObject> Object = AsObject(Row);
		if (!Object)
		{
			Kept.Add(Row);
			continue;
		}
		TSharedPtr<FJsonObject> Trimmed = Without(Without(Object, TEXT("matchReasons")), TEXT("score"));
		const TSharedPtr<FJsonObject> Next = ObjectField(Object, TEXT("nextCall"));
		for (const TCHAR* Field : ToolEchoFields)
		{
			if (SameField(Trimmed, Field, Next, TEXT("tool"))) Trimmed->RemoveField(Field);
		}
		Kept.Add(MakeShared<FJsonValueObject>(Trimmed));
	}
	Out->SetArrayField(TEXT("results"), Kept);
	return Out;
}
}
