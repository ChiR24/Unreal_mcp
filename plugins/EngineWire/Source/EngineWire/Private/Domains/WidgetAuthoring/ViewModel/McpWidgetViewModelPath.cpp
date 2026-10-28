// Viewmodel and binding rows, and a binding's paths as text ("Health.Current" -> "HealthBar.Percent") both ways (see
// McpWidgetViewModel.h). A path is written as FMVVMBlueprintFieldPath's constructor writes one: each field a member
// reference, self when the Widget Blueprint's own class has it, else external on its owner (a Blueprint owner's
// skeleton class), with the guid a Blueprint gave the field.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace McpWidgetViewModel
{
namespace
{
// FMemberReference's fields are protected; a struct owner is set the way the engine's own MVVM path sets one.
struct FStructMemberReference : public FMemberReference
{
	void Set(FName Name, const UStruct* Owner, const FGuid& Guid)
	{
		MemberName = Name;
		MemberGuid = Guid;
		MemberParent = const_cast<UStruct*>(Owner);
		MemberScope.Empty();
		bSelfContext = false;
		bWasDeprecated = false;
	}
};

FMemberReference* ReferenceOf(const UStruct* FieldStruct, void* Field)
{
	const FStructProperty* Reference = CastField<FStructProperty>(FieldOf(FieldStruct, TEXT("BindingReference")));
	return Reference && Reference->Struct == FMemberReference::StaticStruct() ? Reference->ContainerPtrToValuePtr<FMemberReference>(Field) : nullptr;
}

// Fields of a Blueprint class are found on its skeleton, which has them before the Blueprint compiles.
const UStruct* SkeletonOf(const UStruct* Owner)
{
	const UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UClass>(Owner));
	return Blueprint && Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass : Owner;
}

// A class or struct by the name a person knows it by (a Blueprint's, not its skeleton class's), and a Blueprint's own
// variables to choose from.
FString OwnerText(const UStruct* Owner)
{
	const UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UClass>(Owner));
	if (!Blueprint)
	{
		return Owner->GetName();
	}
	TArray<FString> Variables;
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		Variables.Add(Variable.VarName.ToString());
	}
	return Variables.Num() ? FString::Printf(TEXT("%s (its variables: %s)"), *Blueprint->GetName(), *FString::Join(Variables, TEXT(", "))) : Blueprint->GetName();
}

const UStruct* NextOwner(const FProperty* Value)
{
	if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Value))
	{
		return Object->PropertyClass;
	}
	const FStructProperty* Struct = CastField<FStructProperty>(Value);
	return Struct ? Struct->Struct : nullptr;
}

FString Names(UWidgetBlueprint* Widget, UObject* View)
{
	TArray<FString> Out;
	const FViewArray Models = ViewArray(View, TEXT("AvailableViewModels"));
	for (int32 Index = 0; Index < Models.Num(); ++Index)
	{
		Out.Add(ReadName(Models.Struct, Models.At(Index), TEXT("ViewModelName")).ToString());
	}
	if (Widget->WidgetTree)
	{
		Widget->WidgetTree->ForEachWidget([&Out](UWidget* Child) { if (Child && Out.Num() < 40) Out.Add(Child->GetName()); });
	}
	return Out.Num() ? FString::Join(Out, TEXT(", ")) + TEXT(", Self") : FString(TEXT("Self"));
}
} // namespace

TSharedPtr<FJsonObject> ViewModelRow(const FViewArray& Models, int32 Index)
{
	const uint8* Context = Models.At(Index);
	const UObject* Class = ReadObject(Models.Struct, Context, TEXT("NotifyFieldValueClass"));
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("name"), ReadName(Models.Struct, Context, TEXT("ViewModelName")).ToString());
	Row->SetStringField(TEXT("class"), Class ? Class->GetPathName() : FString());
	Row->SetStringField(TEXT("id"), ReadGuid(Models.Struct, Context, TEXT("ViewModelContextId")).ToString(EGuidFormats::DigitsWithHyphens));
	Row->SetStringField(TEXT("creation"), ReadEnum(Models.Struct, Context, TEXT("CreationType")));
	return Row;
}

