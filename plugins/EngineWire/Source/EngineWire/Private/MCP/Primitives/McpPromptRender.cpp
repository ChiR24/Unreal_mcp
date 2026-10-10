#include "MCP/Primitives/McpPromptRender.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "Dom/JsonObject.h"

namespace
{
	const TCHAR* Disclaimer =
		TEXT("Guidance only. Nothing here runs on its own, no conversation state is kept, and you decide ")
		TEXT("whether to run each call. Discover exact parameters with the gateway `describe` operation, ")
		TEXT("then run one `execute` call at a time and review each receipt yourself.");

	const FMcpWorkflowPrompt* FindPrompt(const FString& Name)
	{
		for (const FMcpWorkflowPrompt& Prompt : McpWorkflowPrompts())
		{
			if (Prompt.Id == Name)
			{
				return &Prompt;
			}
		}
		return nullptr;
	}

	FString RenderBody(const FMcpWorkflowPrompt& Prompt, const TArray<TPair<FString, FString>>& Inputs)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("# %s  (prompt %s)"), *Prompt.Title, *Prompt.Id));
		Lines.Add(TEXT(""));
		Lines.Add(Disclaimer);
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Your inputs:"));
		if (Inputs.Num() == 0)
		{
			Lines.Add(TEXT("- (none provided)"));
		}
		else
		{
			for (const TPair<FString, FString>& Input : Inputs)
			{
				Lines.Add(FString::Printf(TEXT("- %s: %s"), *Input.Key, *Input.Value));
			}
		}
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Steps:"));
		for (int32 Index = 0; Index < Prompt.Steps.Num(); ++Index)
		{
			const FMcpPromptStep& Step = Prompt.Steps[Index];
			Lines.Add(FString::Printf(TEXT("%d. %s"), Index + 1, *Step.Summary));
			Lines.Add(FString::Printf(TEXT("   describe: unreal { \"operation\": \"describe\", \"tool\": \"%s\", \"action\": \"%s\" }"), *Step.ParentTool, *Step.Action));
			Lines.Add(FString::Printf(TEXT("   execute:  unreal { \"operation\": \"execute\", \"capability\": \"%s\", \"params\": { } }"), *Step.CapabilityId));
			if (!Step.ResourceUri.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("   read:     %s"), *Step.ResourceUri));
			}
			Lines.Add(FString::Printf(TEXT("   safety:   %s"), *Step.Safety));
		}
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Finish: re-read the relevant resource and confirm the outcome before moving on."));
		Lines.Add(TEXT("Nothing above is executed for you; run each call yourself."));
		return FString::Join(Lines, TEXT("\n"));
	}
}  // namespace

TArray<TSharedPtr<FJsonValue>> McpBuildPromptListEntries()
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FMcpWorkflowPrompt& Prompt : McpWorkflowPrompts())
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Prompt.Id);
		Obj->SetStringField(TEXT("title"), Prompt.Title);
		Obj->SetStringField(TEXT("description"), Prompt.Description);
		TArray<TSharedPtr<FJsonValue>> Arguments;
		for (const FMcpPromptArgumentSpec& Spec : Prompt.Arguments)
		{
			auto ArgObj = MakeShared<FJsonObject>();
			ArgObj->SetStringField(TEXT("name"), Spec.Name);
			ArgObj->SetStringField(TEXT("description"), Spec.Description);
			ArgObj->SetBoolField(TEXT("required"), Spec.bRequired);
			Arguments.Add(MakeShared<FJsonValueObject>(ArgObj));
		}
		Obj->SetArrayField(TEXT("arguments"), Arguments);
		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}
	return Items;
}

FMcpPromptRenderResult McpRenderWorkflowPrompt(const FString& Name, const TMap<FString, FString>& Args)
{
	FMcpPromptRenderResult Result;
	const FMcpWorkflowPrompt* Prompt = FindPrompt(Name);
	if (Prompt == nullptr)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown workflow prompt: %s"), *Name);
		return Result;
	}
	for (const TPair<FString, FString>& Arg : Args)
	{
		if (!Prompt->Arguments.ContainsByPredicate([&Arg](const FMcpPromptArgumentSpec& Spec) { return Spec.Name == Arg.Key; }))
		{
			Result.ErrorMessage = FString::Printf(TEXT("Unknown argument: %s"), *Arg.Key);
			return Result;
		}
	}
	TArray<TPair<FString, FString>> Inputs;
	for (const FMcpPromptArgumentSpec& Spec : Prompt->Arguments)
	{
		if (const FString* Raw = Args.Find(Spec.Name))
		{
			Inputs.Add(TPair<FString, FString>(Spec.Name, *Raw));
		}
		else if (Spec.bRequired)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Missing required argument: %s"), *Spec.Name);
			return Result;
		}
	}
	Result.bOk = true;
	Result.Body = RenderBody(*Prompt, Inputs);
	Result.Description = Prompt->Description;
	return Result;
}
