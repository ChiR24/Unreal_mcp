#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/Schema/McpPCGPropSchema.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
struct FPCGSchemaTransientFixture
{
    TStrongObjectPtr<UScriptStruct> Scope;
    TArray<TStrongObjectPtr<UEnum>> Enums;

    FPCGSchemaTransientFixture()
        : Scope(NewObject<UScriptStruct>(GetTransientPackage(), NAME_None, RF_Transient))
    {
    }

    UEnum* NewEnum(const FName& BaseName)
    {
        const FName Name = MakeUniqueObjectName(GetTransientPackage(), UEnum::StaticClass(), BaseName);
        TStrongObjectPtr<UEnum> Enum(NewObject<UEnum>(GetTransientPackage(), Name, RF_Transient));
        Enums.Add(MoveTemp(Enum));
        return Enums.Last().Get();
    }

    bool SetEnumNames(UEnum* Enum, TArray<TPair<FName, int64>>& Names, bool bWide, int32 HiddenIndex = INDEX_NONE)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
        const UEnum::EUnderlyingType UnderlyingType = bWide ? UEnum::EUnderlyingType::int64 : UEnum::EUnderlyingType::int32;
        const bool bSet = Enum->SetEnums(Names, UEnum::ECppForm::EnumClass, UnderlyingType,
            EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::No);
#else
        static_cast<void>(bWide);
        const bool bSet = Enum->SetEnums(Names, UEnum::ECppForm::EnumClass, EEnumFlags::None, false);
#endif
        if (!bSet) return false;
        if (HiddenIndex != INDEX_NONE) Enum->SetMetaData(TEXT("Hidden"), TEXT("true"), HiddenIndex);
        return true;
    }

    void AddIntProperty(const FName& Name, bool bEditable)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
        FIntProperty* Property = new FIntProperty(FFieldVariant(Scope.Get()), Name);
#else
        FIntProperty* Property = new FIntProperty(FFieldVariant(Scope.Get()), Name, RF_Transient);
#endif
        if (bEditable) Property->SetPropertyFlags(CPF_Edit);
        Scope->AddCppProperty(Property);
    }

    void AddEnumProperty(const FName& Name, UEnum* Enum, bool bWide)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
        FEnumProperty* Property = new FEnumProperty(FFieldVariant(Scope.Get()), Name);
        FNumericProperty* Underlying = bWide
            ? static_cast<FNumericProperty*>(new FInt64Property(FFieldVariant(Property), NAME_None))
            : static_cast<FNumericProperty*>(new FIntProperty(FFieldVariant(Property), NAME_None));
#else
        FEnumProperty* Property = new FEnumProperty(FFieldVariant(Scope.Get()), Name, RF_Transient);
        FNumericProperty* Underlying = bWide
            ? static_cast<FNumericProperty*>(new FInt64Property(FFieldVariant(Property), NAME_None, RF_Transient))
            : static_cast<FNumericProperty*>(new FIntProperty(FFieldVariant(Property), NAME_None, RF_Transient));
#endif
        Property->AddCppProperty(Underlying);
        Property->SetEnum(Enum);
        Property->SetPropertyFlags(CPF_Edit);
        Scope->AddCppProperty(Property);
    }
};

