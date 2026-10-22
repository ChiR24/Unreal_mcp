#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/Schema/McpPCGPropSchema.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Data/PCGPointData.h"
#include "PCGPoint.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "PCGDataAsset.h"
#include "MeshSelectors/PCGISMDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Engine/StaticMesh.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
TSharedPtr<FJsonObject> ReadPCGPropSchemaPage(FAutomationTestBase& Test, const UStruct* Scope, int32 Offset, int32 Limit)
{
    TSharedPtr<FJsonObject> Schema;
    FString Error;
    const bool bSuccess = McpPCGHandlers::DescribePCGPropertySchema(Scope, Offset, Limit, Schema, Error);
    Test.TestTrue(TEXT("Bounded reflected property schema succeeds"), bSuccess);
    if (!bSuccess) Test.AddError(Error);
    return Schema;
}

TSharedPtr<FJsonObject> FindPCGPropSchemaItem(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
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

TSharedPtr<FJsonObject> FindPCGPropSchemaItemAcrossPages(FAutomationTestBase& Test, const UStruct* Scope, const FString& Name)
{
    TSharedPtr<FJsonObject> Page = ReadPCGPropSchemaPage(Test, Scope, 0, 32);
    if (!Page.IsValid()) return nullptr;
    if (const TSharedPtr<FJsonObject> Item = FindPCGPropSchemaItem(Page, Name)) return Item;
    const int32 Total = static_cast<int32>(Page->GetNumberField(TEXT("total")));
    for (int32 Offset = 32; Offset < Total; Offset += 32)
    {
        Page = ReadPCGPropSchemaPage(Test, Scope, Offset, 32);
        if (!Page.IsValid()) return nullptr;
        if (const TSharedPtr<FJsonObject> Item = FindPCGPropSchemaItem(Page, Name)) return Item;
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FindPCGPropSchemaEnumOption(const TSharedPtr<FJsonObject>& Details, const FString& Name)
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaNativeMetadataTest, "EngineWire.PCG.PropSchema.NativeFlagsAndClassConstraints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaNativeMetadataTest::RunTest(const FString& Parameters)
{
    const UStruct* Scope = UPCGStaticMeshSpawnerSettings::StaticClass();
    const bool bPackageDirty = Scope->GetOutermost()->IsDirty();
    const TSharedPtr<FJsonObject> Schema = ReadPCGPropSchemaPage(*this, Scope, 0, 32);
    if (!TestTrue(TEXT("Native spawner schema is returned"), Schema.IsValid())) return false;
    TestTrue(TEXT("Schema explicitly describes metadata rather than write support"), Schema->GetBoolField(TEXT("metadataOnly")));
    const TSharedPtr<FJsonObject> ClassRef = FindPCGPropSchemaItemAcrossPages(*this, Scope, TEXT("MeshSelectorType"));
    const TSharedPtr<FJsonObject> Instanced = FindPCGPropSchemaItemAcrossPages(*this, Scope, TEXT("MeshSelectorParameters"));
    if (!TestTrue(TEXT("Native class-reference and instanced properties are found"), ClassRef.IsValid() && Instanced.IsValid())) return false;
    TestEqual(TEXT("Class property type name is precise"), ClassRef->GetStringField(TEXT("type")), FString(TEXT("Class")));
    const TSharedPtr<FJsonObject> ClassDetails = ClassRef->GetObjectField(TEXT("typeDetails"));
    TestEqual(TEXT("Class reference kind is explicit"), ClassDetails->GetStringField(TEXT("referenceKind")), FString(TEXT("class")));
    TestTrue(TEXT("Class constraint path is available without resolving a value"), ClassDetails->GetBoolField(TEXT("classConstraintAvailable")));
    TestEqual(TEXT("Class constraint is the PCG selector base"), ClassDetails->GetStringField(TEXT("classConstraintPath")), UPCGMeshSelectorBase::StaticClass()->GetPathName());
    TestTrue(TEXT("Visible instanced property is marked read-only"), Instanced->GetBoolField(TEXT("readOnly")));
    TestTrue(TEXT("Visible instanced property retains edit metadata"), Instanced->GetBoolField(TEXT("editable")));
    TestTrue(TEXT("Visible instanced property retains instancing flag"), Instanced->GetBoolField(TEXT("instancedReference")));
#if WITH_EDITORONLY_DATA
    const TSharedPtr<FJsonObject> SoftClassRef = FindPCGPropSchemaItemAcrossPages(*this, UPCGDataAsset::StaticClass(), TEXT("ExporterClass"));
    if (!TestTrue(TEXT("Native soft-class property exists"), SoftClassRef.IsValid())) return false;
    const TSharedPtr<FJsonObject> SoftClassDetails = SoftClassRef->GetObjectField(TEXT("typeDetails"));
    TestEqual(TEXT("Soft-class type is precise"), SoftClassRef->GetStringField(TEXT("type")), FString(TEXT("SoftClass")));
    TestEqual(TEXT("Soft-class constraint comes from its native property class"), SoftClassDetails->GetStringField(TEXT("classConstraintPath")), UObject::StaticClass()->GetPathName());
    TestTrue(TEXT("Soft-class reference remains marked soft"), SoftClassDetails->GetBoolField(TEXT("softReference")));
#endif
    TestEqual(TEXT("Schema reflection does not dirty the class package"), Scope->GetOutermost()->IsDirty(), bPackageDirty);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaNestedMetadataTest, "EngineWire.PCG.PropSchema.ArraysStructsEnumsAndSoftReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaNestedMetadataTest::RunTest(const FString& Parameters)
{
    const TSharedPtr<FJsonObject> MeshEntries = FindPCGPropSchemaItemAcrossPages(*this, UPCGMeshSelectorWeighted::StaticClass(), TEXT("MeshEntries"));
    if (!TestTrue(TEXT("Weighted mesh array exists"), MeshEntries.IsValid())) return false;
    TestEqual(TEXT("Container type helper names arrays"), MeshEntries->GetStringField(TEXT("type")), FString(TEXT("Array")));
    const TSharedPtr<FJsonObject> ArrayDetails = MeshEntries->GetObjectField(TEXT("typeDetails"));
    TestEqual(TEXT("Array shape is explicit"), ArrayDetails->GetStringField(TEXT("container")), FString(TEXT("array")));
    const TSharedPtr<FJsonObject> Entry = ArrayDetails->GetObjectField(TEXT("element"));
    TestEqual(TEXT("Array element is reflected as a struct"), Entry->GetStringField(TEXT("structPath")), FPCGMeshSelectorWeightedEntry::StaticStruct()->GetPathName());

    const TSharedPtr<FJsonObject> Transform = FindPCGPropSchemaItemAcrossPages(*this, FPCGPoint::StaticStruct(), TEXT("Transform"));
    if (!TestTrue(TEXT("Point transform struct field exists"), Transform.IsValid())) return false;
    TestEqual(TEXT("Struct type path is reported"), Transform->GetObjectField(TEXT("typeDetails"))->GetStringField(TEXT("structPath")), TBaseStructure<FTransform>::Get()->GetPathName());

    const TSharedPtr<FJsonObject> Coordinate = FindPCGPropSchemaItemAcrossPages(*this, UPCGCreatePointsSettings::StaticClass(), TEXT("CoordinateSpace"));
    if (!TestTrue(TEXT("Create Points native enum property exists"), Coordinate.IsValid())) return false;
    const TSharedPtr<FJsonObject> EnumDetails = Coordinate->GetObjectField(TEXT("typeDetails"));
    TestTrue(TEXT("Enum path is native and explicit"), EnumDetails->GetStringField(TEXT("enumPath")).EndsWith(TEXT("EPCGCoordinateSpace")));
    const TSharedPtr<FJsonObject> World = FindPCGPropSchemaEnumOption(EnumDetails, TEXT("World"));
    const TSharedPtr<FJsonObject> Original = FindPCGPropSchemaEnumOption(EnumDetails, TEXT("OriginalComponent"));
    const TSharedPtr<FJsonObject> Local = FindPCGPropSchemaEnumOption(EnumDetails, TEXT("LocalComponent"));
    if (!TestTrue(TEXT("All native coordinate options are present"), World.IsValid() && Original.IsValid() && Local.IsValid())) return false;
    TestEqual(TEXT("World enum numeric value is exact"), World->GetNumberField(TEXT("value")), 0.0);
    TestEqual(TEXT("Original component enum numeric value is exact"), Original->GetNumberField(TEXT("value")), 1.0);
    TestEqual(TEXT("Local component enum numeric value is exact"), Local->GetNumberField(TEXT("value")), 2.0);
    TestEqual(TEXT("Native visible enum option total is bounded and honest"), EnumDetails->GetNumberField(TEXT("enumOptionTotal")), 3.0);
    TestEqual(TEXT("Enum entry total partitions visible, hidden, and max options"), EnumDetails->GetNumberField(TEXT("enumEntryTotal")),
        EnumDetails->GetNumberField(TEXT("enumOptionTotal")) + EnumDetails->GetNumberField(TEXT("hiddenEnumOptionTotal")) +
        EnumDetails->GetNumberField(TEXT("maxEnumOptionTotal")));
    TestEqual(TEXT("Native terminal max is counted separately"), EnumDetails->GetNumberField(TEXT("maxEnumOptionTotal")), 1.0);
    TestTrue(TEXT("Native terminal max omission is explicit"), EnumDetails->GetBoolField(TEXT("maxEnumOptionsOmitted")));
    const TArray<TSharedPtr<FJsonValue>>& UsableOptions = EnumDetails->GetArrayField(TEXT("enumOptions"));
    TestTrue(TEXT("Enum options never exceed the bounded page"), UsableOptions.Num() <= 64);
    for (const TSharedPtr<FJsonValue>& OptionValue : UsableOptions)
    {
        const FString OptionName = OptionValue->AsObject()->GetStringField(TEXT("name"));
        TestFalse(TEXT("Terminal max sentinel is absent from usable options"), OptionName == TEXT("MAX") ||
            OptionName.EndsWith(TEXT("_MAX"), ESearchCase::CaseSensitive));
    }
    TestFalse(TEXT("The small native enum option list is not truncated"), EnumDetails->GetBoolField(TEXT("enumOptionsTruncated")));

    const TSharedPtr<FJsonObject> StaticMesh = FindPCGPropSchemaItemAcrossPages(*this, FPCGSoftISMComponentDescriptor::StaticStruct(), TEXT("StaticMesh"));
    if (!TestTrue(TEXT("Native soft mesh reference property exists"), StaticMesh.IsValid())) return false;
    const TSharedPtr<FJsonObject> SoftDetails = StaticMesh->GetObjectField(TEXT("typeDetails"));
    TestEqual(TEXT("Soft object reference kind is explicit"), SoftDetails->GetStringField(TEXT("referenceKind")), FString(TEXT("softObject")));
    TestTrue(TEXT("Soft object references are identified without loading"), SoftDetails->GetBoolField(TEXT("softReference")));
    TestEqual(TEXT("Soft object constraint is path-only"), SoftDetails->GetStringField(TEXT("classConstraintPath")), UStaticMesh::StaticClass()->GetPathName());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPropSchemaPagingValidationTest, "EngineWire.PCG.PropSchema.SortedPagesInvalidInputsAndNoMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPropSchemaPagingValidationTest::RunTest(const FString& Parameters)
{
    const UStruct* Scope = FPCGMeshSelectorWeightedEntry::StaticStruct();
    const bool bDirty = Scope->GetOutermost()->IsDirty();
    const TSharedPtr<FJsonObject> First = ReadPCGPropSchemaPage(*this, Scope, 0, 1);
    const TSharedPtr<FJsonObject> Second = ReadPCGPropSchemaPage(*this, Scope, 1, 1);
    if (!TestTrue(TEXT("Native struct pages are returned"), First.IsValid() && Second.IsValid())) return false;
    TestEqual(TEXT("Total eligible properties is reported"), First->GetNumberField(TEXT("total")), 2.0);
    TestEqual(TEXT("Offset and limit are reported"), First->GetNumberField(TEXT("offset")), 0.0);
    TestEqual(TEXT("Requested page limit is reported"), First->GetNumberField(TEXT("limit")), 1.0);
    TestTrue(TEXT("First page reports truncation"), First->GetBoolField(TEXT("truncated")));
    TestFalse(TEXT("Final page is complete"), Second->GetBoolField(TEXT("truncated")));
    const TSharedPtr<FJsonObject> FirstItem = FindPCGPropSchemaItem(First, TEXT("Descriptor"));
    const TSharedPtr<FJsonObject> SecondItem = FindPCGPropSchemaItem(Second, TEXT("Weight"));
    if (!TestTrue(TEXT("Both ordered native properties exist"), FirstItem.IsValid() && SecondItem.IsValid())) return false;
    TestEqual(TEXT("Names are sorted case-sensitively"), FirstItem->GetStringField(TEXT("name")), FString(TEXT("Descriptor")));
    TestEqual(TEXT("Second page follows lexical order"), SecondItem->GetStringField(TEXT("name")), FString(TEXT("Weight")));

    TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
    FString Error;
    TestFalse(TEXT("Null scope is refused"), McpPCGHandlers::DescribePCGPropertySchema(nullptr, 0, 1, Failure, Error));
    TestFalse(TEXT("Invalid scope clears previous output"), Failure.IsValid());
    TestFalse(TEXT("Negative offset is refused"), McpPCGHandlers::DescribePCGPropertySchema(Scope, -1, 1, Failure, Error));
    TestFalse(TEXT("Zero page size is refused"), McpPCGHandlers::DescribePCGPropertySchema(Scope, 0, 0, Failure, Error));
    TestFalse(TEXT("Page size above 32 is refused"), McpPCGHandlers::DescribePCGPropertySchema(Scope, 0, 33, Failure, Error));
    const TSharedPtr<FJsonObject> PastEnd = ReadPCGPropSchemaPage(*this, Scope, 20, 1);
    if (!TestTrue(TEXT("Off-end page is a valid empty page"), PastEnd.IsValid())) return false;
    TestEqual(TEXT("Off-end page has no items"), PastEnd->GetArrayField(TEXT("items")).Num(), 0);
    TestFalse(TEXT("Off-end page is not truncated"), PastEnd->GetBoolField(TEXT("truncated")));
    TestEqual(TEXT("Metadata inspection does not dirty the reflected package"), Scope->GetOutermost()->IsDirty(), bDirty);
    return true;
}
#endif
