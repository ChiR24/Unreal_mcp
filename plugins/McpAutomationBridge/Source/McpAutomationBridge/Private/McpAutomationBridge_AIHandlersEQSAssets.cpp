#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace McpAIHandlers
{
static UEnvQuery* CreateEQSQueryAsset(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }

    FString FullPath = SanitizedPath / Name;

    // Check if asset already exists
    if (FindObject<UEnvQuery>(nullptr, *FullPath) != nullptr)
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }

    // Also check if the package exists
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UEnvQuery* Query = NewObject<UEnvQuery>(Package, UEnvQuery::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Query)
    {
        OutError = TEXT("Failed to create EQS Query asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Query);
    McpSafeAssetSave(Query);

    return Query;
}

bool HandleCreateEQSQuery(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_eqs_query");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_eqs_query"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/EQS"));

        if (Name.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UEnvQuery* Query = CreateEQSQueryAsset(Path, Name, Error);
        if (!Query)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("queryPath"), Query->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created EQS Query: %s"), *Name));
        McpHandlerUtils::AddVerification(Result, Query);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("EQS Query created"), Result);
        return true;
    }

    return true;
}

bool HandleAddEQSGenerator(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_eqs_generator");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_eqs_generator"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString GeneratorType = GetStringFieldAI(Payload, TEXT("generatorType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Map generator type strings to actual UE classes via template or runtime lookup
        UEnvQueryGenerator* NewGenerator = nullptr;
        UClass* GeneratorClass = nullptr;
        if (GeneratorType.Equals(TEXT("ActorsOfClass"), ESearchCase::IgnoreCase) ||
            GeneratorType.Equals(TEXT("EnvQueryGenerator_ActorsOfClass"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_ActorsOfClass>(Query);
        }
        else if (GeneratorType.Equals(TEXT("OnCircle"), ESearchCase::IgnoreCase) ||
                 GeneratorType.Equals(TEXT("EnvQueryGenerator_OnCircle"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_OnCircle>(Query);
        }
        else if (GeneratorType.Equals(TEXT("SimpleGrid"), ESearchCase::IgnoreCase) ||
                 GeneratorType.Equals(TEXT("EnvQueryGenerator_SimpleGrid"), ESearchCase::IgnoreCase))
        {
            NewGenerator = NewObject<UEnvQueryGenerator_SimpleGrid>(Query);
        }
        else
        {
            // Fallback: try runtime class lookup with full class name
            GeneratorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), *GeneratorType));
            if (!GeneratorClass)
            {
                GeneratorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.EnvQueryGenerator_%s"), *GeneratorType));
            }
            if (GeneratorClass)
            {
                UObject* GenObj = NewObject<UObject>(Query, GeneratorClass);
                if (GenObj && GenObj->GetClass()->IsChildOf(UEnvQueryGenerator::StaticClass()))
                {
                    NewGenerator = static_cast<UEnvQueryGenerator*>(GenObj);
                }
            }
        }

        if (NewGenerator)
        {
            const TSharedPtr<FJsonObject>* GeneratorSettings = nullptr;
            if (Payload->TryGetObjectField(TEXT("generatorSettings"), GeneratorSettings) && GeneratorSettings && GeneratorSettings->IsValid())
            {
                double NumberValue = 0.0;
                if (UEnvQueryGenerator_ActorsOfClass* ActorsGenerator = Cast<UEnvQueryGenerator_ActorsOfClass>(NewGenerator))
                {
                    if ((*GeneratorSettings)->TryGetNumberField(TEXT("searchRadius"), NumberValue))
                    {
                        ActorsGenerator->SearchRadius.DefaultValue = static_cast<float>(NumberValue);
                        ActorsGenerator->GenerateOnlyActorsInRadius.DefaultValue = true;
                    }
                    FString ActorClassPath;
                    if ((*GeneratorSettings)->TryGetStringField(TEXT("actorClass"), ActorClassPath) && !ActorClassPath.IsEmpty())
                    {
                        if (UClass* ActorClass = ResolveClassByName(ActorClassPath))
                        {
                            if (ActorClass->IsChildOf(AActor::StaticClass()))
                            {
                                ActorsGenerator->SearchedActorClass = ActorClass;
                            }
                        }
                    }
                }
                else if (UEnvQueryGenerator_SimpleGrid* GridGenerator = Cast<UEnvQueryGenerator_SimpleGrid>(NewGenerator))
                {
                    if ((*GeneratorSettings)->TryGetNumberField(TEXT("gridSize"), NumberValue))
                    {
                        GridGenerator->GridSize.DefaultValue = static_cast<float>(NumberValue);
                    }
                    if ((*GeneratorSettings)->TryGetNumberField(TEXT("spacesBetween"), NumberValue))
                    {
                        GridGenerator->SpaceBetween.DefaultValue = static_cast<float>(NumberValue);
                    }
                }
                else if (UEnvQueryGenerator_OnCircle* CircleGenerator = Cast<UEnvQueryGenerator_OnCircle>(NewGenerator))
                {
                    if ((*GeneratorSettings)->TryGetNumberField(TEXT("searchRadius"), NumberValue) ||
                        (*GeneratorSettings)->TryGetNumberField(TEXT("outerRadius"), NumberValue))
                    {
                        CircleGenerator->CircleRadius.DefaultValue = static_cast<float>(NumberValue);
                    }
                    if ((*GeneratorSettings)->TryGetNumberField(TEXT("spacesBetween"), NumberValue))
                    {
                        CircleGenerator->SpaceBetween.DefaultValue = static_cast<float>(NumberValue);
                    }
                }
            }

            UEnvQueryOption* NewOption = NewObject<UEnvQueryOption>(Query);
            if (!NewOption)
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("Failed to create EQS option for generator"),
                                    TEXT("CREATION_FAILED"));
                return true;
            }

            NewOption->Generator = NewGenerator;
            const int32 OptionIndex = Query->GetOptionsMutable().Add(NewOption);
            Query->MarkPackageDirty();
            McpSafeAssetSave(Query);
            Result->SetNumberField(TEXT("optionIndex"), OptionIndex);
            Result->SetStringField(TEXT("generatorType"), GeneratorType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s generator"), *GeneratorType));
            McpHandlerUtils::AddVerification(Result, Query);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Generator added"), Result);
        }
        else
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create generator: %s"), *GeneratorType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    return true;
}
}
#endif
