#pragma once

#include "trieste/json.h"
#include "trieste/token.h"
#include "trieste/writer.h"
#include "internal.hh"

namespace rego::ir
{
  inline const auto Policy = TokenDef("rego-ir-policy", flag::symtab);
  inline const auto DataSeq = TokenDef("rego-ir-dataseq");
  inline const auto Static = TokenDef("rego-ir-static");
  inline const auto String = TokenDef("rego-ir-string", flag::print);
  inline const auto BuiltInFunction = TokenDef("rego-ir-builtinfunction", flag::lookup | flag::symtab);
  inline const auto Decl = TokenDef("rego-ir-decl");
  inline const auto Plan = TokenDef("rego-ir-plan", flag::lookup);
  inline const auto Block = TokenDef("rego-ir-block");
  inline const auto Function = TokenDef("rego-ir-function", flag::lookup | flag::symtab);
  inline const auto File = TokenDef("rego-ir-file");
  inline const auto LocalIndex = TokenDef("rego-ir-localindex", flag::print);
  inline const auto Operand = TokenDef("rego-ir-operand");
  inline const auto Int32 = TokenDef("rego-ir-int32", flag::print);
  inline const auto Int64 = TokenDef("rego-ir-int64", flag::print);
  inline const auto UInt32 = TokenDef("rego-ir-uint32", flag::print);
  inline const auto StringIndex = TokenDef("rego-ir-stringindex", flag::print);
  inline const auto Boolean = TokenDef("rego-ir-boolean", flag::print);
  
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
  inline const auto DebugFileStmt = TokenDef("rego-ir-debugfilestmt");
  inline const auto DebugLocationStmt = TokenDef("rego-ir-debuglocationstmt");

  // utility tokens
  inline const auto Key = TokenDef("rego-ir-key");
  inline const auto Value = TokenDef("rego-ir-value");
  inline const auto Stmt = TokenDef("rego-ir-stmt");
  inline const auto Source = TokenDef("rego-ir-source");
  inline const auto Target = TokenDef("rego-ir-target");
  inline const auto Func = TokenDef("rego-ir-func");
  inline const auto Array = TokenDef("rego-ir-array");
  inline const auto Blocks = TokenDef("rego-ir-blocks");
  inline const auto Index = TokenDef("rego-ir-index");
  inline const auto Return = TokenDef("rego-ir-return");
  inline const auto Args = TokenDef("rego-ir-args");
  inline const auto Lhs = TokenDef("rego-ir-lhs");
  inline const auto Rhs = TokenDef("rego-ir-rhs");
  inline const auto Capacity = TokenDef("rego-ir-capacity");
  inline const auto Object = TokenDef("rego-ir-object");
  inline const auto Set = TokenDef("rego-ir-set");
  inline const auto Name = TokenDef("rego-ir-name");
  inline const auto Row = TokenDef("rego-ir-row");
  inline const auto Col = TokenDef("rego-ir-col");

  // clang-format off
  inline const auto wf_ir_input =
    wf
    | (Top <<= Rego)
    | (Rego <<= Query * Input * DataSeq * ModuleSeq)
    | (ModuleSeq <<= Module++)
    | (DataSeq <<= json::Object++)
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
    ReturnLocalStmt | ScanStmt | SetAddStmt | WithStmt |
    DebugFileStmt | DebugLocationStmt;

  // clang-format off
  inline const auto wf_ir =
    (Policy <<= Static * PlanSeq * FunctionSeq)
    | (Static <<= StringSeq * BuiltInFunctionSeq * PathSeq)
    | (StringSeq <<= String++)
    | (BuiltInFunctionSeq <<= BuiltInFunction++)
    | (BuiltInFunction <<= Name * Decl)
    | (PathSeq <<= String++)
    | (PlanSeq <<= Plan++[1])
    | (Plan <<= (Name >>= String) * BlockSeq)
    | (BlockSeq <<= Block++)
    | (Block <<= Statement++)
    | (FunctionSeq <<= Function++)
    | (Function <<= (Name >>= String) * (Path >>= StringSeq) * ParameterSeq * (Return >>= LocalIndex) * BlockSeq)
    | (ParameterSeq <<= LocalIndex++)
    | (ArrayAppendStmt <<= (Array >>= LocalIndex) * (Value >>= Operand))
    | (AssignIntStmt <<= (Value >>= Int64) * (Target >>= LocalIndex))
    | (AssignVarOnceStmt <<= (Source >>= Operand) * (Target >>= LocalIndex))
    | (AssignVarStmt <<= (Source >>= Operand) * (Target >>= LocalIndex))
    | (BlockStmt <<= (Blocks >>= BlockSeq))
    | (BreakStmt <<= (Index >>= UInt32))
    | (CallDynamicStmt <<= (Func >>= OperandSeq) * (Args >>= OperandSeq) * (Result >>= LocalIndex))
    | (CallStmt <<= (Func >>= String) * (Args >>= OperandSeq) * (Result >>= LocalIndex))
    | (DotStmt <<= (Source >>= Operand) * (Key >>= Operand) * (Target >>= LocalIndex))
    | (EqualStmt <<= (Lhs >>= Operand) * (Rhs >>= Operand))
    | (IsArrayStmt <<= (Source >>= Operand))
    | (IsDefinedStmt <<= (Source >>= LocalIndex))
    | (IsObjectStmt <<= (Source >>= Operand))
    | (IsSetStmt <<= (Source >>= Operand))
    | (IsUndefinedStmt <<= (Source >>= Operand))
    | (LenStmt <<= (Source >>= Operand) * (Target >>= LocalIndex))
    | (MakeArrayStmt <<= (Capacity >>= Int32) * (Target >>= LocalIndex))
    | (MakeNullStmt <<= (Target >>= LocalIndex))
    | (MakeNumberIntStmt <<= (Value >>= Int64) * (Target >>= LocalIndex))
    | (MakeNumberRefStmt <<= (Index >>= Int32) * (Target >>= LocalIndex))
    | (MakeObjectStmt <<= (Target >>= LocalIndex))
    | (MakeSetStmt <<= (Target >>= LocalIndex))
    | (NotEqualStmt <<= (Lhs >>= Operand) * (Rhs >>= Operand))
    | (NotStmt <<= Block)
    | (ObjectInsertOnceStmt <<= (Key >>= Operand) * (Value >>= Operand) * (Object >>= LocalIndex))
    | (ObjectInsertStmt <<= (Key >>= Operand) * (Value >>= Operand) * (Object >>= LocalIndex))
    | (ObjectMergeStmt <<= (Lhs >>= LocalIndex) * (Rhs >>= LocalIndex) * (Target >>= LocalIndex))
    | (ResetLocalStmt <<= (Target >>= LocalIndex))
    | (ResultSetAddStmt <<= (Value >>= LocalIndex))
    | (ReturnLocalStmt <<= (Source >>= LocalIndex))
    | (ScanStmt <<= (Source >>= LocalIndex) * (Key >>= LocalIndex) * (Value >>= LocalIndex) * Block)
    | (SetAddStmt <<= (Value >>= Operand) * (Set >>= LocalIndex))
    | (WithStmt <<= LocalIndex * (Path >>= Int32Seq) * (Value >>= Operand) * Block)
    | (DebugFileStmt <<= (Source >>= StringIndex))
    | (DebugLocationStmt <<= (Row >>= Int32) * (Col >>= Int32))
    | (Operand <<= LocalIndex | Boolean | StringIndex)
    | (LocalSeq <<= LocalIndex++)
    | (OperandSeq <<= Operand++)
    | (Int32Seq <<= Int32++)
    ;
  // clang-format on
}