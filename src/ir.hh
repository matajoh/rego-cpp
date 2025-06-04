#pragma once

#include "trieste/token.h"
#include "trieste/writer.h"
#include "internal.hh"

namespace rego::ir
{
  inline const auto Policy = TokenDef("rego-ir-policy");
  inline const auto Static = TokenDef("rego-ir-static", flag::lookup | flag::symtab);
  inline const auto String = TokenDef("rego-ir-string");
  inline const auto BuiltInFunction = TokenDef("rego-ir-builtinfunction", flag::lookup | flag::symtab);
  inline const auto Name = TokenDef("rego-ir-name", flag::print);
  inline const auto Decl = TokenDef("rego-ir-decl");
  inline const auto Plan = TokenDef("rego-ir-plan", flag::lookup | flag::symtab);
  inline const auto Block = TokenDef("rego-ir-block");
  inline const auto Function = TokenDef("rego-ir-function", flag::lookup | flag::symtab);
  inline const auto File = TokenDef("rego-ir-file");
  inline const auto Local = TokenDef("rego-ir-local");
  inline const auto Return = TokenDef("rego-ir-return");
  inline const auto Operand = TokenDef("rego-ir-operand");
  inline const auto Int32 = TokenDef("rego-ir-int32", flag::print);
  inline const auto Int64 = TokenDef("rego-ir-int64", flag::print);
  inline const auto UInt32 = TokenDef("rego-ir-uint32", flag::print);
  
  // sequences
  inline const auto BlockSeq = TokenDef("rego-ir-blockseq");
  inline const auto ParameterSeq = TokenDef("rego-ir-parameterseq");
  inline const auto PlanSeq = TokenDef("rego-ir-planseq");
  inline const auto FunctionSeq = TokenDef("rego-ir-functionseq");
  inline const auto BuiltInFunctionSeq = TokenDef("rego-ir-builtinfunctionseq");
  inline const auto StringSeq = TokenDef("rego-ir-stringseq");
  inline const auto PathSeq = TokenDef("rego-ir-pathseq");
  inline const auto LocalSeq = TokenDef("rego-ir-localseq");
  inline const auto OperandSeq = TokenDef("rego-ir-operandseq");
  inline const auto Int32Seq = TokenDef("rego-ir-int32seq");

  // Statements
  inline const auto ArrayAppendStmt = TokenDef("rego-ir-arrayappendstmt");
  inline const auto AssignIntStmt = TokenDef("rego-ir-assignintstmt");
  inline const auto AssignVarOnceStmt = TokenDef("rego-ir-assignvaroncestmt");
  inline const auto AssignVarStmt = TokenDef("rego-ir-assignvarstmt");
  inline const auto BlockStmt = TokenDef("rego-ir-blockstmt");
  inline const auto BreakStmt = TokenDef("rego-ir-breakstmt");
  inline const auto CallDynamicStmt = TokenDef("rego-ir-calldynamicstmt");  
  inline const auto CallStmt = TokenDef("rego-ir-callstmt");
  inline const auto DotStmt = TokenDef("rego-ir-dotstmt");
  inline const auto EqualStmt = TokenDef("rego-ir-equalstmt");
  inline const auto IsArrayStmt = TokenDef("rego-ir-isarraystmt");
  inline const auto IsDefinedStmt = TokenDef("rego-ir-isdefinedstmt");
  inline const auto IsObjectStmt = TokenDef("rego-ir-isobjectstmt");
  inline const auto IsSetStmt = TokenDef("rego-ir-issetstmt");
  inline const auto IsUndefinedStmt = TokenDef("rego-ir-isundefinedstmt");
  inline const auto LenStmt = TokenDef("rego-ir-lenstmt");
  inline const auto MakeArrayStmt = TokenDef("rego-ir-makearraystmt");
  inline const auto MakeNullStmt = TokenDef("rego-ir-makenullstmt");
  inline const auto MakeNumberIntStmt = TokenDef("rego-ir-makenumberintstmt");
  inline const auto MakeNumberRefStmt = TokenDef("rego-ir-makenumberrefstmt");
  inline const auto MakeObjectStmt = TokenDef("rego-ir-makeobjectstmt");
  inline const auto MakeSetStmt = TokenDef("rego-ir-makesetstmt");
  inline const auto NotEqualStmt = TokenDef("rego-ir-notequalstmt");
  inline const auto NotStmt = TokenDef("rego-ir-notstmt");
  inline const auto ObjectInsertOnceStmt = TokenDef("rego-ir-objectinsertoncestmt");
  inline const auto ObjectInsertStmt = TokenDef("rego-ir-objectinsertstmt");
  inline const auto ObjectMergeStmt = TokenDef("rego-ir-objectmergestmt");
  inline const auto ResetLocalStmt = TokenDef("rego-ir-resetlocalstmt");
  inline const auto ResultSetAddStmt = TokenDef("rego-ir-resultsetaddstmt");
  inline const auto ReturnLocalStmt = TokenDef("rego-ir-returnlocalstmt");
  inline const auto ScanStmt = TokenDef("rego-ir-scanstmt");
  inline const auto SetAddStmt = TokenDef("rego-ir-setaddstmt");
  inline const auto WithStmt = TokenDef("rego-ir-withstmt");

