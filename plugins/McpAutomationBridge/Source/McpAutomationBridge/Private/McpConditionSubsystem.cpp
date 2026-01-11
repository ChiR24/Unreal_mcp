// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpConditionSubsystem.h"
#include "McpWorldTimeSubsystem.h"
#include "McpFactionSubsystem.h"
#include "McpZoneSubsystem.h"
#include "McpValueTrackerComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpCondition, Log, All);

void UMcpConditionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogMcpCondition, Log, TEXT("MCP Condition Subsystem initialized"));
}

void UMcpConditionSubsystem::Deinitialize()
{
    Conditions.Empty();
    Listeners.Empty();
    
    UE_LOG(LogMcpCondition, Log, TEXT("MCP Condition Subsystem deinitialized"));
    
    Super::Deinitialize();
}

bool UMcpConditionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Create for all world types (Editor, PIE, Game)
    return true;
}

bool UMcpConditionSubsystem::CreateCondition(const FString& ConditionId, const FString& PredicateJson)
{
    if (ConditionId.IsEmpty())
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCondition: ConditionId cannot be empty"));
        return false;
    }
    
    if (PredicateJson.IsEmpty())
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCondition: PredicateJson cannot be empty"));
        return false;
    }
    
    // Check for duplicate
    if (Conditions.Contains(ConditionId))
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCondition: Condition '%s' already exists"), *ConditionId);
        return false;
    }
    
    // Parse JSON predicate
    TSharedPtr<FJsonObject> ParsedPredicate;
    TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(PredicateJson);
    
    if (!FJsonSerializer::Deserialize(JsonReader, ParsedPredicate) || !ParsedPredicate.IsValid())
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCondition: Failed to parse JSON predicate for '%s'"), *ConditionId);
        return false;
    }
    
    // Validate predicate has required 'type' field
    if (!ParsedPredicate->HasField(TEXT("type")))
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCondition: Predicate missing 'type' field for '%s'"), *ConditionId);
        return false;
    }
    
    FMcpConditionDefinition NewCondition;
    NewCondition.ConditionId = ConditionId;
    NewCondition.PredicateJson = PredicateJson;
    NewCondition.ParsedPredicate = ParsedPredicate;
    
    Conditions.Add(ConditionId, NewCondition);
    
    UE_LOG(LogMcpCondition, Log, TEXT("Created condition '%s'"), *ConditionId);
    
    return true;
}

bool UMcpConditionSubsystem::CreateCompoundCondition(const FString& ConditionId, const FString& Operator, const TArray<FString>& ConditionIds)
{
    if (ConditionId.IsEmpty())
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCompoundCondition: ConditionId cannot be empty"));
        return false;
    }
    
    if (Conditions.Contains(ConditionId))
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCompoundCondition: Condition '%s' already exists"), *ConditionId);
        return false;
    }
    
    // Validate operator
    if (Operator != TEXT("all") && Operator != TEXT("any") && Operator != TEXT("not"))
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCompoundCondition: Invalid operator '%s', must be 'all', 'any', or 'not'"), *Operator);
        return false;
    }
    
    // Validate "not" has exactly one condition
    if (Operator == TEXT("not") && ConditionIds.Num() != 1)
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("CreateCompoundCondition: 'not' operator requires exactly one condition"));
        return false;
    }
    
    // Validate all referenced conditions exist
    for (const FString& RefConditionId : ConditionIds)
    {
        if (!Conditions.Contains(RefConditionId))
        {
            UE_LOG(LogMcpCondition, Warning, TEXT("CreateCompoundCondition: Referenced condition '%s' not found"), *RefConditionId);
            return false;
        }
    }
    
    // Build compound predicate JSON
    TSharedPtr<FJsonObject> CompoundPredicate = MakeShared<FJsonObject>();
    CompoundPredicate->SetStringField(TEXT("type"), Operator);
    
    TArray<TSharedPtr<FJsonValue>> ConditionsArray;
    for (const FString& RefConditionId : ConditionIds)
    {
        // Reference existing condition by ID
        TSharedPtr<FJsonObject> ConditionRef = MakeShared<FJsonObject>();
        ConditionRef->SetStringField(TEXT("type"), TEXT("condition_ref"));
        ConditionRef->SetStringField(TEXT("condition_id"), RefConditionId);
        ConditionsArray.Add(MakeShared<FJsonValueObject>(ConditionRef));
    }
    
    CompoundPredicate->SetArrayField(TEXT("conditions"), ConditionsArray);
    
    // Serialize to JSON string
    FString SerializedJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedJson);
    FJsonSerializer::Serialize(CompoundPredicate.ToSharedRef(), Writer);
    
    FMcpConditionDefinition NewCondition;
    NewCondition.ConditionId = ConditionId;
    NewCondition.PredicateJson = SerializedJson;
    NewCondition.ParsedPredicate = CompoundPredicate;
    
    Conditions.Add(ConditionId, NewCondition);
    
    UE_LOG(LogMcpCondition, Log, TEXT("Created compound condition '%s' with operator '%s' and %d child conditions"),
        *ConditionId, *Operator, ConditionIds.Num());
    
    return true;
}