TSharedPtr<FJsonObject> BindingRow(UObject* View, const FViewArray& Bindings, int32 Index)
{
	uint8* Binding = Bindings.At(Index);
	const FStructProperty* Source = CastField<FStructProperty>(FieldOf(Bindings.Struct, TEXT("SourcePath")));
	const FStructProperty* Conversion = CastField<FStructProperty>(FieldOf(Bindings.Struct, TEXT("Conversion")));
	const void* ConversionValue = ValuePtr(Bindings.Struct, Binding, TEXT("Conversion"));
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("id"), ReadGuid(Bindings.Struct, Binding, TEXT("BindingId")).ToString(EGuidFormats::DigitsWithHyphens));
	Row->SetStringField(TEXT("source"), Source ? PathText(View, Source->Struct, ValuePtr(Bindings.Struct, Binding, TEXT("SourcePath"))) : FString());
	Row->SetStringField(TEXT("destination"), Source ? PathText(View, Source->Struct, ValuePtr(Bindings.Struct, Binding, TEXT("DestinationPath"))) : FString());
	Row->SetStringField(TEXT("mode"), ModeName(ReadEnum(Bindings.Struct, Binding, TEXT("BindingType"))));
	Row->SetBoolField(TEXT("enabled"), ReadBool(Bindings.Struct, Binding, TEXT("bEnabled")));
	Row->SetBoolField(TEXT("conversion"), Conversion && (ReadObject(Conversion->Struct, ConversionValue, TEXT("SourceToDestinationConversion")) ||
		ReadObject(Conversion->Struct, ConversionValue, TEXT("DestinationToSourceConversion"))));
	return Row;
}

FString PathText(UObject* View, const UScriptStruct* PathStruct, const void* Path)
{
	const FString Source = ReadEnum(PathStruct, Path, TEXT("Source"));
	const FName WidgetName = ReadName(PathStruct, Path, TEXT("WidgetName"));
	const FGuid Context = ReadGuid(PathStruct, Path, TEXT("ContextId"));
	TArray<FString> Parts;
	if (Source == TEXT("SelfContext"))
	{
		Parts.Add(TEXT("Self"));
	}
	else if (!WidgetName.IsNone() && Source != TEXT("ViewModel"))
	{
		Parts.Add(WidgetName.ToString());
	}
	else if (Context.IsValid())
	{
		const FViewArray Models = ViewArray(View, TEXT("AvailableViewModels"));
		FString Name = Context.ToString(EGuidFormats::DigitsWithHyphens);
		for (int32 Index = 0; Index < Models.Num(); ++Index)
		{
			if (ReadGuid(Models.Struct, Models.At(Index), TEXT("ViewModelContextId")) == Context)
			{
				Name = ReadName(Models.Struct, Models.At(Index), TEXT("ViewModelName")).ToString();
			}
		}
		Parts.Add(Name);
	}
	const FArrayProperty* Paths = CastField<FArrayProperty>(FieldOf(PathStruct, TEXT("Paths")));
	const FStructProperty* Element = Paths ? CastField<FStructProperty>(Paths->Inner) : nullptr;
	if (Element && Path)
	{
		FScriptArrayHelper Fields(Paths, Paths->ContainerPtrToValuePtr<void>(Path));
		for (int32 Index = 0; Index < Fields.Num(); ++Index)
		{
			const FMemberReference* Reference = ReferenceOf(Element->Struct, Fields.GetRawPtr(Index));
			Parts.Add(Reference ? Reference->GetMemberName().ToString() : FString(TEXT("?")));
		}
	}
	return FString::Join(Parts, TEXT("."));
}

