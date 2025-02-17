#pragma once

#include "internal.hh"
#include "rego.hh"
#include "trieste/token.h"

namespace rego
{
  inline const auto FastData = TokenDef("fast-rego-data", flag::lookup | flag::symtab);
  inline const auto FastModuleSeq = TokenDef("fast-rego-moduleseq");
  inline const auto FastDataModule = TokenDef("fast-rego-datamodule", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastModule = TokenDef("fast-rego-module", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastRule = TokenDef("fast-rego-rule", flag::lookdown | flag::lookup);
  inline const auto FastBody = TokenDef("fast-rego-body");
  inline const auto FastLocal = TokenDef("fast-rego-local", flag::lookup);
  inline const auto FastTimeNowNS = TokenDef("fast-rego-timenowns");
  inline const auto FastRegexMatch = TokenDef("fast-rego-regexmatch");
  inline const auto FastObject= TokenDef("fast-rego-object", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastDataObject= TokenDef("fast-rego-dataobject", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastObjectItem = TokenDef("fast-rego-objectitem", flag::lookdown | flag::lookup);
  inline const auto FastDataObjectItem = TokenDef("fast-rego-dataobjectitem", flag::lookdown | flag::lookup | flag::symtab);

  // clang-format off
  inline const auto wf_fast_input =
    rego::wf_fast
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module)
    | (DataSeq <<= Data++)
    | (Input <<= DataTerm | Undefined)
    | (Data <<= DataTerm)
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
    | (FastData <<= Key * (Val >>= FastModuleSeq))[Key]
    | (FastModuleSeq <<= (FastModule | FastDataModule)++)
    | (FastModule <<= Key * (Val >>= Policy))[Key]
    | (FastDataModule <<= Key * (Val >>= DataTerm))[Key]
    | (Query <<= (FastLocal | Expr | NotExpr)++)
    | (Input <<= Key * (Val >>= DataTerm | Undefined))[Key]
    | (Policy <<= FastRule++)
    | (FastRule <<= Var * FastBody * (Val >>= (Expr | Term)) * (Idx >>= Int))[Var]
    | (FastBody <<= (FastLocal | Expr | NotExpr)++)
    | (FastLocal <<= Var * (Val >>= Expr))[Var]
    | (Expr <<= (Term | ExprInfix | ExprParens | UnaryExpr | FastTimeNowNS | FastRegexMatch))
    | (Term <<= Ref | Var | Scalar | Array | FastObject | Set)
    | (DataTerm <<= Scalar | DataArray | FastDataObject | DataSet)
    | (FastObject <<= FastObjectItem++)
    | (FastDataObject <<= FastDataObjectItem++)
    | (FastRegexMatch <<= Expr * Expr)
    | (FastObjectItem <<= Key * (Val >>= Term | Expr))[Key]
    | (FastDataObjectItem <<= Key * (Val >>= DataTerm))[Key]
    | (Scalar <<= JSONString | Int | Float | True | False | Null)
    ;
  // clang-format on

  // clang-format off
  inline const auto wf_fast_resolve =
    wf_fast_prep
    | (Rego <<= Query)
    | (Query <<= Result++)
    | (Result <<= Terms * Bindings)
    | (Terms <<= Term++)
    | (Bindings <<= Binding++)
    | (Binding <<= Var * (Val >>= Term))[Var]
    | (Term <<= Scalar | Array | Object | Set)
    | (ObjectItem <<= (Key >>= Term) * (Val >>= Term))
    ;
  // clang-format on
}