bool UMcpConditionSubsystem::EvaluateCondition(const FString& ConditionId, bool& OutResult)
{
    OutResult = false;
    
    FMcpConditionDefinition* Condition = Conditions.Find(ConditionId);
    if (!Condition)
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("EvaluateCondition: Condition '%s' not found"), *ConditionId);
        return false;
    }
    
    // Re-parse if needed (ParsedPredicate may be null if loaded from serialization)
    if (!Condition->ParsedPredicate.IsValid())
    {
        TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Condition->PredicateJson);
        if (!FJsonSerializer::Deserialize(JsonReader, Condition->ParsedPredicate) || !Condition->ParsedPredicate.IsValid())
        {
            UE_LOG(LogMcpCondition, Warning, TEXT("EvaluateCondition: Failed to parse cached predicate for '%s'"), *ConditionId);
            return false;
        }
    }
    
    OutResult = EvaluatePredicate(Condition->ParsedPredicate);
    
    // Notify listeners
    NotifyListeners(ConditionId, OutResult);
    
    UE_LOG(LogMcpCondition, Verbose, TEXT("Evaluated condition '%s': %s"), *ConditionId, OutResult ? TEXT("TRUE") : TEXT("FALSE"));
    
    return true;
}

bool UMcpConditionSubsystem::EvaluatePredicate(const TSharedPtr<FJsonObject>& Predicate)
{
    if (!Predicate.IsValid())
    {
        return false;
    }
    
    FString Type = Predicate->GetStringField(TEXT("type"));
    
    if (Type == TEXT("all"))
    {
        // All child conditions must be true
        const TArray<TSharedPtr<FJsonValue>>* ConditionsArray;
        if (!Predicate->TryGetArrayField(TEXT("conditions"), ConditionsArray))
        {
            return false;
        }
        
        for (const TSharedPtr<FJsonValue>& CondValue : *ConditionsArray)
        {
            const TSharedPtr<FJsonObject>* ChildPredicate;
            if (CondValue->TryGetObject(ChildPredicate))
            {
                if (!EvaluatePredicate(*ChildPredicate))
                {
                    return false;
                }
            }
        }
        return true;
    }
    else if (Type == TEXT("any"))
    {
        // Any child condition must be true
        const TArray<TSharedPtr<FJsonValue>>* ConditionsArray;
        if (!Predicate->TryGetArrayField(TEXT("conditions"), ConditionsArray))
        {
            return false;
        }
        
        for (const TSharedPtr<FJsonValue>& CondValue : *ConditionsArray)
        {
            const TSharedPtr<FJsonObject>* ChildPredicate;
            if (CondValue->TryGetObject(ChildPredicate))
            {
                if (EvaluatePredicate(*ChildPredicate))
                {
                    return true;
                }
            }
        }
        return false;
    }
    else if (Type == TEXT("not"))
    {
        // Negate child condition
        const TArray<TSharedPtr<FJsonValue>>* ConditionsArray;
        if (!Predicate->TryGetArrayField(TEXT("conditions"), ConditionsArray) || ConditionsArray->Num() == 0)
        {
            // Try single condition field
            const TSharedPtr<FJsonObject>* ChildPredicate;
            if (Predicate->TryGetObjectField(TEXT("condition"), ChildPredicate))
            {
                return !EvaluatePredicate(*ChildPredicate);
            }
            return false;
        }
        
        const TSharedPtr<FJsonObject>* ChildPredicate;
        if ((*ConditionsArray)[0]->TryGetObject(ChildPredicate))
        {
            return !EvaluatePredicate(*ChildPredicate);
        }
        return false;
    }
    else if (Type == TEXT("compare"))
    {
        // Compare two operands
        FString Operator = Predicate->GetStringField(TEXT("operator"));
        
        const TSharedPtr<FJsonObject>* LeftOperand;
        const TSharedPtr<FJsonObject>* RightOperand;
        
        if (!Predicate->TryGetObjectField(TEXT("left"), LeftOperand) ||
            !Predicate->TryGetObjectField(TEXT("right"), RightOperand))
        {
            UE_LOG(LogMcpCondition, Warning, TEXT("Compare predicate missing 'left' or 'right' operand"));
            return false;
        }
        
        // For equality on strings (e.g., world_time period)
        FString LeftType = (*LeftOperand)->GetStringField(TEXT("type"));
        FString RightType = (*RightOperand)->GetStringField(TEXT("type"));
        
        // Check if this is a string comparison (world_time with "period" field or const with string value)
        bool bIsStringComparison = false;
        if (LeftType == TEXT("world_time") && (*LeftOperand)->HasField(TEXT("field")))
        {
            FString Field = (*LeftOperand)->GetStringField(TEXT("field"));
            if (Field == TEXT("period"))
            {
                bIsStringComparison = true;
            }
        }
        if (RightType == TEXT("const") && (*RightOperand)->HasTypedField<EJson::String>(TEXT("value")))
        {
            bIsStringComparison = true;
        }
        
        if (bIsStringComparison && (Operator == TEXT("eq") || Operator == TEXT("neq")))
        {
            FString LeftStr = ResolveOperandString(*LeftOperand);
            FString RightStr = ResolveOperandString(*RightOperand);
            
            if (Operator == TEXT("eq"))
            {
                return LeftStr.Equals(RightStr, ESearchCase::IgnoreCase);
            }
            else
            {
                return !LeftStr.Equals(RightStr, ESearchCase::IgnoreCase);
            }
        }
        
        // Numeric comparison
        float LeftValue = ResolveOperandValue(*LeftOperand);
        float RightValue = ResolveOperandValue(*RightOperand);
        
        return CompareValues(LeftValue, RightValue, Operator);
    }
    else if (Type == TEXT("condition_ref"))
    {
        // Reference to another condition
        FString RefConditionId = Predicate->GetStringField(TEXT("condition_id"));
        bool Result = false;
        EvaluateCondition(RefConditionId, Result);
        return Result;
    }
    
    UE_LOG(LogMcpCondition, Warning, TEXT("Unknown predicate type: %s"), *Type);
    return false;
}

