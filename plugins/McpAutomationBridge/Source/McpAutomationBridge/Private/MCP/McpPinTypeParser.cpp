// McpPinTypeParser.cpp — implementation. Filled progressively in Tasks 3-6.

#include "MCP/McpPinTypeParser.h"

#if WITH_EDITOR

#include "EdGraphSchema_K2.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Color.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/Class.h"

namespace
{
	UClass* FindClassBySimpleName(const FString& SimpleName)
	{
		// UE 5.1+ exposes UClass::TryFindTypeSlow.
		if (UClass* Cls = UClass::TryFindTypeSlow<UClass>(SimpleName))
		{
			return Cls;
		}
		// Engine-prefixed fallback for common engine classes (Texture2D, StaticMesh, etc.)
		return UClass::TryFindTypeSlow<UClass>(FString(TEXT("/Script/Engine.")) + SimpleName);
	}

	UObject* FindUserDefinedAssetByName(const FString& Name, UClass* ExpectedClass)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByClass(ExpectedClass->GetClassPathName(), Assets);
		for (const FAssetData& A : Assets)
		{
			if (A.AssetName.ToString() == Name)
			{
				return A.GetAsset();
			}
		}
		return nullptr;
	}
}

bool FMcpPinTypeParser::Parse(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutError)
{
	const FString T = TypeString.TrimStartAndEnd();
	if (T.IsEmpty()) { OutError = TEXT("Empty type string"); return false; }

	OutPinType = FEdGraphPinType();
	OutPinType.ContainerType = EPinContainerType::None;

	// 1. Keyword scalar
	if (T == TEXT("Bool"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; return true; }
	if (T == TEXT("Byte"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;    return true; }
	if (T == TEXT("Int"))    { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;     return true; }
	if (T == TEXT("Int64"))  { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;   return true; }
	if (T == TEXT("Float"))  { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
	                           OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float; return true; }
	if (T == TEXT("Double")) { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
	                           OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double; return true; }
	if (T == TEXT("String")) { OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;  return true; }
	if (T == TEXT("Name"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;    return true; }
	if (T == TEXT("Text"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;    return true; }

	// 2. Builtin structs (TBaseStructure)
	auto MakeStruct = [&](UScriptStruct* SS) {
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = SS;
	};
	if (T == TEXT("Vector"))      { MakeStruct(TBaseStructure<FVector>::Get());      return true; }
	if (T == TEXT("Vector2D"))    { MakeStruct(TBaseStructure<FVector2D>::Get());    return true; }
	if (T == TEXT("Rotator"))     { MakeStruct(TBaseStructure<FRotator>::Get());     return true; }
	if (T == TEXT("Transform"))   { MakeStruct(TBaseStructure<FTransform>::Get());   return true; }
	if (T == TEXT("Color"))       { MakeStruct(TBaseStructure<FColor>::Get());       return true; }
	if (T == TEXT("LinearColor")) { MakeStruct(TBaseStructure<FLinearColor>::Get()); return true; }

	// 3. Object reference: ClassName*
	if (T.EndsWith(TEXT("*")))
	{
		const FString ClassName = T.LeftChop(1).TrimStartAndEnd();
		UClass* Cls = FindClassBySimpleName(ClassName);
		if (!Cls)
		{
			OutError = FString::Printf(TEXT("Unknown class for object reference: '%s'"), *ClassName);
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPinType.PinSubCategoryObject = Cls;
		return true;
	}

	// 4. Class reference: TSubclassOf<X>
	if (T.StartsWith(TEXT("TSubclassOf<")) && T.EndsWith(TEXT(">")))
	{
		const FString Inner = T.Mid(12, T.Len() - 13).TrimStartAndEnd();
		UClass* Cls = FindClassBySimpleName(Inner);
		if (!Cls)
		{
			OutError = FString::Printf(TEXT("Unknown class for TSubclassOf<%s>"), *Inner);
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		OutPinType.PinSubCategoryObject = Cls;
		return true;
	}

	// 5. User-defined enum: E_X
	if (T.StartsWith(TEXT("E_")))
	{
		UObject* Obj = FindUserDefinedAssetByName(T, UUserDefinedEnum::StaticClass());
		if (!Obj)
		{
			OutError = FString::Printf(TEXT("User-defined enum not found: '%s'"), *T);
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		OutPinType.PinSubCategoryObject = Obj;
		return true;
	}

	// 6. User-defined struct: S_X
	if (T.StartsWith(TEXT("S_")))
	{
		UObject* Obj = FindUserDefinedAssetByName(T, UUserDefinedStruct::StaticClass());
		if (!Obj)
		{
			OutError = FString::Printf(TEXT("User-defined struct not found: '%s'"), *T);
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = Obj;
		return true;
	}

	// 7. Lenient fallback: bare class name treated as object reference (e.g. "Texture2D" == "Texture2D*")
	if (UClass* Cls = FindClassBySimpleName(T))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPinType.PinSubCategoryObject = Cls;
		return true;
	}

	OutError = FString::Printf(TEXT("Cannot parse type: '%s' (not a keyword, builtin struct, class reference, or known user-defined asset)"), *T);
	return false;
}

FString FMcpPinTypeParser::Serialize(const FEdGraphPinType& PinType, FString& OutWarning)
{
	OutWarning = TEXT("Not yet implemented");
	return TEXT("");
}

#endif // WITH_EDITOR
