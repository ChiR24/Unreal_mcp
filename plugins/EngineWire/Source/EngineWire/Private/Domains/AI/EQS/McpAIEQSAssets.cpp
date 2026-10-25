#include "Domains/AI/McpAIContext.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Donut.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace McpAIHandlers
{
static UEnvQuery* CreateEQSQueryAsset(const FString& Path, const FString& Name, FString& OutError, FString& OutCode, bool& bOutSaved)
{
    UEnvQuery* Query = CreateAIAssetInPackage<UEnvQuery>(
        Path, Name, TEXT("EQS Query"), OutError, OutCode);
    if (Query)
    {
        bOutSaved = McpSafeAssetSave(Query);
    }
    return Query;
}

// Implements the "create_eqs_query" action.
bool HandleCreateEQSQuery(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/EQS"));

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    FString ErrorCode;
    bool bSaved = false;
    UEnvQuery* Query = CreateEQSQueryAsset(Path, Name, Error, ErrorCode, bSaved);
    if (!Query)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }

    Result->SetStringField(TEXT("queryPath"), Query->GetPathName());
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created EQS Query: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, Query);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("EQS Query created"), Result);
    return true;
}

// The generatorSettings keys each generator reads (PathingGrid is a SimpleGrid); any other key is refused, not dropped.
static TArray<FString> GeneratorSettingKeys(const UClass* GeneratorClass)
{
    if (GeneratorClass->IsChildOf<UEnvQueryGenerator_ActorsOfClass>()) { return {TEXT("searchRadius"), TEXT("actorClass")}; }
    if (GeneratorClass->IsChildOf<UEnvQueryGenerator_SimpleGrid>()) { return {TEXT("gridSize"), TEXT("spacesBetween")}; }
    if (GeneratorClass->IsChildOf<UEnvQueryGenerator_OnCircle>()) { return {TEXT("searchRadius"), TEXT("outerRadius"), TEXT("spacesBetween")}; }
    if (GeneratorClass->IsChildOf<UEnvQueryGenerator_Donut>()) { return {TEXT("innerRadius"), TEXT("outerRadius")}; }
    return {};
}

static void ApplyGeneratorSettings(UEnvQueryGenerator* Generator, const TSharedPtr<FJsonObject>& Settings, UClass* ActorClass)
{
    double Value = 0.0;
    if (UEnvQueryGenerator_ActorsOfClass* Actors = Cast<UEnvQueryGenerator_ActorsOfClass>(Generator))
    {
        if (Settings->TryGetNumberField(TEXT("searchRadius"), Value))
        {
            Actors->SearchRadius.DefaultValue = static_cast<float>(Value);
            Actors->GenerateOnlyActorsInRadius.DefaultValue = true;
        }
        if (ActorClass)
        {
            Actors->SearchedActorClass = ActorClass;
        }
    }
    else if (UEnvQueryGenerator_SimpleGrid* Grid = Cast<UEnvQueryGenerator_SimpleGrid>(Generator))
    {
        if (Settings->TryGetNumberField(TEXT("gridSize"), Value)) { Grid->GridSize.DefaultValue = static_cast<float>(Value); }
        if (Settings->TryGetNumberField(TEXT("spacesBetween"), Value)) { Grid->SpaceBetween.DefaultValue = static_cast<float>(Value); }
    }
    else if (UEnvQueryGenerator_OnCircle* Circle = Cast<UEnvQueryGenerator_OnCircle>(Generator))
    {
        if (Settings->TryGetNumberField(TEXT("searchRadius"), Value) || Settings->TryGetNumberField(TEXT("outerRadius"), Value))
        {
            Circle->CircleRadius.DefaultValue = static_cast<float>(Value);
        }
        if (Settings->TryGetNumberField(TEXT("spacesBetween"), Value)) { Circle->SpaceBetween.DefaultValue = static_cast<float>(Value); }
    }
    else if (UEnvQueryGenerator_Donut* Donut = Cast<UEnvQueryGenerator_Donut>(Generator))
    {
        if (Settings->TryGetNumberField(TEXT("innerRadius"), Value)) { Donut->InnerRadius.DefaultValue = static_cast<float>(Value); }
        if (Settings->TryGetNumberField(TEXT("outerRadius"), Value)) { Donut->OuterRadius.DefaultValue = static_cast<float>(Value); }
    }
}

// Implements the "add_eqs_generator" action: a new option whose generator is generatorType, saved. Settings the generator
// does not read (searchCenter, innerRadius on a grid) and an actorClass that did not resolve were dropped with success.
bool HandleAddEQSGenerator(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    const FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    FString GeneratorType = GetJsonStringField(Payload, TEXT("generatorType"));
    GeneratorType.RemoveFromStart(TEXT("EnvQueryGenerator_"));
    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        return Fail(FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath), TEXT("NOT_FOUND"));
    }
    UClass* GeneratorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.EnvQueryGenerator_%s"), *GeneratorType));
    if (!GeneratorClass || !GeneratorClass->IsChildOf(UEnvQueryGenerator::StaticClass()) || GeneratorClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return Fail(FString::Printf(TEXT("generatorType %s is not an engine EQS generator. Nothing was added."), *GeneratorType), TEXT("INVALID_ARGUMENT"));
    }
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject>* SentSettings = nullptr;
    if (Payload->TryGetObjectField(TEXT("generatorSettings"), SentSettings) && SentSettings && SentSettings->IsValid())
    {
        Settings = *SentSettings;
    }
    const TArray<FString> Keys = GeneratorSettingKeys(GeneratorClass);
    for (const auto& Pair : Settings->Values)
    {
        // 5.8 keys are not FString: copy the text first.
        const FString Key(Pair.Key.Len(), *Pair.Key);
        if (!Keys.Contains(Key))
        {
            return Fail(FString::Printf(TEXT("generatorSettings.%s does not apply to %s, which reads %s; add_eqs_context sets where it centers. Nothing was added."),
                *Key, *GeneratorType, Keys.Num() > 0 ? *FString::Join(Keys, TEXT(", ")) : TEXT("no settings")), TEXT("INVALID_ARGUMENT"));
        }
    }
    FString ActorClassPath;
    UClass* ActorClass = Settings->TryGetStringField(TEXT("actorClass"), ActorClassPath) ? ResolveClassByName(ActorClassPath) : nullptr;
    if (!ActorClassPath.IsEmpty() && (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass())))
    {
        return Fail(FString::Printf(TEXT("actorClass %s is not an Actor class that loads. Nothing was added."), *ActorClassPath), TEXT("INVALID_ARGUMENT"));
    }

    Query->Modify();
    UEnvQueryGenerator* Generator = NewObject<UEnvQueryGenerator>(Query, GeneratorClass);
    ApplyGeneratorSettings(Generator, Settings, ActorClass);
    UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query);
    Option->Generator = Generator;
    const int32 OptionIndex = Query->GetOptionsMutable().Add(Option);
    Query->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("optionIndex"), OptionIndex);
    Result->SetStringField(TEXT("generatorType"), GeneratorType);
    Result->SetStringField(TEXT("generatorClass"), GeneratorClass->GetName());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Query));
    McpHandlerUtils::AddVerification(Result, Query);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Added %s generator as option %d"), *GeneratorType, OptionIndex), Result);
    return true;
}
}