float UMcpConditionSubsystem::ResolveOperandValue(const TSharedPtr<FJsonObject>& Operand)
{
    if (!Operand.IsValid())
    {
        return 0.0f;
    }
    
    FString Type = Operand->GetStringField(TEXT("type"));
    
    if (Type == TEXT("const"))
    {
        return Operand->GetNumberField(TEXT("value"));
    }
    else if (Type == TEXT("value_tracker"))
    {
        // Query McpValueTrackerComponent on specified actor
        FString ActorId = Operand->GetStringField(TEXT("actor"));
        FString Key = Operand->GetStringField(TEXT("key"));
        
        UWorld* World = GetWorld();
        if (!World)
        {
            return 0.0f;
        }
        
        // Find actor by label or name
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor)
            {
                continue;
            }
            
            // Check label or name match
            if (Actor->GetActorLabel() == ActorId || Actor->GetName() == ActorId)
            {
                UMcpValueTrackerComponent* ValueTracker = Actor->FindComponentByClass<UMcpValueTrackerComponent>();
                if (ValueTracker)
                {
                    float Value = 0.0f;
                    if (ValueTracker->GetValue(Key, Value))
                    {
                        return Value;
                    }
                }
                break;
            }
        }
        
        UE_LOG(LogMcpCondition, Warning, TEXT("ResolveOperandValue: value_tracker - Actor '%s' or key '%s' not found"), *ActorId, *Key);
        return 0.0f;
    }
    else if (Type == TEXT("world_time"))
    {
        // Query McpWorldTimeSubsystem
        UMcpWorldTimeSubsystem* WorldTime = GetWorld()->GetSubsystem<UMcpWorldTimeSubsystem>();
        if (!WorldTime)
        {
            return 0.0f;
        }
        
        FString Field = Operand->GetStringField(TEXT("field"));
        if (Field == TEXT("hour") || Field.IsEmpty())
        {
            return WorldTime->GetWorldTime();
        }
        else if (Field == TEXT("day"))
        {
            return static_cast<float>(WorldTime->GetDay());
        }
        else if (Field == TEXT("minute"))
        {
            return static_cast<float>(WorldTime->GetMinute());
        }
        
        return WorldTime->GetWorldTime();
    }
    else if (Type == TEXT("faction_reputation"))
    {
        // Query McpFactionSubsystem
        FString ActorId = Operand->GetStringField(TEXT("actor"));
        FString FactionId = Operand->GetStringField(TEXT("faction"));
        
        UMcpFactionSubsystem* FactionSystem = GetWorld()->GetSubsystem<UMcpFactionSubsystem>();
        if (!FactionSystem)
        {
            return 0.0f;
        }
        
        float Reputation = 0.0f;
        if (FactionSystem->GetReputation(ActorId, FactionId, Reputation))
        {
            return Reputation;
        }
        
        return 0.0f;
    }
    else if (Type == TEXT("zone_membership"))
    {
        // Query McpZoneSubsystem - returns 1 if in zone, 0 otherwise
        FString ActorId = Operand->GetStringField(TEXT("actor"));
        FString ZoneId = Operand->GetStringField(TEXT("zone"));
        
        UMcpZoneSubsystem* ZoneSystem = GetWorld()->GetSubsystem<UMcpZoneSubsystem>();
        if (!ZoneSystem)
        {
            return 0.0f;
        }
        
        UWorld* World = GetWorld();
        if (!World)
        {
            return 0.0f;
        }
        
        // Find actor
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor)
            {
                continue;
            }
            
            if (Actor->GetActorLabel() == ActorId || Actor->GetName() == ActorId)
            {
                FString CurrentZoneId;
                FString CurrentZoneName;
                if (ZoneSystem->GetActorZone(Actor, CurrentZoneId, CurrentZoneName))
                {
                    return CurrentZoneId == ZoneId ? 1.0f : 0.0f;
                }
                break;
            }
        }
        
        return 0.0f;
    }
    
    UE_LOG(LogMcpCondition, Warning, TEXT("ResolveOperandValue: Unknown operand type '%s'"), *Type);
    return 0.0f;
}

