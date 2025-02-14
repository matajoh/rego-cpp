#pragma once

#include "internal.hh"
#include "rego.hh"

namespace rego
{
  inline const auto FastData = TokenDef("fast-rego-data", flag::lookup);
  inline const auto FastModule = TokenDef("fast-rego-module", flag::lookup);
  inline const auto FastRule = TokenDef("fast-rego-rule", flag::lookup);
  inline const auto FastBody = TokenDef("fast-rego-body");
  inline const auto FastLocal = TokenDef("fast-rego-local", flag::lookup);
  inline const auto FastTimeNS = TokenDef("fast-rego-timens");
  inline const auto FastRegexMatch = TokenDef("fast-rego-regexmatch");

  // clang-format off
  inline const auto wf_fast_input =
    rego::wf
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module)
    | (Input <<= DataTerm | Undefined)
    ;
  // clang-format on

  // clang-format off
  inline const auto wf_fast_prep =
    wf_fast_input
    | (Rego <<= Query * FastData * Input)
    | (FastData <<= Key * (Val >>= FastModule))[Key]
    | (Query <<= (FastLocal | Expr | NotExpr)++)
    | (Input <<= Key * (Val >>= DataTerm | Undefined))[Key]
    | (FastModule <<= FastRule++)
    | (FastRule <<= Var * FastBody * Term)[Var]
    | (FastBody <<= (FastLocal | Expr)++)
    | (FastLocal <<= Var * (Val >>= Expr))[Var]
    | (Expr <<= (Term | ExprInfix | ExprParens | UnaryExpr))
    | (ObjectItem <<= (Key >>= JSONString) * (Val >>= Expr))
    ;
  // clang-format on

  // clang-format off
  inline const auto wf_fast_unify =
    wf_fast_prep
    | (Rego <<= Query)
    | (Query <<= Result++)
    | (Result <<= Terms * Bindings)
    | (Terms <<= Term++)
    | (Bindings <<= Binding++)
    | (Binding <<= Var * Term)[Var]
    | (Term <<= Scalar | Array | Object | Set)
    ;
  // clang-format on
}