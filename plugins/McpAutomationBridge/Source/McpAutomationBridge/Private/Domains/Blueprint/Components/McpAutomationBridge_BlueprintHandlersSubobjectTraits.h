#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include <type_traits>
#include <utility>

#include "SubobjectDataSubsystem.h"

namespace McpAutomationBridge {
template <typename, typename = void> struct THasK2Add : std::false_type {};

template <typename T>
struct THasK2Add<T, std::void_t<decltype(std::declval<T>().K2_AddNewSubobject(
                        std::declval<FAddNewSubobjectParams>()))>>
    : std::true_type {};

template <typename, typename = void> struct THasAdd : std::false_type {};

template <typename T>
struct THasAdd<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(
                      std::declval<FAddNewSubobjectParams>()))>>
    : std::true_type {};

template <typename, typename = void> struct THasAddTwoArg : std::false_type {};

template <typename T>
struct THasAddTwoArg<
    T, std::void_t<decltype(std::declval<T>().AddNewSubobject(
           std::declval<FAddNewSubobjectParams>(), std::declval<FText &>()))>>
    : std::true_type {};

template <typename, typename = void>
struct THandleHasIsValid : std::false_type {};

template <typename T>
struct THandleHasIsValid<T, std::void_t<decltype(std::declval<T>().IsValid())>>
    : std::true_type {};

template <typename, typename = void> struct THasRename : std::false_type {};

template <typename T>
struct THasRename<
    T, std::void_t<decltype(std::declval<T>().RenameSubobjectMemberVariable(
           std::declval<UBlueprint *>(), std::declval<FSubobjectDataHandle>(),
           std::declval<FName>()))>> : std::true_type {};

template <typename, typename = void>
struct THasDeleteSubobject : std::false_type {};

template <typename T>
struct THasDeleteSubobject<
    T, std::void_t<decltype(std::declval<T>().DeleteSubobject(
           std::declval<const FSubobjectDataHandle &>(),
           std::declval<const FSubobjectDataHandle &>(),
           std::declval<UBlueprint *>()))>> : std::true_type {};
} // namespace McpAutomationBridge
