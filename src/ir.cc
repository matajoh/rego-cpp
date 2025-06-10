#include "ir.hh"

#include "trieste/json.h"

namespace
{
  using namespace trieste;
  using namespace rego;
  using namespace rego::ir;

  inline const auto NumberIndex = TokenDef("rego-numberindex", flag::print);

  // clang-format off
  inline const auto wf_ir_strings =
    wf_ir_input
    | (Top <<= Rego * ir::Policy)
    | (Rego <<= DataSeq * ModuleSeq)
    | (ir::Policy <<= Static * PlanSeq * FunctionSeq)
    | (Static <<= StringSeq * BuiltInFunctionSeq * PathSeq)
    | (StringSeq <<= ir::String++)
    | (PathSeq <<= ir::String++)
    | (Scalar <<= StringIndex | NumberIndex | True | False | Null)
    ;
  // clang-format on

  PassDef strings()
  {
    auto strings = std::make_shared<std::map<std::string, int>>();
    auto files = std::make_shared<std::map<std::string, int>>();
    PassDef pass = {
      "strings",
      wf_ir_strings,
      dir::topdown | dir::once,
      {
        In(Top) * T(Rego)[Rego] >>
          [](Match& _) {
            return Seq
              << _(Rego)
              << (ir::Policy
                  << ((Static << StringSeq << BuiltInFunctionSeq << PathSeq)
                      << PlanSeq << FunctionSeq));
          },

        T(Query, Input) >> [](Match&) -> Node { return nullptr; },

        T(Module)[Module] >> [files](Match& _) -> Node {
          std::string src = _(Module)->location().source->origin();
          if (!src.empty())
          {
            files->emplace(src, files->size());
          }
          return NoChange;
        },

        T(rego::String)[rego::String] >>
          [strings](Match& _) {
            std::string str(_(rego::String)->location().view());
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

        T(Int, Float)[NumberIndex] >>
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
        node << (ir::String ^ str);
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
        node << (ir::String ^ file);
      }

      return 0;
    });

    return pass;
  }
}

namespace rego
{
  Rewriter to_ir()
  {
    return {"to_ir", {strings()}, wf_ir_input};
  }
}