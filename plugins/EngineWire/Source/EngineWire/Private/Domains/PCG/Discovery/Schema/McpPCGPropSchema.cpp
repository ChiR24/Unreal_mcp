#include "Domains/PCG/Discovery/Schema/McpPCGPropSchema.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/Schema/McpPCGPropTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace McpPCGHandlers
{
namespace
{
constexpr int32 PCGPropSchemaMaxVisitedProperties = 1024;
constexpr int32 PCGPropSchemaMaxEligibleProperties = 256;
constexpr int32 PCGPropSchemaMaxPage = 32;
constexpr int32 PCGPropSchemaMaxString = 1024;
constexpr int32 PCGPropSchemaMaxOutputCharacters = 60000;

struct FPCGPropSchemaItem
{
    FProperty* Property = nullptr;
    FString Name;
    FString Type;
    FString Category;
    FString DeclaredIn;
};

bool FailPCGPropSchema(TSharedPtr<FJsonObject>& OutSchema, FString& OutError, const TCHAR* Reason)
{
    OutSchema.Reset();
    OutError = Reason;
    return false;
}
}

bool DescribePCGPropertySchema(const UStruct* Scope, int32 Offset, int32 Limit,
    TSharedPtr<FJsonObject>& OutSchema, FString& OutError)
{
    OutSchema.Reset();
    OutError.Reset();
    if (!IsInGameThread())
        return FailPCGPropSchema(OutSchema, OutError, TEXT("PCG property schema reflection requires the game thread."));
    if (!IsValid(Scope))
        return FailPCGPropSchema(OutSchema, OutError, TEXT("A resolved reflected UStruct scope is required."));
    if (Offset < 0 || Limit < 1 || Limit > PCGPropSchemaMaxPage)
        return FailPCGPropSchema(OutSchema, OutError, TEXT("Property schema offset must be non-negative and limit must be between 1 and 32."));

    FPCGReadBudget Budget;
    const FString ScopePath = Scope->GetPathName();
    if (ScopePath.Len() > PCGPropSchemaMaxString)
        return FailPCGPropSchema(OutSchema, OutError, TEXT("Reflected scope path exceeds the 1024-character schema bound."));

    TArray<FPCGPropSchemaItem> Items;
    int32 Visited = 0;
    for (TFieldIterator<FProperty> It(Scope, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        if (++Visited > PCGPropSchemaMaxVisitedProperties)
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Reflected scope exceeds the 1024-property visitation bound."));
        if (!Budget.IsWithinTime())
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Property enumeration exceeded the 100 ms schema budget."));
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
        if (Items.Num() >= PCGPropSchemaMaxEligibleProperties)
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Reflected scope exceeds the 256 editable-property schema bound."));

        FPCGPropSchemaItem Item;
        Item.Property = Property;
        Item.Name = Property->GetName();
        Item.Type = GetPCGPropSchemaTypeName(Property);
        Item.Category = Property->GetMetaData(TEXT("Category"));
        const UStruct* Owner = Property->GetOwnerStruct();
        Item.DeclaredIn = Owner ? Owner->GetPathName() : TEXT("");
        if (Item.Name.Len() > PCGPropSchemaMaxString || Item.Type.Len() > PCGPropSchemaMaxString ||
            Item.Category.Len() > PCGPropSchemaMaxString || Item.DeclaredIn.Len() > PCGPropSchemaMaxString)
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Reflected property metadata exceeds the 1024-character string bound."));
        Items.Add(MoveTemp(Item));
    }
    Items.Sort([](const FPCGPropSchemaItem& A, const FPCGPropSchemaItem& B)
    {
        const int32 NameOrder = A.Name.Compare(B.Name, ESearchCase::CaseSensitive);
        return NameOrder == 0 ? A.DeclaredIn.Compare(B.DeclaredIn, ESearchCase::CaseSensitive) < 0 : NameOrder < 0;
    });

    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
    Schema->SetStringField(TEXT("scope"), ScopePath);
    Schema->SetBoolField(TEXT("metadataOnly"), true);
    Schema->SetNumberField(TEXT("total"), Items.Num());
    Schema->SetNumberField(TEXT("offset"), Offset);
    Schema->SetNumberField(TEXT("limit"), Limit);
    FString BaseEncoded;
    const auto BaseWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&BaseEncoded);
    if (!FJsonSerializer::Serialize(Schema, BaseWriter))
        return FailPCGPropSchema(OutSchema, OutError, TEXT("Property schema envelope could not be encoded."));
    int64 PageCharacters = BaseEncoded.Len() + 64;
    TArray<TSharedPtr<FJsonValue>> Page;
    const int32 End = static_cast<int32>(FMath::Min<int64>(Items.Num(), static_cast<int64>(Offset) + Limit));
    for (int32 Index = Offset; Index < End; ++Index)
    {
        if (!Budget.IsWithinTime())
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Property schema projection exceeded the 100 ms budget."));
        const FPCGPropSchemaItem& Source = Items[Index];
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Source.Name);
        Item->SetStringField(TEXT("type"), Source.Type);
        Item->SetStringField(TEXT("category"), Source.Category);
        Item->SetStringField(TEXT("declaredIn"), Source.DeclaredIn);
        Item->SetBoolField(TEXT("readOnly"), Source.Property->HasAnyPropertyFlags(CPF_EditConst));
        Item->SetBoolField(TEXT("editable"), true);
        Item->SetBoolField(TEXT("instancedReference"), Source.Property->HasAnyPropertyFlags(CPF_InstancedReference));

        TSharedPtr<FJsonObject> Details;
        if (!BuildPCGPropSchemaType(Source.Property, Source.Type, 0, Budget, Details, OutError))
        {
            OutSchema.Reset();
            if (OutError.IsEmpty()) OutError = TEXT("A reflected property descriptor could not be built.");
            return false;
        }
        Item->SetObjectField(TEXT("typeDetails"), Details);
        FString ItemEncoded;
        const auto ItemWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ItemEncoded);
        if (!FJsonSerializer::Serialize(Item, ItemWriter))
            return FailPCGPropSchema(OutSchema, OutError, TEXT("A reflected property item could not be encoded."));
        PageCharacters += ItemEncoded.Len() + 1;
        if (PageCharacters > PCGPropSchemaMaxOutputCharacters)
            return FailPCGPropSchema(OutSchema, OutError, TEXT("Property page exceeds the 60000-character output budget."));
        Page.Add(MakeShared<FJsonValueObject>(Item));
    }
    Schema->SetArrayField(TEXT("items"), Page);
    Schema->SetBoolField(TEXT("truncated"), static_cast<int64>(Offset) + Page.Num() < Items.Num());
    FString Encoded;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Schema, Writer) || Encoded.Len() > PCGPropSchemaMaxOutputCharacters || !Budget.IsWithinTime())
        return FailPCGPropSchema(OutSchema, OutError, TEXT("Serialized property schema exceeded 60000 characters or the 100 ms budget."));
    OutSchema = MoveTemp(Schema);
    return true;
}
}
#endif
