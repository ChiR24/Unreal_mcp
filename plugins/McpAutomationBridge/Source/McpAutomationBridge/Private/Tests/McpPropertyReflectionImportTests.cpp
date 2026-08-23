#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/SkyLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
UEnum *CreateEnumImportTestEnum()
{
    UEnum *Enum = NewObject<UEnum>(
        GetTransientPackage(), MakeUniqueObjectName(
                                   GetTransientPackage(), UEnum::StaticClass(),
                                   TEXT("McpEnumImportTest")));
    TArray<TPair<FName, int64>> Names = {
        {TEXT("McpEnumImportTest::Negative"), -1},
        {TEXT("McpEnumImportTest::Visible"), 0},
        {TEXT("McpEnumImportTest::Hidden"), 1},
    };
    MCP_SET_ENUMS(Enum, Names, UEnum::ECppForm::EnumClass);
    Enum->SetMetaData(TEXT("DisplayName"), TEXT("Friendly Visible"), 1);
    Enum->SetMetaData(TEXT("Hidden"), TEXT(""), 2);
    return Enum;
}

FEnumProperty *CreateIntEnumProperty(UEnum *Enum)
{
    FEnumProperty *Property =
        new FEnumProperty(Enum, TEXT("McpEnumValue"), RF_Transient);
    Property->SetEnum(Enum);
    Property->AddCppProperty(
        new FIntProperty(Property, TEXT("UnderlyingType"), RF_Transient));
    return Property;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropertyReflectionRejectsInvalidEnumNumbersTest,
    "McpAutomationBridge.Reflection.InvalidEnumNumbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropertyReflectionRejectsInvalidEnumNumbersTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    USkyLightComponent *Component = NewObject<USkyLightComponent>();
    FProperty *SourceTypeProperty =
        FindFProperty<FProperty>(USkyLightComponent::StaticClass(), TEXT("SourceType"));
    if (!TestNotNull(TEXT("Sky light source type property exists"), SourceTypeProperty))
    {
        return false;
    }

    FString Error;
    TestFalse(
        TEXT("Fractional enum values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty, MakeShared<FJsonValueNumber>(1.5), Error));
    TestTrue(TEXT("Fractional enum error explains the integer requirement"), Error.Contains(TEXT("integer")));

    Error.Empty();
    TestFalse(
        TEXT("Out-of-range enum values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty, MakeShared<FJsonValueNumber>(9223372036854775808.0), Error));
    TestTrue(TEXT("Out-of-range enum error explains the int64 requirement"), Error.Contains(TEXT("int64")));

    Error.Empty();
    TestTrue(
        TEXT("Valid numeric enum values remain supported"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty, MakeShared<FJsonValueNumber>(0.0), Error));

    Error.Empty();
    TestTrue(
        TEXT("Valid enum names remain supported"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueString>(TEXT("SLS_SpecifiedCubemap")), Error));

    Error.Empty();
    TestFalse(
        TEXT("Non-finite enum values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueNumber>(
                std::numeric_limits<double>::infinity()),
            Error));

    Error.Empty();
    TestFalse(
        TEXT("NaN enum values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueNumber>(
                std::numeric_limits<double>::quiet_NaN()),
            Error));

    Error.Empty();
    TestFalse(
        TEXT("Byte-backed enum max sentinels are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueNumber>(
                static_cast<double>(SLS_MAX)),
            Error));

    FString EmbeddedNullName(TEXT("SLS_CapturedScene"));
    EmbeddedNullName.GetCharArray().Insert(TEXT('\0'), EmbeddedNullName.Len());
    EmbeddedNullName.Append(TEXT("SLS_MAX"));
    Error.Empty();
    TestFalse(
        TEXT("Enum names containing an embedded NUL are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueString>(EmbeddedNullName), Error));
    TestTrue(
        TEXT("Embedded NUL error identifies the invalid string"),
        Error.Contains(TEXT("NUL")));

    const FString UnknownEnumName = FString::Printf(
        TEXT("McpUnknownEnumName_%llu"),
        static_cast<unsigned long long>(FPlatformTime::Cycles64()));
    TestTrue(
        TEXT("Unknown enum fixture starts outside the FName pool"),
        FName(*UnknownEnumName, FNAME_Find).IsNone());
    Error.Empty();
    TestFalse(
        TEXT("Unknown enum names are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Component, SourceTypeProperty,
            MakeShared<FJsonValueString>(UnknownEnumName), Error));
    TestTrue(
        TEXT("Rejected enum names are not added to the FName pool"),
        FName(*UnknownEnumName, FNAME_Find).IsNone());

    UEnum *TestEnum = CreateEnumImportTestEnum();
    FEnumProperty *EnumProperty = CreateIntEnumProperty(TestEnum);
    int32 EnumStorage = 0;

    Error.Empty();
    TestTrue(
        TEXT("A visible negative enum value is accepted by name"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            &EnumStorage, EnumProperty,
            MakeShared<FJsonValueString>(TEXT("Negative")), Error));
    TestEqual(TEXT("Named negative enum value is stored"), EnumStorage, -1);

    EnumStorage = 0;
    Error.Empty();
    TestTrue(
        TEXT("A visible negative enum value is accepted numerically"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            &EnumStorage, EnumProperty, MakeShared<FJsonValueNumber>(-1.0), Error));
    TestEqual(TEXT("Numeric negative enum value is stored"), EnumStorage, -1);

    Error.Empty();
    TestTrue(
        TEXT("Enum display names remain supported"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            &EnumStorage, EnumProperty,
            MakeShared<FJsonValueString>(TEXT("Friendly Visible")), Error));
    TestEqual(TEXT("Display-name enum value is stored"), EnumStorage, 0);

    Error.Empty();
    TestFalse(
        TEXT("Hidden enum values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            &EnumStorage, EnumProperty, MakeShared<FJsonValueNumber>(1.0), Error));

    const int32 MaxIndex = TestEnum->NumEnums() - 1;
    Error.Empty();
    TestFalse(
        TEXT("Generated enum max sentinels are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            &EnumStorage, EnumProperty,
            MakeShared<FJsonValueNumber>(
                static_cast<double>(TestEnum->GetValueByIndex(MaxIndex))),
            Error));

    UCharacterMovementComponent *Movement =
        NewObject<UCharacterMovementComponent>();
    FIntProperty *IntProperty = FindFProperty<FIntProperty>(
        UCharacterMovementComponent::StaticClass(),
        TEXT("MaxSimulationIterations"));
    if (!TestNotNull(TEXT("Character movement int property exists"), IntProperty))
    {
        delete EnumProperty;
        return false;
    }
    const int32 OriginalSimulationIterations =
        Movement->MaxSimulationIterations;
    Error.Empty();
    TestFalse(
        TEXT("Positive int32 overflow is rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, IntProperty,
            MakeShared<FJsonValueNumber>(2147483648.0), Error));
    TestEqual(
        TEXT("Rejected positive overflow leaves the int property unchanged"),
        Movement->MaxSimulationIterations,
        OriginalSimulationIterations);
    TestTrue(
        TEXT("Positive overflow error identifies the 32-bit requirement"),
        Error.Contains(TEXT("32-bit")));
    Error.Empty();
    TestFalse(
        TEXT("Negative int32 overflow strings are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, IntProperty,
            MakeShared<FJsonValueString>(TEXT("-2147483649")), Error));
    TestEqual(
        TEXT("Rejected negative overflow leaves the int property unchanged"),
        Movement->MaxSimulationIterations,
        OriginalSimulationIterations);
    Error.Empty();
    TestTrue(
        TEXT("Valid int32 strings remain supported"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, IntProperty,
            MakeShared<FJsonValueString>(TEXT("42")), Error));
    TestEqual(
        TEXT("Valid int32 value is assigned"),
        Movement->MaxSimulationIterations,
        42);

    FByteProperty *ByteProperty = FindFProperty<FByteProperty>(
        UCharacterMovementComponent::StaticClass(), TEXT("CustomMovementMode"));
    if (!TestNotNull(TEXT("Plain byte property exists"), ByteProperty))
    {
        delete EnumProperty;
        return false;
    }
    TestNull(TEXT("Plain byte property has no enum"), ByteProperty->Enum);
    Error.Empty();
    TestFalse(
        TEXT("Fractional plain byte values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, ByteProperty, MakeShared<FJsonValueNumber>(1.5), Error));
    Error.Empty();
    TestFalse(
        TEXT("Malformed plain byte strings are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, ByteProperty, MakeShared<FJsonValueString>(TEXT("12x")), Error));
    Error.Empty();
    TestFalse(
        TEXT("Oversized plain byte values are rejected"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, ByteProperty, MakeShared<FJsonValueNumber>(9223372036854775808.0), Error));
    Error.Empty();
    TestTrue(
        TEXT("Valid plain byte strings remain supported"),
        McpPropertyReflection::ApplyJsonValueToProperty(
            Movement, ByteProperty, MakeShared<FJsonValueString>(TEXT("255")), Error));
    TestEqual(
        TEXT("Valid plain byte value is assigned"),
        Movement->CustomMovementMode, static_cast<uint8>(255));

    delete EnumProperty;
    return true;
}

#endif