FString UMcpConditionSubsystem::ResolveOperandString(const TSharedPtr<FJsonObject>& Operand)
{
    if (!Operand.IsValid())
    {
        return FString();
    }
    
    FString Type = Operand->GetStringField(TEXT("type"));
    
    if (Type == TEXT("const"))
    {
        return Operand->GetStringField(TEXT("value"));
    }
    else if (Type == TEXT("world_time"))
    {
        FString Field = Operand->GetStringField(TEXT("field"));
        
        if (Field == TEXT("period"))
        {
            UMcpWorldTimeSubsystem* WorldTime = GetWorld()->GetSubsystem<UMcpWorldTimeSubsystem>();
            if (!WorldTime)
            {
                return FString();
            }
            
            EMcpTimePeriod Period = WorldTime->GetCurrentPeriod();
            switch (Period)
            {
                case EMcpTimePeriod::Dawn: return TEXT("dawn");
                case EMcpTimePeriod::Day: return TEXT("day");
                case EMcpTimePeriod::Dusk: return TEXT("dusk");
                case EMcpTimePeriod::Night: return TEXT("night");
                default: return TEXT("unknown");
            }
        }
    }
    
    return FString();
}

bool UMcpConditionSubsystem::CompareValues(float Left, float Right, const FString& Operator)
{
    if (Operator == TEXT("eq"))
    {
        return FMath::IsNearlyEqual(Left, Right, KINDA_SMALL_NUMBER);
    }
    else if (Operator == TEXT("neq"))
    {
        return !FMath::IsNearlyEqual(Left, Right, KINDA_SMALL_NUMBER);
    }
    else if (Operator == TEXT("gt"))
    {
        return Left > Right;
    }
    else if (Operator == TEXT("gte"))
    {
        return Left >= Right;
    }
    else if (Operator == TEXT("lt"))
    {
        return Left < Right;
    }
    else if (Operator == TEXT("lte"))
    {
        return Left <= Right;
    }
    
    UE_LOG(LogMcpCondition, Warning, TEXT("CompareValues: Unknown operator '%s'"), *Operator);
    return false;
}