  // utility tokens
  inline const auto Idx = TokenDef("rego-ir-idx", flag::print);
  inline const auto Value = TokenDef("rego-ir-value", flag::print);

  // clang-format off
  inline const auto wf_ir_input =
    rego::wf
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module++)
    | (Input <<= DataTerm | Undefined)
    | (DataSeq <<= Data++)
    | (Data <<= DataTerm)
    | (DataObject <<= DataObjectItem++)
    | (DataObjectItem <<= (Key >>= DataTerm) * (Val >>= DataTerm))
    | (DataTerm <<= Scalar | DataArray | DataObject | DataSet)
    | (DataArray <<= DataTerm++)
    | (DataSet <<= DataTerm++)
    | (Import <<= Ref * Var)[Var]
    ;
  // clang-format on

  inline const auto Statement = ArrayAppendStmt | AssignIntStmt | AssignVarOnceStmt |
    AssignVarStmt | BlockStmt | BreakStmt | CallDynamicStmt | CallStmt |
    DotStmt | EqualStmt | IsArrayStmt | IsDefinedStmt | IsObjectStmt |
    IsSetStmt | IsUndefinedStmt | LenStmt | MakeArrayStmt | MakeNullStmt |
    MakeNumberIntStmt | MakeNumberRefStmt | MakeObjectStmt | MakeSetStmt |
    NotEqualStmt | NotStmt | ObjectInsertOnceStmt | ObjectInsertStmt |
    ObjectMergeStmt | ResetLocalStmt | ResultSetAddStmt |
    ReturnLocalStmt | ScanStmt | SetAddStmt | WithStmt;

  // clang-format off
  inline const auto wf_ir =
    (Policy <<= Static * PlanSeq * FunctionSeq)
    | (Static <<= StringSeq * BuiltInFunctionSeq * PathSeq)
    | (StringSeq <<= String++)
    | (String <<= Idx * Value)[Idx]
    | (BuiltInFunctionSeq <<= BuiltInFunction++)
    | (BuiltInFunction <<= Name * Decl)
    | (PathSeq <<= Path++)
    | (PlanSeq <<= Plan++[1])
    | (Plan <<= Name * BlockSeq)
    | (BlockSeq <<= Block++)
    | (Block <<= Statement++)
    | (FunctionSeq <<= Function++)
    | (Function <<= Name * Path * ParameterSeq * Return * BlockSeq)
    | (ArrayAppendStmt <<= Local * Operand)
    | (AssignIntStmt <<= Int64 * Local)
    | (AssignVarOnceStmt <<= Operand * Local)
    | (AssignVarStmt <<= Operand * Local)
    | (BlockStmt <<= BlockSeq)
    | (BreakStmt <<= UInt32)
    | (CallDynamicStmt <<= OperandSeq * LocalSeq * Local)
    | (CallStmt <<= Name * LocalSeq * Local)
    | (DotStmt <<= Operand * Operand * Local)
    | (EqualStmt <<= Operand * Operand)
    | (IsArrayStmt <<= Operand)
    | (IsDefinedStmt <<= Operand)
    | (IsObjectStmt <<= Operand)
    | (IsSetStmt <<= Operand)
    | (IsUndefinedStmt <<= Operand)
    | (LenStmt <<= Operand * Local)
    | (MakeArrayStmt <<= Int32 * Local)
    | (MakeNullStmt <<= Local)
    | (MakeNumberIntStmt <<= Int64 * Local)
    | (MakeNumberRefStmt <<= Int32 * Local)
    | (MakeObjectStmt <<= Local)
    | (MakeSetStmt <<= Local)
    | (NotEqualStmt <<= Operand * Operand)
    | (NotStmt <<= Block)
    | (ObjectInsertOnceStmt <<= Operand * Operand * Local)
    | (ObjectInsertStmt <<= Operand * Operand * Local)
    | (ObjectMergeStmt <<= Local * Local * Local)
    | (ResetLocalStmt <<= Local)
    | (ResultSetAddStmt <<= Local)
    | (ReturnLocalStmt <<= Local)
    | (ScanStmt <<= Local * Local * Local * Block)
    | (SetAddStmt <<= Operand * Local)
    | (WithStmt <<= Local * Int32Seq * Operand * Block)
    | (Operand <<= Local | True | False | Idx)
    | (LocalSeq <<= Local++)
    | (OperandSeq <<= Operand++)
    | (Int32Seq <<= Int32++)
    ;
  // clang-format on
}