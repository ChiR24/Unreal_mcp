// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Containers/SortedMap.h"

// Cassowary (Foundation/Solvers): linear equalities and inequalities over variables, each required or preferred with a
// strength, solved by the incremental simplex of Badros, Borning and Stuckey (2001) as Kiwi (BSD) builds it: a required
// constraint holds exactly or is refused; preferred ones are met as far as the required ones allow, a stronger one
// before any number of weaker ones. Symbols are numbered and kept in order, so the same constraints always give the
// same answer. For one-shot layouts of tens to hundreds of constraints (no edit variables).
namespace McpCassowary
{
constexpr double Required = 1001001000.0;
constexpr double Strong = 1000000.0;
constexpr double Medium = 1000.0;
constexpr double Weak = 1.0;

/** A coefficient this small is zero: it leaves its row. */
inline bool CassowaryNearZero(double Value)
{
    return FMath::Abs(Value) < 1.0e-8;
}

enum class ERelation : uint8
{
    LessOrEqual,
    Equal,
    GreaterOrEqual
};

/** Sum of Coefficient * Variable over Terms, plus Constant, related to zero by Relation, held at Strength. */
struct FConstraint
{
    TArray<TPair<int32, double>> Terms; // variable, coefficient
    double Constant = 0.0;
    ERelation Relation = ERelation::Equal;
    double Strength = Required;
};

class FSolver
{
public:
    /** A new variable (its value is 0 until a constraint moves it). */
    int32 AddVariable();

    /** Adds C; false when C is required and cannot hold with the required constraints already added. */
    bool AddConstraint(const FConstraint& C);

    double Value(int32 Variable) const;

private:
    enum class ESymbol : uint8
    {
        External,
        Slack,
        Error,
        Dummy
    };

    struct FRow
    {
        double Constant = 0.0;
        TSortedMap<int32, double> Cells; // symbol -> coefficient

        void Insert(int32 Symbol, double Coefficient);
        void Insert(const FRow& Other, double Coefficient);
        void SolveFor(int32 Symbol);
        void SolveFor(int32 Lhs, int32 Rhs);
        void Substitute(int32 Symbol, const FRow& Row);
    };

    int32 NewSymbol(ESymbol Type);
    FRow CreateRow(const FConstraint& C, int32& OutMarker, int32& OutOther);
    int32 ChooseSubject(const FRow& Row, int32 Marker, int32 Other) const;
    bool AddWithArtificial(const FRow& Row);
    void Substitute(int32 Symbol, const FRow& Row);
    bool Optimize(FRow& Objective);

    TArray<ESymbol> Symbols;          // type of each symbol
    TArray<int32> VariableSymbols;    // the external symbol of each variable
    TSortedMap<int32, FRow> Rows;     // basic symbol -> its row
    FRow Objective;
    TOptional<FRow> Artificial;
};
} // namespace McpCassowary