bool UMcpConditionSubsystem::AddConditionListener(const FString& ConditionId, const FString& ListenerId, bool bOneShot)
{
    if (ConditionId.IsEmpty() || ListenerId.IsEmpty())
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("AddConditionListener: ConditionId and ListenerId cannot be empty"));
        return false;
    }
    
    if (!Conditions.Contains(ConditionId))
    {
        UE_LOG(LogMcpCondition, Warning, TEXT("AddConditionListener: Condition '%s' not found"), *ConditionId);
        return false;
    }
    
    // Check for duplicate listener
    for (const FMcpConditionListener& Listener : Listeners)
    {
        if (Listener.ListenerId == ListenerId)
        {
            UE_LOG(LogMcpCondition, Warning, TEXT("AddConditionListener: Listener '%s' already exists"), *ListenerId);
            return false;
        }
    }
    
    FMcpConditionListener NewListener;
    NewListener.ListenerId = ListenerId;
    NewListener.ConditionId = ConditionId;
    NewListener.bOneShot = bOneShot;
    NewListener.bHasTriggered = false;
    
    Listeners.Add(NewListener);
    
    UE_LOG(LogMcpCondition, Log, TEXT("Added listener '%s' for condition '%s' (OneShot=%s)"),
        *ListenerId, *ConditionId, bOneShot ? TEXT("true") : TEXT("false"));
    
    return true;
}

bool UMcpConditionSubsystem::RemoveConditionListener(const FString& ListenerId)
{
    for (int32 i = Listeners.Num() - 1; i >= 0; --i)
    {
        if (Listeners[i].ListenerId == ListenerId)
        {
            Listeners.RemoveAt(i);
            UE_LOG(LogMcpCondition, Log, TEXT("Removed listener '%s'"), *ListenerId);
            return true;
        }
    }
    
    UE_LOG(LogMcpCondition, Warning, TEXT("RemoveConditionListener: Listener '%s' not found"), *ListenerId);
    return false;
}

TArray<FString> UMcpConditionSubsystem::GetAllConditionIds() const
{
    TArray<FString> ConditionIds;
    Conditions.GetKeys(ConditionIds);
    return ConditionIds;
}

void UMcpConditionSubsystem::NotifyListeners(const FString& ConditionId, bool bResult)
{
    // Broadcast delegate
    OnConditionTriggered.Broadcast(ConditionId, bResult);
    
    // Process listeners
    TArray<FString> ListenersToRemove;
    
    for (FMcpConditionListener& Listener : Listeners)
    {
        if (Listener.ConditionId != ConditionId)
        {
            continue;
        }
        
        // Only trigger on true result for listeners
        if (bResult)
        {
            UE_LOG(LogMcpCondition, Log, TEXT("Listener '%s' triggered for condition '%s'"),
                *Listener.ListenerId, *ConditionId);
            
            if (Listener.bOneShot)
            {
                Listener.bHasTriggered = true;
                ListenersToRemove.Add(Listener.ListenerId);
            }
        }
    }
    
    // Remove one-shot listeners that have triggered
    for (const FString& ListenerId : ListenersToRemove)
    {
        RemoveConditionListener(ListenerId);
    }
}
