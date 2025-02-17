#pragma once

#include "internal.hh"
#include "rego.hh"
#include "trieste/token.h"

namespace rego
{
  inline const auto FastData = TokenDef("fast-rego-data", flag::lookup | flag::symtab);
  inline const auto FastModule = TokenDef("fast-rego-module", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastRule = TokenDef("fast-rego-rule", flag::lookdown | flag::lookup);
  inline const auto FastBody = TokenDef("fast-rego-body");
  inline const auto FastLocal = TokenDef("fast-rego-local", flag::lookup);
  inline const auto FastTimeNowNS = TokenDef("fast-rego-timenowns");
  inline const auto FastRegexMatch = TokenDef("fast-rego-regexmatch");
  inline const auto FastObject= TokenDef("fast-rego-object", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastObjectItem = TokenDef("fast-rego-objectitem", flag::lookdown | flag::lookup);
  inline const auto FastObjectKey = TokenDef("fast-rego-objectkey", flag::print);

  // clang-format off
  inline const auto wf_fast_input =
    rego::wf_fast
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module)
    | (Input <<= DataTerm | Undefined)
    | (DataObject <<= DataObjectItem++)
    | (DataObjectItem <<= (Key >>= DataTerm) * (Val >>= DataTerm))[Key]
    | (DataTerm <<= Scalar | DataArray | DataObject | DataSet)
    | (DataArray <<= DataTerm++)
    | (DataSet <<= DataTerm++)
    ;
  // clang-format on

  // clang-format off
  inline const auto wf_fast_prep =
    wf_fast_input
    | (Rego <<= Query * Input * FastData)
    | (FastData <<= Key * (Val >>= FastModule))[Key]
    | (Query <<= (FastLocal | Expr | NotExpr)++)
    | (Input <<= Key * (Val >>= DataTerm | Undefined))[Key]
    | (FastModule <<= Key * Policy)[Key]
    | (Policy <<= FastRule++)
    | (FastRule <<= Var * FastBody * (Val >>= (Expr | Term)) * (Idx >>= Int))[Var]
    | (FastBody <<= (FastLocal | Expr | NotExpr)++)
    | (FastLocal <<= Var * (Val >>= Expr))[Var]
    | (Expr <<= (Term | ExprInfix | ExprParens | UnaryExpr | FastTimeNowNS | FastRegexMatch))
    | (Term <<= Ref | Var | Scalar | Array | FastObject | Set)
    | (DataTerm <<= Scalar | DataArray | FastObject | DataSet)
    | (FastObject <<= FastObjectItem++)
    | (FastRegexMatch <<= Expr * Expr)
    | (FastObjectItem <<= FastObjectKey * (Val >>= Term | Expr))[FastObjectKey]
    | (Scalar <<= JSONString | Int | Float | True | False | Null)
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