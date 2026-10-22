#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Data/PCGPointArrayData.h"
#include "Misc/Guid.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGInheritedDataTest, "EngineWire.PCG.Data.InheritedEntryKeysAndDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGInheritedDataTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointArrayData> Parent(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    TStrongObjectPtr<UPCGPointArrayData> Child(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    FPCGMetadataDomain* ParentDomain = Parent->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Parent element domain exists"), ParentDomain)) return false;
    Parent->MutableMetadata()->CreateInteger64Attribute(TEXT("Exact"), 9007199254740993LL, false, false);
    auto* Exact = ParentDomain->GetMutableTypedAttribute<int64>(TEXT("Exact"));
    if (!TestNotNull(TEXT("Parent integer attribute exists"), Exact)) return false;
    const PCGMetadataEntryKey ParentEntry = ParentDomain->AddEntry();
    Exact->SetValue(ParentEntry, MIN_int64);
    Child->MutableMetadata()->Initialize(Parent->ConstMetadata());
    FPCGMetadataDomain* ChildDomain = Child->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Child element domain exists"), ChildDomain)) return false;
    const PCGMetadataEntryKey Inherited = ChildDomain->AddEntry(ParentEntry);
    const PCGMetadataEntryKey Default = ChildDomain->AddEntry();
    const FPCGMetadataDomain* View = Child->ConstMetadata()->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
    const auto* Values = View ? View->GetConstTypedAttribute<int64>(TEXT("Exact")) : nullptr;
    if (!TestNotNull(TEXT("Child exposes the inherited typed attribute"), Values)) return false;
    TestTrue(TEXT("Child domain retains public parent identity"), View->GetParent() == ParentDomain);
    TestEqual(TEXT("A local row maps to its actual parent entry"), View->GetParentKey(Inherited), ParentEntry);
    TestEqual(TEXT("A local default row has no parent entry"), View->GetParentKey(Default), PCGInvalidEntryKey);
    TestEqual(TEXT("Inherited minimum Int64 remains exact"), Values->GetValueFromItemKey(Inherited), MIN_int64);
    TestEqual(TEXT("An unmapped local entry resolves the native default"), Values->GetValueFromItemKey(Default), 9007199254740993LL);
    TestEqual(TEXT("Invalid entry requests the native default"), Values->GetValueFromItemKey(PCGInvalidEntryKey), 9007199254740993LL);
    TestEqual(TEXT("Local entries and inherited entry capacity differ"), View->GetLocalItemCount(), static_cast<int64>(2));
    TestEqual(TEXT("Child-key capacity includes parent keys"), View->GetItemCountForChild(), ParentDomain->GetItemCountForChild() + 2);
    TestEqual(TEXT("Const child reads do not create parent entries"), ParentDomain->GetLocalItemCount(), static_cast<int64>(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataIdentityTest, "EngineWire.PCG.Data.ObjectIdentityIsNotContentRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataIdentityTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointArrayData> Points(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Points->SetNumPoints(1);
    Points->GetSeedValueRange()[0] = 41;
    const UPCGBasePointData* PointView = Points.Get();
    const uint64 PointUid = PointView->UID;
    TestEqual(TEXT("Initial native row is observable"), PointView->GetConstSeedValueRange()[0], 41);
    Points->GetSeedValueRange()[0] = 97;
    TestEqual(TEXT("Public in-place point edits preserve the object UID"), PointView->UID, PointUid);
    TestEqual(TEXT("Same UID and row count can expose different values"), PointView->GetConstSeedValueRange()[0], 97);
    TestEqual(TEXT("Row count is not a content revision either"), PointView->GetNumPoints(), 1);

    TStrongObjectPtr<UPCGParamData> Attributes(NewObject<UPCGParamData>(GetTransientPackage()));
    FPCGMetadataDomain* Domain = Attributes->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Owned attribute domain exists"), Domain)) return false;
    Attributes->MutableMetadata()->CreateInteger64Attribute(TEXT("Exact"), 0, false, false);
    auto* Exact = Domain->GetMutableTypedAttribute<int64>(TEXT("Exact"));
    if (!TestNotNull(TEXT("Owned integer attribute exists"), Exact)) return false;
    const PCGMetadataEntryKey Key = Domain->AddEntry();
    Exact->SetValue(Key, 9007199254740993LL);
    const UPCGData* DataView = Attributes.Get();
    const uint64 AttributeUid = DataView->UID;
    const int64 EntryCount = Domain->GetLocalItemCount();
    // CRC computation is permitted only on this test-owned fixture, not proposed as a production read.
    const FPCGCrc Before = DataView->GetOrComputeCrc(false);
    Exact->SetValue(Key, MIN_int64);
    const auto* ConstExact = DataView->ConstMetadata()->GetConstMetadataDomain(PCGMetadataDomainID::Elements)->GetConstTypedAttribute<int64>(TEXT("Exact"));
    if (!TestNotNull(TEXT("Mutated attribute remains readable"), ConstExact)) return false;
    TestEqual(TEXT("Public metadata edits preserve the data UID"), DataView->UID, AttributeUid);
    TestEqual(TEXT("Metadata content changes independently of data identity"), ConstExact->GetValueFromItemKey(Key), MIN_int64);
    TestEqual(TEXT("In-place attribute edits preserve the entry count"), Domain->GetLocalItemCount(), EntryCount);
    const FPCGCrc After = DataView->GetOrComputeCrc(false);
    if (!TestTrue(TEXT("Fixture produced valid native CRCs"), Before.IsValid() && After.IsValid())) return false;
    AddInfo(FString::Printf(TEXT("Public metadata edit: native CRC changed=%s; UID and entry count unchanged. CRC behavior is probe evidence, not a page-integrity guarantee."),
        Before == After ? TEXT("false") : TEXT("true")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGWeakDataTest, "EngineWire.PCG.Data.WeakDatasetIdentityDoesNotRetainOrRetarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGWeakDataTest::RunTest(const FString& Parameters)
{
    TWeakObjectPtr<UPCGParamData> Old;
    const FName Name(*FString::Printf(TEXT("McpPCGData_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FString Path;
    uint64 Uid = 0;
    {
        TStrongObjectPtr<UPCGParamData> Owned(NewObject<UPCGParamData>(GetTransientPackage(), Name, RF_Transient));
        Old = Owned.Get();
        Path = Owned->GetPathName();
        Uid = Owned->UID;
        TestTrue(TEXT("A live weak dataset reference resolves"), Old.IsValid());
    }
    CollectGarbage(RF_NoFlags);
    if (!TestFalse(TEXT("A weak dataset reference does not disable collection"), Old.IsValid())) return false;
    TStrongObjectPtr<UPCGParamData> Replacement(NewObject<UPCGParamData>(GetTransientPackage(), Name, RF_Transient));
    TestEqual(TEXT("Replacement can reuse the previous path"), Replacement->GetPathName(), Path);
    TestTrue(TEXT("Replacement receives a different object UID"), Replacement->UID != Uid);
    TestFalse(TEXT("The old weak reference cannot retarget the replacement"), Old.IsValid());
    return true;
}
#endif
