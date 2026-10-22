#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Domains/PCG/Data/McpPCGDataValue.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "PCGParamData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

namespace
{
TSharedPtr<FJsonValue> ExportValue(FAutomationTestBase& Test, const FPCGMetadataAttributeBase* Attribute, PCGMetadataEntryKey Key)
{
    TSharedPtr<FJsonValue> Value;
    FString Error;
    const bool bSuccess = McpPCGDataValue::ExportPCGDataValue(Attribute, Key, Value, Error);
    Test.TestTrue(TEXT("Typed metadata value exports successfully"), bSuccess);
    if (!bSuccess) Test.AddError(Error);
    return Value;
}

bool ExpectFailure(FAutomationTestBase& Test, const FPCGMetadataAttributeBase* Attribute, PCGMetadataEntryKey Key)
{
    TSharedPtr<FJsonValue> Value = MakeShared<FJsonValueNumber>(123.0);
    FString Error;
    const bool bSuccess = McpPCGDataValue::ExportPCGDataValue(Attribute, Key, Value, Error);
    Test.TestFalse(TEXT("Unsafe or unsupported metadata value is rejected"), bSuccess);
    Test.TestFalse(TEXT("Failed export clears the previous JSON value"), Value.IsValid());
    Test.TestTrue(TEXT("Failed export explains the refusal"), !Error.IsEmpty());
    return !bSuccess && !Value.IsValid() && !Error.IsEmpty();
}

double NumberField(FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    double Value = 0.0;
    const bool bFound = Object.IsValid() && Object->TryGetNumberField(Field, Value);
    Test.TestTrue(TEXT("Expected numeric JSON object field exists"), bFound);
    return Value;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataValueTypesTest, "EngineWire.PCG.Data.TypedValueProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataValueTypesTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    FPCGMetadataDomain* Domain = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Element metadata domain exists"), Domain)) return false;

    Metadata->CreateFloatAttribute(TEXT("Float"), 0.0f, false, false);
    Metadata->CreateDoubleAttribute(TEXT("Double"), 0.0, false, false);
    Metadata->CreateInteger32Attribute(TEXT("Int32"), 0, false, false);
    Metadata->CreateInteger64Attribute(TEXT("Int64"), 9007199254740991LL, false, false);
    Metadata->CreateVector2Attribute(TEXT("Vector2"), FVector2D::ZeroVector, false, false);
    Metadata->CreateVectorAttribute(TEXT("Vector"), FVector::ZeroVector, false, false);
    Metadata->CreateVector4Attribute(TEXT("Vector4"), FVector4(0, 0, 0, 0), false, false);
    Metadata->CreateQuatAttribute(TEXT("Quat"), FQuat::Identity, false, false);
    Metadata->CreateTransformAttribute(TEXT("Transform"), FTransform::Identity, false, false);
    Metadata->CreateStringAttribute(TEXT("String"), TEXT(""), false, false);
    Metadata->CreateBoolAttribute(TEXT("Bool"), false, false, false);
    Metadata->CreateRotatorAttribute(TEXT("Rotator"), FRotator::ZeroRotator, false, false);
    Metadata->CreateNameAttribute(TEXT("Name"), NAME_None, false, false);
    const FSoftObjectPath ObjectPath(TEXT("/Game/McpMissing.McpMissing"));
    const FSoftClassPath ClassPath(TEXT("/Game/McpMissing.McpMissing_C"));
    Metadata->CreateSoftObjectPathAttribute(TEXT("SoftObject"), ObjectPath, false, false);
    Metadata->CreateSoftClassPathAttribute(TEXT("SoftClass"), ClassPath, false, false);

    const PCGMetadataEntryKey Row = Domain->AddEntry();
    const PCGMetadataEntryKey AboveSafe = Domain->AddEntry();
    const PCGMetadataEntryKey Maximum = Domain->AddEntry();
    const PCGMetadataEntryKey Minimum = Domain->AddEntry();
    Domain->GetMutableTypedAttribute<float>(TEXT("Float"))->SetValue(Row, 1.25f);
    Domain->GetMutableTypedAttribute<double>(TEXT("Double"))->SetValue(Row, 1.2345678901234567);
    Domain->GetMutableTypedAttribute<int32>(TEXT("Int32"))->SetValue(Row, 17);
    Domain->GetMutableTypedAttribute<int64>(TEXT("Int64"))->SetValue(Row, -9007199254740991LL);
    Domain->GetMutableTypedAttribute<int64>(TEXT("Int64"))->SetValue(AboveSafe, 9007199254740992LL);
    Domain->GetMutableTypedAttribute<int64>(TEXT("Int64"))->SetValue(Maximum, MAX_int64);
    Domain->GetMutableTypedAttribute<int64>(TEXT("Int64"))->SetValue(Minimum, MIN_int64);
    Domain->GetMutableTypedAttribute<FVector2D>(TEXT("Vector2"))->SetValue(Row, FVector2D(1.0, 2.0));
    Domain->GetMutableTypedAttribute<FVector>(TEXT("Vector"))->SetValue(Row, FVector(1.2345678901234567, 4.0, 5.0));
    Domain->GetMutableTypedAttribute<FVector4>(TEXT("Vector4"))->SetValue(Row, FVector4(6.0, 7.0, 8.0, 9.0));
    Domain->GetMutableTypedAttribute<FQuat>(TEXT("Quat"))->SetValue(Row, FQuat(0.0, 0.0, 0.12345678901234567, 1.0));
    const FTransform ExpectedTransform(FRotator(10.0, 20.0, 30.0), FVector(1234567890.1234567, 12.0, 13.0), FVector(2.0, 3.0, 4.0));
    Domain->GetMutableTypedAttribute<FTransform>(TEXT("Transform"))->SetValue(Row, ExpectedTransform);
    Domain->GetMutableTypedAttribute<FString>(TEXT("String"))->SetValue(Row, TEXT("sample"));
    Domain->GetMutableTypedAttribute<bool>(TEXT("Bool"))->SetValue(Row, true);
    Domain->GetMutableTypedAttribute<FRotator>(TEXT("Rotator"))->SetValue(Row, FRotator(15.0, 25.0, 35.0));
    Domain->GetMutableTypedAttribute<FName>(TEXT("Name"))->SetValue(Row, FName(TEXT("sample_name")));

    const TSharedPtr<FJsonValue> FloatValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Float")), Row);
    const TSharedPtr<FJsonValue> DoubleValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Double")), Row);
    const TSharedPtr<FJsonValue> Int32Value = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int32")), Row);
    const TSharedPtr<FJsonValue> SafeInt64 = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int64")), PCGInvalidEntryKey);
    const TSharedPtr<FJsonValue> NegativeSafeInt64 = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int64")), Row);
    const TSharedPtr<FJsonValue> AboveSafeInt64 = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int64")), AboveSafe);
    const TSharedPtr<FJsonValue> MaxInt64 = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int64")), Maximum);
    const TSharedPtr<FJsonValue> MinInt64 = ExportValue(*this, Domain->GetConstAttribute(TEXT("Int64")), Minimum);
    if (!TestTrue(TEXT("All scalar exports return JSON values"), FloatValue.IsValid() && DoubleValue.IsValid() && Int32Value.IsValid() &&
        SafeInt64.IsValid() && NegativeSafeInt64.IsValid() && AboveSafeInt64.IsValid() && MaxInt64.IsValid() && MinInt64.IsValid())) return false;
    TestEqual(TEXT("Float value is retained"), FloatValue->AsNumber(), 1.25);
    TestEqual(TEXT("17-digit double value is not rounded for display"), DoubleValue->AsNumber(), 1.2345678901234567);
    TestEqual(TEXT("Int32 remains a JSON number"), Int32Value->AsNumber(), 17.0);
    TestEqual(TEXT("Positive safe Int64 remains a JSON number"), SafeInt64->AsNumber(), 9007199254740991.0);
    TestEqual(TEXT("Negative safe Int64 remains a JSON number"), NegativeSafeInt64->AsNumber(), -9007199254740991.0);
    TestEqual(TEXT("Int64 above JSON safe range is exact decimal text"), AboveSafeInt64->AsString(), FString(TEXT("9007199254740992")));
    TestEqual(TEXT("Int64 maximum is exact decimal text"), MaxInt64->AsString(), FString(TEXT("9223372036854775807")));
    TestEqual(TEXT("Int64 minimum is exact decimal text"), MinInt64->AsString(), FString(TEXT("-9223372036854775808")));

    const TSharedPtr<FJsonValue> Vector2Value = ExportValue(*this, Domain->GetConstAttribute(TEXT("Vector2")), Row);
    const TSharedPtr<FJsonValue> VectorValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Vector")), Row);
    const TSharedPtr<FJsonValue> Vector4Value = ExportValue(*this, Domain->GetConstAttribute(TEXT("Vector4")), Row);
    const TSharedPtr<FJsonValue> QuatValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Quat")), Row);
    if (!TestTrue(TEXT("Coordinate values use JSON objects"), Vector2Value.IsValid() && Vector2Value->Type == EJson::Object &&
        VectorValue.IsValid() && VectorValue->Type == EJson::Object && Vector4Value.IsValid() && Vector4Value->Type == EJson::Object &&
        QuatValue.IsValid() && QuatValue->Type == EJson::Object)) return false;
    TestEqual(TEXT("Vector2 x field"), NumberField(*this, Vector2Value->AsObject(), TEXT("x")), 1.0);
    TestEqual(TEXT("Vector2 y field"), NumberField(*this, Vector2Value->AsObject(), TEXT("y")), 2.0);
    TestEqual(TEXT("Fine vector component is not rounded for display"), NumberField(*this, VectorValue->AsObject(), TEXT("x")), 1.2345678901234567);
    TestEqual(TEXT("Vector z field"), NumberField(*this, VectorValue->AsObject(), TEXT("z")), 5.0);
    TestEqual(TEXT("Vector4 w field"), NumberField(*this, Vector4Value->AsObject(), TEXT("w")), 9.0);
    TestEqual(TEXT("Fine quaternion component is not rounded for display"), NumberField(*this, QuatValue->AsObject(), TEXT("z")), 0.12345678901234567);
    TestEqual(TEXT("Quaternion w component"), NumberField(*this, QuatValue->AsObject(), TEXT("w")), 1.0);

    const TSharedPtr<FJsonValue> TransformValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Transform")), Row);
    if (!TestTrue(TEXT("Transform uses a structured JSON object"), TransformValue.IsValid() && TransformValue->Type == EJson::Object)) return false;
    TSharedPtr<FJsonObject> TransformObject = TransformValue->AsObject();
    const TSharedPtr<FJsonObject>* Location = nullptr;
    const TSharedPtr<FJsonObject>* Rotation = nullptr;
    const TSharedPtr<FJsonObject>* Scale = nullptr;
    if (!TestTrue(TEXT("Transform contains location, rotation, and scale"), TransformObject->TryGetObjectField(TEXT("location"), Location) &&
        TransformObject->TryGetObjectField(TEXT("rotation"), Rotation) && TransformObject->TryGetObjectField(TEXT("scale"), Scale))) return false;
    TestEqual(TEXT("Transform translation keeps its full double precision"), NumberField(*this, *Location, TEXT("x")), 1234567890.1234567);
    TestEqual(TEXT("Transform rotation yaw matches native quaternion conversion"), NumberField(*this, *Rotation, TEXT("yaw")),
        ExpectedTransform.GetRotation().Rotator().Yaw);
    TestEqual(TEXT("Transform scale z"), NumberField(*this, *Scale, TEXT("z")), 4.0);

    const TSharedPtr<FJsonValue> StringValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("String")), Row);
    const TSharedPtr<FJsonValue> BoolValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Bool")), Row);
    const TSharedPtr<FJsonValue> RotatorValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Rotator")), Row);
    const TSharedPtr<FJsonValue> NameValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("Name")), Row);
    const TSharedPtr<FJsonValue> SoftObjectValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("SoftObject")), PCGInvalidEntryKey);
    const TSharedPtr<FJsonValue> SoftClassValue = ExportValue(*this, Domain->GetConstAttribute(TEXT("SoftClass")), PCGInvalidEntryKey);
    if (!TestTrue(TEXT("Text, boolean, and soft-path exports are present"), StringValue.IsValid() && BoolValue.IsValid() &&
        RotatorValue.IsValid() && RotatorValue->Type == EJson::Object && NameValue.IsValid() && SoftObjectValue.IsValid() && SoftClassValue.IsValid())) return false;
    TestEqual(TEXT("String value"), StringValue->AsString(), FString(TEXT("sample")));
    TestTrue(TEXT("Boolean value"), BoolValue->AsBool());
    TestEqual(TEXT("Rotator keeps pitch/yaw/roll names"), NumberField(*this, RotatorValue->AsObject(), TEXT("pitch")), 15.0);
    TestEqual(TEXT("Name exports as text"), NameValue->AsString(), FString(TEXT("sample_name")));
    TestEqual(TEXT("Soft object reference exports its path"), SoftObjectValue->AsString(), ObjectPath.ToString());
    TestEqual(TEXT("Soft class reference exports its path"), SoftClassValue->AsString(), ClassPath.ToString());
    TestNull(TEXT("Soft object remains unresolved after reading"), ObjectPath.ResolveObject());
    TestNull(TEXT("Soft class path remains unresolved after reading"), ClassPath.ResolveObject());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataValueInheritanceTest, "EngineWire.PCG.Data.TypedValueDefaultsAndInheritance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataValueInheritanceTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointArrayData> Parent(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    TStrongObjectPtr<UPCGPointArrayData> Child(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    FPCGMetadataDomain* ParentDomain = Parent->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Parent element domain exists"), ParentDomain)) return false;
    Parent->MutableMetadata()->CreateInteger64Attribute(TEXT("Exact"), 9007199254740991LL, false, false);
    auto* MutableExact = ParentDomain->GetMutableTypedAttribute<int64>(TEXT("Exact"));
    if (!TestNotNull(TEXT("Parent integer attribute exists"), MutableExact)) return false;
    const PCGMetadataEntryKey ParentEntry = ParentDomain->AddEntry();
    MutableExact->SetValue(ParentEntry, MIN_int64);
    Child->MutableMetadata()->Initialize(Parent->ConstMetadata());
    FPCGMetadataDomain* ChildDomain = Child->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Child element domain exists"), ChildDomain)) return false;
    const PCGMetadataEntryKey InheritedEntry = ChildDomain->AddEntry(ParentEntry);
    const PCGMetadataEntryKey DefaultEntry = ChildDomain->AddEntry();
    const FPCGMetadataDomain* View = Child->ConstMetadata()->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
    const FPCGMetadataAttributeBase* Exact = View ? View->GetConstAttribute(TEXT("Exact")) : nullptr;
    if (!TestNotNull(TEXT("Child exposes inherited metadata"), Exact)) return false;

    const TSharedPtr<FJsonValue> Inherited = ExportValue(*this, Exact, InheritedEntry);
    const TSharedPtr<FJsonValue> Default = ExportValue(*this, Exact, DefaultEntry);
    const TSharedPtr<FJsonValue> InvalidKeyDefault = ExportValue(*this, Exact, PCGInvalidEntryKey);
    if (!TestTrue(TEXT("Inherited and default exports are present"), Inherited.IsValid() && Default.IsValid() && InvalidKeyDefault.IsValid())) return false;
    TestEqual(TEXT("Inherited minimum Int64 stays exact"), Inherited->AsString(), FString(TEXT("-9223372036854775808")));
    TestEqual(TEXT("Unmapped child row resolves its metadata default"), Default->AsNumber(), 9007199254740991.0);
    TestEqual(TEXT("Invalid entry resolves its metadata default"), InvalidKeyDefault->AsNumber(), 9007199254740991.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataValueFailureTest, "EngineWire.PCG.Data.TypedValueRejectsInvalidAndNonFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataValueFailureTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointData> Data(NewObject<UPCGPointData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    FPCGMetadataDomain* Domain = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Element domain exists"), Domain)) return false;
    const PCGMetadataEntryKey Key = Domain->AddEntry();
    Metadata->CreateFloatAttribute(TEXT("Float"), 0.0f, false, false);
    Metadata->CreateDoubleAttribute(TEXT("Double"), 0.0, false, false);
    Metadata->CreateVectorAttribute(TEXT("Vector"), FVector::ZeroVector, false, false);
    Domain->GetMutableTypedAttribute<float>(TEXT("Float"))->SetValue(Key, std::numeric_limits<float>::quiet_NaN());
    Domain->GetMutableTypedAttribute<double>(TEXT("Double"))->SetValue(Key, std::numeric_limits<double>::infinity());
    Domain->GetMutableTypedAttribute<FVector>(TEXT("Vector"))->SetValue(Key, FVector(1.0, std::numeric_limits<double>::quiet_NaN(), 3.0));
    if (!ExpectFailure(*this, Domain->GetConstAttribute(TEXT("Float")), Key) ||
        !ExpectFailure(*this, Domain->GetConstAttribute(TEXT("Double")), Key) ||
        !ExpectFailure(*this, Domain->GetConstAttribute(TEXT("Vector")), Key)) return false;
    if (!ExpectFailure(*this, nullptr, PCGInvalidEntryKey)) return false;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    FPCGMetadataAttributeDesc UnsupportedDesc;
    UnsupportedDesc.Name = TEXT("UnsupportedGenericType");
    UnsupportedDesc.ValueType = EPCGMetadataTypes::Byte;
    const FPCGMetadataAttributeBase* Unsupported = Metadata->CreateAttribute(
        FPCGAttributeIdentifier(UnsupportedDesc.Name), UnsupportedDesc, false, false);
    if (!TestNotNull(TEXT("Generic unsupported attribute fixture is created through the public descriptor API"), Unsupported)) return false;
    if (!ExpectFailure(*this, Unsupported, PCGInvalidEntryKey)) return false;
    FPCGMetadataAttributeDesc ArrayDesc;
    ArrayDesc.Name = TEXT("UnsupportedScalarContainer");
    ArrayDesc.ValueType = EPCGMetadataTypes::Double;
    ArrayDesc.ContainerTypes.Add(EPCGMetadataAttributeContainerTypes::Array);
    const FPCGMetadataAttributeBase* Array = Metadata->CreateAttribute(
        FPCGAttributeIdentifier(ArrayDesc.Name), ArrayDesc, false, false);
    if (!TestNotNull(TEXT("Public descriptor factory creates a Double array"), Array)) return false;
    if (!ExpectFailure(*this, Array, PCGInvalidEntryKey)) return false;
