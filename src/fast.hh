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
  inline const auto FastRule = TokenDef("fast-rego-rule", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastBody = TokenDef("fast-rego-body");
  inline const auto FastLocal = TokenDef("fast-rego-local", flag::lookup | flag::shadowing);
  inline const auto FastTimeNowNS = TokenDef("fast-rego-timenowns");
  inline const auto FastRegexMatch = TokenDef("fast-rego-regexmatch");
  inline const auto FastRegexIsValid = TokenDef("fast-rego-regexisvalid");
  inline const auto FastObject= TokenDef("fast-rego-object", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastDataObject= TokenDef("fast-rego-dataobject", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto FastObjectItem = TokenDef("fast-rego-objectitem", flag::lookdown | flag::lookup);
  inline const auto FastDataObjectItem = TokenDef("fast-rego-dataobjectitem", flag::lookdown | flag::lookup | flag::symtab);
  inline const auto Pattern = TokenDef("fast-rego-pattern");

  // clang-format off
  inline const auto wf_fast_input =
    rego::wf_fast
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module++)
    | (DataSeq <<= Data++)
    | (Input <<= DataTerm | Undefined)
    | (Data <<= DataTerm)
    | (DataObject <<= DataObjectItem++)
    | (DataObjectItem <<= (Key >>= DataTerm) * (Val >>= DataTerm))[Key]
    | (DataTerm <<= Scalar | DataArray | DataObject)
    | (DataArray <<= DataTerm++)
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
    | (Expr <<= (Term | ExprInfix | UnaryExpr | FastTimeNowNS | FastRegexIsValid | FastRegexMatch))
    | (Term <<= Ref | Var | Scalar | Array | FastObject)
    | (DataTerm <<= Scalar | DataArray | FastDataObject)
    | (FastObject <<= FastObjectItem++)
    | (FastDataObject <<= FastDataObjectItem++)
    | (FastRegexMatch <<= (Pattern >>= Expr) * (Val >>= Expr))
    | (FastRegexIsValid <<= (Pattern >>= Expr))
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
    | (Term <<= Scalar | Array | FastObject | FastDataObject | DataArray)
    | (Array <<= Term++)
    | (FastObjectItem <<= Key * (Val >>= Term))[Key]
    ;
  // clang-format on
}