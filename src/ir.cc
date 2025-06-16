#include "ir.hh"

#include "trieste/json.h"

namespace
{
  using namespace trieste;
  using namespace rego::ir;
  namespace r = rego;
  namespace ir = rego::ir;

  inline const auto NumberIndex = TokenDef("rego-ir-numberindex", flag::print);

  // clang-format off
  inline const auto wf_ir_strings_files_data =
    wf_ir_input
    | (Top <<= json::Object * r::Rego * Policy)
    | (r::Rego <<= r::ModuleSeq)
    | (Policy <<= Static * PlanSeq * FunctionSeq)
    | (Static <<= StringSeq * BuiltInFunctionSeq * PathSeq)
    | (StringSeq <<= String++)
    | (PathSeq <<= String++)
    | (r::Scalar <<= StringIndex | NumberIndex | r::True | r::False | r::Null)
    ;
  // clang-format on

  /** This pass extracts all string literals and merges the data JSON objects.
   */
  PassDef strings_files_data()
  {
    auto strings = std::make_shared<std::map<std::string, int>>();
    auto files = std::make_shared<std::map<std::string, int>>();
    PassDef pass = {
      "strings_files_data",
      wf_ir_strings_files_data,
      dir::bottomup | dir::once,
      {
        In(Top) * T(r::Rego)[r::Rego] >>
          [](Match& _) {
            return Seq << _(r::Rego)
                       << (Policy << (Static << StringSeq << PathSeq));
          },

        T(r::Query, r::Input) >> [](Match&) -> Node { return nullptr; },

        In(r::DataSeq) * (T(json::Object)[Lhs] * T(json::Object)[Rhs]) >>
          [](Match& _) { return json::Object << *_[Lhs] << *_[Rhs]; },

        T(r::DataSeq) << (T(json::Object)[json::Object] * End) >>
          [](Match& _) { return Lift << Top << _(json::Object); },

        T(r::DataSeq) << End >>
          [](Match& _) { return Lift << Top << NodeDef::create(json::Object); },

        T(r::Module)[r::Module] >> [files](Match& _) -> Node {
          std::string src = _(r::Module)->location().source->origin();
          if (!src.empty())
          {
            files->emplace(src, files->size());
          }
          return NoChange;
        },

        T(r::String)[String] >>
          [strings](Match& _) {
            std::string str(_(String)->location().view());
            size_t index;
            if (!str.empty())
            {
              auto it = strings->find(str);
              if (it == strings->end())
              {
                index = strings->size();
                (*strings)[str] = index;
              }
              else
              {
                index = it->second;
              }
            }

            return StringIndex ^ std::to_string(index);
          },

        T(r::Int, r::Float)[NumberIndex] >>
          [strings](Match& _) {
            std::string num_str(_(NumberIndex)->location().view());
            size_t index;
            auto it = strings->find(num_str);
            if (it == strings->end())
            {
              index = strings->size();
              (*strings)[num_str] = index;
            }
            else
            {
              index = it->second;
            }
            return NumberIndex ^ std::to_string(index);
          },
      }};

    pass.post(StringSeq, [strings](Node node) {
      std::vector<std::string> strings_vec(strings->size());
      for (const auto& [str, id] : *strings)
      {
        strings_vec[id] = str;
      }

      for (const auto& str : strings_vec)
      {
        node << (String ^ str);
      }

      return 0;
    });

    pass.post(PathSeq, [files](Node node) {
      std::vector<std::string> files_vec(files->size());
      for (const auto& [file, id] : *files)
      {
        files_vec[id] = file;
      }

      for (const auto& file : files_vec)
      {
        node << (String ^ file);
      }

      return 0;
    });

    return pass;
  }

  inline const auto Local =
    TokenDef("rego-ir-local", flag::lookup | flag::shadowing);
  inline const auto LocalRef = TokenDef("rego-ir-localref", flag::print);
  inline const auto Ident = TokenDef("rego-ir-ident", flag::print);

  inline const auto wf_ir_functions_locals_stmts = AssignVarOnceStmt |
    IsDefinedStmt | MakeNullStmt | MakeNumberRefStmt | ReturnLocalStmt;