#endif
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataValueStringBudgetTest, "EngineWire.PCG.Data.TypedValueEnforcesStringBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataValueStringBudgetTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    FPCGMetadataDomain* Domain = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Element domain exists"), Domain)) return false;
    const FString LongText = FString::ChrN(1025, TEXT('x'));
    const FString LongSubPath = FString::ChrN(1025, TEXT('s'));
    const FString LongObjectPathText = FString::Printf(TEXT("/Game/McpMissing.McpMissing:%s"), *LongSubPath);
    const FString LongClassPathText = FString::Printf(TEXT("/Game/McpMissing.McpMissing_C:%s"), *LongSubPath);
    const FString MaxNameText = FString::ChrN(NAME_SIZE - 1, TEXT('n'));
    const FName MaxName(*MaxNameText);
    Metadata->CreateStringAttribute(TEXT("LongString"), LongText, false, false);
    Metadata->CreateNameAttribute(TEXT("MaxName"), MaxName, false, false);
    Metadata->CreateSoftObjectPathAttribute(TEXT("LongObjectPath"), FSoftObjectPath(LongObjectPathText), false, false);
    Metadata->CreateSoftClassPathAttribute(TEXT("LongClassPath"), FSoftClassPath(LongClassPathText), false, false);
    const FPCGMetadataAttributeBase* LongString = Domain->GetConstAttribute(TEXT("LongString"));
    const FPCGMetadataAttributeBase* MaximumName = Domain->GetConstAttribute(TEXT("MaxName"));
    const FPCGMetadataAttributeBase* LongObjectPath = Domain->GetConstAttribute(TEXT("LongObjectPath"));
    const FPCGMetadataAttributeBase* LongClassPath = Domain->GetConstAttribute(TEXT("LongClassPath"));
    if (!TestNotNull(TEXT("String fixture exists"), LongString) || !TestNotNull(TEXT("Maximum legal name fixture exists"), MaximumName) ||
        !TestNotNull(TEXT("Soft object path fixture exists"), LongObjectPath) || !TestNotNull(TEXT("Soft class path fixture exists"), LongClassPath)) return false;
    // FName's fixed NAME_SIZE buffer includes its terminator; use its maximum legal text length rather than a fatal overlong fixture.
    const TSharedPtr<FJsonValue> NameValue = ExportValue(*this, MaximumName, PCGInvalidEntryKey);
    if (!TestTrue(TEXT("Maximum legal FName is exported"), NameValue.IsValid() && NameValue->Type == EJson::String)) return false;
    TestEqual(TEXT("Maximum legal FName text is preserved"), NameValue->AsString(), MaxName.ToString());
    TestTrue(TEXT("Maximum legal FName stays within the native name buffer"), MaxName.ToString().Len() < NAME_SIZE);
    TestTrue(TEXT("Soft object path fixture has an over-budget subobject path"), FSoftObjectPath(LongObjectPathText).ToString().Len() > 1024);
    TestTrue(TEXT("Soft class path fixture has an over-budget subobject path"), FSoftClassPath(LongClassPathText).ToString().Len() > 1024);
    if (!ExpectFailure(*this, LongString, PCGInvalidEntryKey) || !ExpectFailure(*this, LongObjectPath, PCGInvalidEntryKey) ||
        !ExpectFailure(*this, LongClassPath, PCGInvalidEntryKey)) return false;
    return true;
}
#endif