TSharedPtr<FJsonObject> FindItem(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
{
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("items"), Items) || !Items) return nullptr;
    for (const TSharedPtr<FJsonValue>& Value : *Items)
    {
        const TSharedPtr<FJsonObject> Item = Value->AsObject();
        if (Item.IsValid() && Item->GetStringField(TEXT("name")) == Name) return Item;
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FindOption(const TSharedPtr<FJsonObject>& Details, const FString& Name)
{
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (!Details.IsValid() || !Details->TryGetArrayField(TEXT("enumOptions"), Options) || !Options) return nullptr;
    for (const TSharedPtr<FJsonValue>& Value : *Options)
    {
        const TSharedPtr<FJsonObject> Option = Value->AsObject();
        if (Option.IsValid() && Option->GetStringField(TEXT("name")) == Name) return Option;
    }
    return nullptr;
}

TArray<TPair<FName, int64>> MakeLongOptions(const FString& EnumTypeName)
{
    TArray<TPair<FName, int64>> Names;
    for (int32 Index = 0; Index < 64; ++Index)
    {
        FString ShortName = FString::Printf(TEXT("Option%02d_"), Index);
        while (ShortName.Len() < 250) ShortName.AppendChar(TEXT('X'));
        const FString FullName = EnumTypeName + TEXT("::") + ShortName;
        Names.Emplace(FName(*FullName), Index);
    }
    return Names;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaDynamicEnumBoundsTest, "EngineWire.PCG.PropSchema.DynamicEnumBoundsAndInt64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaDynamicEnumBoundsTest::RunTest(const FString& Parameters)
{
    FPCGSchemaTransientFixture Fixture;
    UEnum* Enum = Fixture.NewEnum(FName(TEXT("McpPCGSchemaBoundsEnum")));
    const FString TypeName = Enum->GetFName().ToString();
    const int64 MinValue = static_cast<int64>(-9223372036854775807LL) - 1;
    const int64 MaxValue = 9223372036854775807LL;
    TArray<TPair<FName, int64>> Names;
    Names.Emplace(FName(*(TypeName + TEXT("::MinValue"))), MinValue);
    Names.Emplace(FName(*(TypeName + TEXT("::MaxValue"))), MaxValue);
    for (int32 Index = 2; Index < 66; ++Index)
        Names.Emplace(FName(*(TypeName + FString::Printf(TEXT("::Option%02d"), Index))), Index);
    const int32 HiddenIndex = Names.Num();
    Names.Emplace(FName(*(TypeName + TEXT("::HiddenOption"))), 1000);
    Names.Emplace(FName(*(TypeName + TEXT("::MAX"))), 1001);
    if (!TestTrue(TEXT("Transient enum accepts explicit names without auto-MAX"), Fixture.SetEnumNames(Enum, Names, true, HiddenIndex))) return false;
    Fixture.AddEnumProperty(FName(TEXT("Mode")), Enum, true);

    TSharedPtr<FJsonObject> Schema;
    FString Error;
    if (!TestTrue(TEXT("Dynamic enum schema succeeds"), McpPCGHandlers::DescribePCGPropertySchema(Fixture.Scope.Get(), 0, 1, Schema, Error))) return false;
    const TSharedPtr<FJsonObject> Item = FindItem(Schema, TEXT("Mode"));
    if (!TestTrue(TEXT("Enum property is returned"), Item.IsValid())) return false;
    const TSharedPtr<FJsonObject> Details = Item->GetObjectField(TEXT("typeDetails"));
    TestEqual(TEXT("Enum entry total includes hidden and terminal max"), Details->GetNumberField(TEXT("enumEntryTotal")), 68.0);
    TestEqual(TEXT("Visible enum total remains honest"), Details->GetNumberField(TEXT("enumOptionTotal")), 66.0);
    TestEqual(TEXT("Hidden option is counted separately"), Details->GetNumberField(TEXT("hiddenEnumOptionTotal")), 1.0);
    TestEqual(TEXT("Terminal max is counted separately"), Details->GetNumberField(TEXT("maxEnumOptionTotal")), 1.0);
    TestTrue(TEXT("Hidden omission is explicit"), Details->GetBoolField(TEXT("hiddenEnumOptionsOmitted")));
    TestTrue(TEXT("Max omission is explicit"), Details->GetBoolField(TEXT("maxEnumOptionsOmitted")));
    TestTrue(TEXT("Visible options report truncation beyond 64"), Details->GetBoolField(TEXT("enumOptionsTruncated")));
    TestEqual(TEXT("Only 64 visible options are emitted"), Details->GetArrayField(TEXT("enumOptions")).Num(), 64);
    const TSharedPtr<FJsonObject> MinOption = FindOption(Details, TEXT("MinValue"));
    const TSharedPtr<FJsonObject> MaxOption = FindOption(Details, TEXT("MaxValue"));
    FString SerializedValue;
    if (TestTrue(TEXT("INT64_MIN option is retained"), MinOption.IsValid()))
    {
        TestTrue(TEXT("INT64_MIN is represented as a string"), MinOption->TryGetStringField(TEXT("value"), SerializedValue));
        TestEqual(TEXT("INT64_MIN decimal value is exact"), SerializedValue, FString(TEXT("-9223372036854775808")));
    }
    if (TestTrue(TEXT("INT64_MAX option is retained"), MaxOption.IsValid()))
    {
        TestTrue(TEXT("INT64_MAX is represented as a string"), MaxOption->TryGetStringField(TEXT("value"), SerializedValue));
        TestEqual(TEXT("INT64_MAX decimal value is exact"), SerializedValue, FString(TEXT("9223372036854775807")));
    }
    TestFalse(TEXT("Hidden option is omitted from usable options"), FindOption(Details, TEXT("HiddenOption")).IsValid());
    TestFalse(TEXT("Terminal max is omitted from usable options"), FindOption(Details, TEXT("MAX")).IsValid());
    TestEqual(TEXT("Enum counts partition every entry"), Details->GetNumberField(TEXT("enumEntryTotal")),
        Details->GetNumberField(TEXT("enumOptionTotal")) + Details->GetNumberField(TEXT("hiddenEnumOptionTotal")) +
        Details->GetNumberField(TEXT("maxEnumOptionTotal")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaDynamicStructBoundsTest, "EngineWire.PCG.PropSchema.DynamicStructPropertyCapsAndEmpty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaDynamicStructBoundsTest::RunTest(const FString& Parameters)
{
    FPCGSchemaTransientFixture Empty;
    TSharedPtr<FJsonObject> Schema;
    FString Error;
    if (!TestTrue(TEXT("Valid empty dynamic struct succeeds"), McpPCGHandlers::DescribePCGPropertySchema(Empty.Scope.Get(), 0, 32, Schema, Error))) return false;
    TestEqual(TEXT("Empty struct reports zero eligible properties"), Schema->GetNumberField(TEXT("total")), 0.0);
    TestEqual(TEXT("Empty struct has no items"), Schema->GetArrayField(TEXT("items")).Num(), 0);
    TestFalse(TEXT("Empty struct is not truncated"), Schema->GetBoolField(TEXT("truncated")));

    FPCGSchemaTransientFixture Eligible;
    for (int32 Index = 0; Index < 257; ++Index)
        Eligible.AddIntProperty(FName(*FString::Printf(TEXT("Editable%04d"), Index)), true);
    Schema = MakeShared<FJsonObject>();
    TestFalse(TEXT("257 eligible properties exceed the 256-property cap"), McpPCGHandlers::DescribePCGPropertySchema(Eligible.Scope.Get(), 0, 1, Schema, Error));
    TestFalse(TEXT("Eligible-property failure clears output"), Schema.IsValid());
    TestTrue(TEXT("Eligible-property failure supplies an error"), !Error.IsEmpty());

    FPCGSchemaTransientFixture Visited;
    for (int32 Index = 0; Index < 1025; ++Index)
        Visited.AddIntProperty(FName(*FString::Printf(TEXT("NotEditable%04d"), Index)), false);
    Schema = MakeShared<FJsonObject>();
    TestFalse(TEXT("1025 visited properties exceed the visitation cap"), McpPCGHandlers::DescribePCGPropertySchema(Visited.Scope.Get(), 0, 1, Schema, Error));
    TestFalse(TEXT("Visited-property failure clears output"), Schema.IsValid());
    TestTrue(TEXT("Visited-property failure supplies an error"), !Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaSerializedOutputBoundTest, "EngineWire.PCG.PropSchema.DynamicEnumsRespectOutputCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaSerializedOutputBoundTest::RunTest(const FString& Parameters)
{
    FPCGSchemaTransientFixture Fixture;
    for (int32 EnumIndex = 0; EnumIndex < 4; ++EnumIndex)
    {
        UEnum* Enum = Fixture.NewEnum(FName(TEXT("McpPCGSchemaOutputEnum")));
        const FString TypeName = Enum->GetFName().ToString();
        TArray<TPair<FName, int64>> Names = MakeLongOptions(TypeName);
        if (!TestTrue(TEXT("Transient long-name enum is registered"), Fixture.SetEnumNames(Enum, Names, false))) return false;
        Fixture.AddEnumProperty(FName(*FString::Printf(TEXT("Output%d"), EnumIndex)), Enum, false);
    }

    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
    FString Error;
    TestFalse(TEXT("Four 64-option enum fields exceed the serialized character cap"),
        McpPCGHandlers::DescribePCGPropertySchema(Fixture.Scope.Get(), 0, 4, Schema, Error));
    TestFalse(TEXT("Output-budget failure clears the entire page"), Schema.IsValid());
    TestTrue(TEXT("Output-budget failure is explicit"), Error.Contains(TEXT("60000")));
    return true;
}
#endif