bool WritePath(UWidgetBlueprint* Widget, UObject* View, const FString& Text, const UScriptStruct* PathStruct, void* Path,
               FString& OutError)
{
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT("."));
	const FArrayProperty* Paths = CastField<FArrayProperty>(FieldOf(PathStruct, TEXT("Paths")));
	const FStructProperty* Element = Paths ? CastField<FStructProperty>(Paths->Inner) : nullptr;
	if (Parts.Num() < 2 || !Element)
	{
		OutError = FString::Printf(TEXT("'%s' is no path: write the viewmodel or widget, then its fields (Health.Current, HealthBar.Percent)."), *Text);
		return false;
	}
	const UStruct* Owner = nullptr;
	const int32 Model = FindViewModel(View, Parts[0]);
	UWidget* Child = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(FName(*Parts[0])) : nullptr;
	if (Parts[0].Equals(TEXT("Self"), ESearchCase::IgnoreCase))
	{
		Owner = Widget->SkeletonGeneratedClass;
		WriteEnum(PathStruct, Path, TEXT("Source"), TEXT("SelfContext"));
	}
	else if (Model != INDEX_NONE)
	{
		const FViewArray Models = ViewArray(View, TEXT("AvailableViewModels"));
		Owner = Cast<UClass>(ReadObject(Models.Struct, Models.At(Model), TEXT("NotifyFieldValueClass")));
		if (FGuid* Id = static_cast<FGuid*>(ValuePtr(PathStruct, Path, TEXT("ContextId"))))
		{
			*Id = ReadGuid(Models.Struct, Models.At(Model), TEXT("ViewModelContextId"));
		}
		WriteEnum(PathStruct, Path, TEXT("Source"), TEXT("ViewModel"));
	}
	else if (Child)
	{
		Owner = Child->GetClass();
		if (FName* Name = static_cast<FName*>(ValuePtr(PathStruct, Path, TEXT("WidgetName"))))
		{
			*Name = Child->GetFName();
		}
		WriteEnum(PathStruct, Path, TEXT("Source"), TEXT("Widget"));
	}
	else
	{
		OutError = FString::Printf(TEXT("No viewmodel or widget is named %s in %s; there are: %s."), *Parts[0], *Widget->GetName(), *Names(Widget, View));
		return false;
	}
	if (FBoolProperty* SourceSet = CastField<FBoolProperty>(FieldOf(PathStruct, TEXT("bDeprecatedSource"))))
	{
		SourceSet->SetPropertyValue_InContainer(Path, true); // true: Source says where the path starts
	}
	FScriptArrayHelper Fields(Paths, Paths->ContainerPtrToValuePtr<void>(Path));
	Fields.EmptyValues();
	for (int32 Index = 1; Index < Parts.Num(); ++Index)
	{
		const UStruct* Lookup = Owner ? SkeletonOf(Owner) : nullptr;
		const FName Name(*Parts[Index]);
		const FProperty* Property = Lookup ? FindFProperty<FProperty>(Lookup, Name) : nullptr;
		const UFunction* Function = !Property && Cast<UClass>(Lookup) ? Cast<UClass>(Lookup)->FindFunctionByName(Name) : nullptr;
		if (!Property && !Function)
		{
			OutError = Lookup ? FString::Printf(TEXT("No field %s is on %s."), *Parts[Index], *OwnerText(Lookup))
				: FString::Printf(TEXT("%s holds no object or struct, so %s cannot be read from it."), *Parts[Index - 1], *Parts[Index]);
			return false;
		}
		const UStruct* FieldOwner = Property ? Property->GetOwnerStruct() : Function->GetOuterUClass();
		const UClass* OwnerClass = Cast<UClass>(FieldOwner);
		uint8* Field = Fields.GetRawPtr(Fields.AddValue());
		FMemberReference* Reference = ReferenceOf(Element->Struct, Field);
		if (!Reference || !WriteEnum(Element->Struct, Field, TEXT("BindingKind"), Property ? TEXT("Property") : TEXT("Function")))
		{
			OutError = TEXT("This engine's MVVM field path has no member reference and binding kind to write.");
			return false;
		}
		FGuid Guid;
		if (OwnerClass && Property)
		{
			UBlueprint::GetGuidFromClassByFieldName<FProperty>(OwnerClass, Name, Guid);
		}
		else if (OwnerClass)
		{
			UBlueprint::GetGuidFromClassByFieldName<UFunction>(OwnerClass, Name, Guid);
		}
		const bool bSelf = OwnerClass && ((Widget->GeneratedClass && Widget->GeneratedClass->IsChildOf(OwnerClass)) ||
			(Widget->SkeletonGeneratedClass && Widget->SkeletonGeneratedClass->IsChildOf(OwnerClass)));
		if (bSelf)
		{
			Reference->SetSelfMember(Name, Guid);
		}
		else if (OwnerClass)
		{
			Reference->SetExternalMember(Name, const_cast<UClass*>(Cast<UClass>(SkeletonOf(OwnerClass))), Guid);
		}
		else
		{
			static_cast<FStructMemberReference*>(Reference)->Set(Name, FieldOwner, Guid);
		}
		Owner = NextOwner(Property ? Property : Function->GetReturnProperty());
	}
	return true;
}
}