  // clang-format off
  inline const auto wf_ir_functions_locals =
    wf_ir_strings_files_data
    | (r::Policy <<= Function++)
    | (Function <<= r::Ref * ParameterSeq * LocalSeq * Local * BlockSeq)
    | (ParameterSeq <<= Local++)
    | (BlockSeq <<= Block++)
    | (Block <<= wf_ir_functions_locals_stmts++[1])
    | (AssignVarOnceStmt <<= Operand * LocalRef)
    | (IsDefinedStmt <<= LocalRef)
    | (MakeNullStmt <<= LocalRef)
    | (MakeNumberRefStmt <<= NumberIndex * LocalRef)
    | (ReturnLocalStmt <<= LocalRef)
    | (Operand <<= LocalRef | Boolean | StringIndex)
    | (Local <<= Ident)[Ident]
    ;
  // clang-format on

  /** This pass converts all rules to functions and creates the local variables.
   */
  PassDef functions_locals()
  {
    return {
      "functions_locals",
      wf_ir_functions_locals,
      dir::bottomup | dir::once,
      {
        In(r::Policy) *
            (T(r::Rule)
             << (T(r::False) *
                 (T(r::RuleHead)
                  << ((T(r::RuleRef) << T(r::Ref)[r::Ref]) *
                      (T(r::RuleHeadComp)
                       << (T(Local)[Local] * T(Block)[Block])))) *
                 T(r::RuleBodySeq)[BlockSeq])) >>
          [](Match& _) {
            Node param_input = Local << (Ident ^ _.fresh({"input"}));
            Node param_data = Local << (Ident ^ _.fresh({"data"}));
            Location return_name = _.fresh({"return"});
            Node local_return = Local << (Ident ^ return_name);
            Node local_head = _(Local);
            Node local_ref = LocalRef ^ (local_head / Ident)->location();

            return Function
              << _(r::Ref) << (ParameterSeq << param_input << param_data)
              << (LocalSeq << local_head) << local_return
              << (BlockSeq << _(Block)
                           << (Block
                               << (IsDefinedStmt << local_ref)
                               << (AssignVarOnceStmt << (Operand << local_ref)
                                                     << (LocalRef ^ return_name)))
                           << (Block << (ReturnLocalStmt << (LocalRef ^ return_name))));
          },

        T(r::Expr)
            << (T(r::Term) << (T(r::Scalar) << T(StringIndex)[StringIndex])) >>
          [](Match& _) {
            Location name = _.fresh({"term"});
            Node term = Local << (Ident ^ name);
            return Seq << term
                       << (Block << (ResetLocalStmt << (LocalRef ^ name)))
                       << (AssignVarOnceStmt << (Operand << _(StringIndex))
                                             << (LocalRef ^ name));
          },

        T(r::Expr)
            << (T(r::Term)
                << (T(r::Scalar) << T(r::True, r::False)[Boolean])) >>
          [](Match& _) {
            Location name = _.fresh({"term"});
            Node term = Local << (Ident ^ name);
            return Seq << term
                       << (Block << (ResetLocalStmt << (LocalRef ^ name))
                                 << (AssignVarOnceStmt
                                     << (Operand << (Boolean ^ _(Boolean)))
                                     << (LocalRef ^ name)));
          },

        T(r::Expr)
            << (T(r::Term) << (T(r::Scalar) << T(NumberIndex)[NumberIndex])) >>
          [](Match& _) {
            Location number_name = _.fresh({"number"});
            Location term_name = _.fresh({"term"});
            Node number = Local << (Ident ^ number_name);
            Node term = Local << (Ident ^ term_name);
            return Seq << term
                       << (Block
                           << (ResetLocalStmt << (LocalRef ^ term_name))
                           << (MakeNumberRefStmt << _(NumberIndex)
                                                 << (LocalRef ^ number_name))
                           << (AssignVarOnceStmt
                               << (Operand << (LocalRef ^ number_name))
                               << (LocalRef ^ term_name)));
          },

        T(r::Expr) << (T(r::Term) << (T(r::Scalar) << T(r::Null))) >>
          [](Match& _) {
            Location null_name = _.fresh({"null"});
            Location term_name = _.fresh({"term"});
            Node null = Local << (Ident ^ null_name);
            Node term = Local << (Ident ^ term_name);
            return Seq << term
                       << (Block << (ResetLocalStmt << (LocalRef ^ term_name))
                                 << (MakeNullStmt << (LocalRef ^ null_name))
                                 << (AssignVarOnceStmt
                                     << (Operand << (LocalRef ^ null_name))
                                     << (LocalRef ^ term_name)));
          },
      }};
  }

  // lift out functions
  // insert entry points
}

namespace rego
{
  Rewriter to_ir()
  {
    return {"to_ir", {strings_files_data(), functions_locals()}, wf_ir_input};
  }
}