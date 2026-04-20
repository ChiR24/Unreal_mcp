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

	// 0. Containers: Array<T>, Set<T>, Map<K,V> — recurse into inner
	if (T.StartsWith(TEXT("Array<")) && T.EndsWith(TEXT(">")))
	{
		const FString Inner = T.Mid(6, T.Len() - 7).TrimStartAndEnd();
		FEdGraphPinType InnerType;
		if (!Parse(Inner, InnerType, OutError)) return false;
		if (InnerType.ContainerType != EPinContainerType::None)
		{
			OutError = TEXT("Nested containers not supported by FEdGraphPinType");
			return false;
		}
		OutPinType = InnerType;
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}

	if (T.StartsWith(TEXT("Set<")) && T.EndsWith(TEXT(">")))
	{
		const FString Inner = T.Mid(4, T.Len() - 5).TrimStartAndEnd();
		FEdGraphPinType InnerType;
		if (!Parse(Inner, InnerType, OutError)) return false;
		if (InnerType.ContainerType != EPinContainerType::None)
		{
			OutError = TEXT("Nested containers not supported by FEdGraphPinType");
			return false;
		}
		OutPinType = InnerType;
		OutPinType.ContainerType = EPinContainerType::Set;
		return true;
	}

	if (T.StartsWith(TEXT("Map<")) && T.EndsWith(TEXT(">")))
	{
		// Split inner on top-level comma (no nested generics expected since we forbid above).
		const FString Inner = T.Mid(4, T.Len() - 5).TrimStartAndEnd();
		int32 CommaIdx = INDEX_NONE;
		int32 Depth = 0;
		for (int32 i = 0; i < Inner.Len(); ++i)
		{
			const TCHAR C = Inner[i];
			if (C == '<') ++Depth;
			else if (C == '>') --Depth;
			else if (C == ',' && Depth == 0) { CommaIdx = i; break; }
		}
		if (CommaIdx == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Map<K,V> requires comma-separated K and V: '%s'"), *T);
			return false;
		}
		const FString KStr = Inner.Left(CommaIdx).TrimStartAndEnd();
		const FString VStr = Inner.Mid(CommaIdx + 1).TrimStartAndEnd();
		FEdGraphPinType KeyType, ValType;
		if (!Parse(KStr, KeyType, OutError)) return false;
		if (!Parse(VStr, ValType, OutError)) return false;
		if (KeyType.ContainerType != EPinContainerType::None ||
		    ValType.ContainerType != EPinContainerType::None)
		{
			OutError = TEXT("Map K/V cannot themselves be containers");
			return false;
		}
		OutPinType = KeyType;
		OutPinType.ContainerType = EPinContainerType::Map;
		OutPinType.PinValueType.TerminalCategory = ValType.PinCategory;
		OutPinType.PinValueType.TerminalSubCategory = ValType.PinSubCategory;
		OutPinType.PinValueType.TerminalSubCategoryObject = ValType.PinSubCategoryObject;
		return true;
	}

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
