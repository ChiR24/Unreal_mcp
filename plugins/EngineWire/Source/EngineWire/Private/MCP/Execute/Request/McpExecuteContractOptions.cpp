// Copyright (c) 2024 EngineWire Contributors

#include "MCP/Execute/Request/McpExecuteContractOptions.h"

#include "Dom/JsonValue.h"
#include "Foundation/Solvers/McpPratt.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

namespace McpExecuteContracts
{
namespace
{
constexpr int32 MaxAssertions = 16;
constexpr int32 MaxAssertionLength = 300;

bool Mentions(const TSharedPtr<McpPratt::FNode>& Node, const TCHAR* Op)
{
    return Node.IsValid() && (Node->Op == Op || Node->Args.ContainsByPredicate([Op](const TSharedPtr<McpPratt::FNode>& Arg) { return Mentions(Arg, Op); }));
}

FMcpSemanticError Refuse(const TCHAR* Field, const FString& Message, const FString& Pointer)
{
    FMcpSemanticError Error = McpValidationError(TEXT("INVALID_OPTIONS"), Message, Pointer);
    Error.Option = Field;
    return Error;
}
} // namespace

bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
    for (const TCHAR* Field : {TEXT("expect"), TEXT("require")})
    {
        const TSharedPtr<FJsonValue> Value = Options.IsValid() ? Options->TryGetField(Field) : nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
        const FString Pointer = FString::Printf(TEXT("/options/%s"), Field);
        if (!Value.IsValid())
        {
            continue;
        }
        if (!Value->TryGetArray(Items) || Items->Num() < 1 || Items->Num() > MaxAssertions)
        {
            OutError = Refuse(Field, FString::Printf(TEXT("options.%s is an array of 1-%d assertions, such as [\"Lamp on Table\", "
                                                          "\"Cube.location.z in 0..10\"]."), Field, MaxAssertions), Pointer);
            return false;
        }
        for (int32 Index = 0; Index < Items->Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& Item = (*Items)[Index];
            const FString Text = Item.IsValid() && Item->Type == EJson::String ? Item->AsString() : FString();
            TSharedPtr<McpPratt::FNode> Root;
            FString Why = Text.IsEmpty() || Text.Len() > MaxAssertionLength
                ? FString::Printf(TEXT("must be an assertion of 1-%d characters"), MaxAssertionLength) : FString();
            if (Why.IsEmpty() && !McpPratt::Parse(Text, Root, Why))
            {
                Why = FString::Printf(TEXT("'%s': %s"), *Text, *Why);
            }
            else if (Why.IsEmpty() && FCString::Strcmp(Field, TEXT("require")) == 0 && Mentions(Root, TEXT("noWarnings")))
            {
                Why = TEXT("noWarnings reads the call's own reply, which a requirement runs before: put it in expect");
            }
            if (!Why.IsEmpty())
            {
                OutError = Refuse(Field, FString::Printf(TEXT("options.%s[%d] %s."), Field, Index, *Why),
                                  FString::Printf(TEXT("%s/%d"), *Pointer, Index));
                return false;
            }
        }
    }
    return true;
}

TArray<FString> Read(const TSharedPtr<FJsonObject>& Options, const TCHAR* Field)
{
    TArray<FString> Out;
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (Options.IsValid() && Options->TryGetArrayField(Field, Items))
    {
        for (const TSharedPtr<FJsonValue>& Item : *Items)
        {
            Out.Add(Item.IsValid() ? Item->AsString() : FString());
        }
    }
    return Out;
}

FString FingerprintPart(const TSharedPtr<FJsonObject>& Options)
{
    FString Out;
    for (const TCHAR* Field : {TEXT("expect"), TEXT("require")})
    {
        for (const FString& Text : Read(Options, Field))
        {
            Out += FString::Printf(TEXT(" %s%d:%s"), Field, Text.Len(), *Text); // length-prefixed: no two lists collide
        }
    }
    return Out;
}
} // namespace McpExecuteContracts
