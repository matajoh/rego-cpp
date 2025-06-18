#include "ir.hh"

#include "trieste/json.h"
#include "trieste/rewrite.h"

namespace
{
  using namespace trieste;
  using namespace rego::ir;
  namespace r = rego;

  inline const auto NumberIndex = TokenDef("rego-ir-numberindex", flag::print);
  inline const auto BaseDocument =
    TokenDef("rego-ir-basedocument", flag::lookdown | flag::symtab);
  inline const auto VirtualDocument = TokenDef(
    "rego-ir-virtualdocument", flag::lookup | flag::lookdown | flag::symtab);
  inline const auto DocumentSeq = TokenDef("rego-ir-documentseq");
  inline const auto Ident = TokenDef("rego-ir-ident", flag::print);

  // clang-format off
  inline const auto wf_ir_strings_files_documents =
    wf_ir_input
    | (IR <<= BaseDocument * VirtualDocument * Policy)
    | (BaseDocument <<= json::Object)
    | (Policy <<= Static * EntryPointSeq)
    | (VirtualDocument <<= Ident * r::Policy * DocumentSeq)[Ident]
    | (DocumentSeq <<= VirtualDocument++)
    | (Static <<= StringSeq * PathSeq)
    | (StringSeq <<= String++)
    | (PathSeq <<= String++)
    | (r::Scalar <<= StringIndex | NumberIndex | r::True | r::False | r::Null)
    | (r::Import <<= r::Ref * r::Var)[r::Var]
    ;
  // clang-format on

  Node to_stringseq(Token token, const std::map<std::string, int>& strings)
  {
    Node seq = NodeDef::create(token);
    std::vector<std::string> strings_vec(strings.size());
    for (const auto& [str, id] : strings)
    {
      strings_vec[id] = str;
    }

    for (const auto& str : strings_vec)
    {
      seq << (String ^ str);
    }

    return seq;
  }

  Node add_or_find_child_document(Node parent, const std::string& child_name)
  {
    Node children = parent / DocumentSeq;
    for (Node child : *children)
    {
      Node ident = child->front();
      if (ident->location().view() == child_name)
      {
        return child;
      }
    }

    Node child = VirtualDocument << (Ident ^ child_name) << r::Policy
                                 << NodeDef::create(DocumentSeq);
    children->push_back(child);
    return child;
  }

  Node merge_documents(const std::vector<Node>& documents)
  {
    Node docseq = NodeDef::create(DocumentSeq);
    std::map<std::string, Node> docs;
    for (Node node : documents)
    {
      Node ref = node->front();
      Node refhead = ref / r::RefHead;
      Node refargseq = ref / r::RefArgSeq;

      std::string name(refhead->front()->location().view());
      if (refargseq->size() == 0)
      {
        docs[name] = node;
        node->replace_at(0, Ident ^ name);
        docseq << node;
        continue;
      }

      Node doc = docs[name];
      for (Node node : *refargseq)
      {
        std::string child_name(node->front()->location().view());
        doc = add_or_find_child_document(doc, child_name);
      }

      doc->replace_at(1, node->at(1));
    }

    return VirtualDocument << (Ident ^ "data") << r::Policy << docseq;
  }

  /** This pass extracts all string literals, merges the base documents, and
   * nests the virtual documents.
   */
  PassDef strings_files_documents()
  {
    auto strings = std::make_shared<std::map<std::string, int>>();
    auto files = std::make_shared<std::map<std::string, int>>();
    auto documents = std::make_shared<Nodes>();
    PassDef pass = {
      "strings_files_documents",
      wf_ir_strings_files_documents,
      dir::bottomup | dir::once,
      {
        In(r::DataSeq) * (T(json::Object)[Lhs] * T(json::Object)[Rhs]) >>
          [](Match& _) { return json::Object << *_[Lhs] << *_[Rhs]; },

        T(r::Module)[r::Module]
            << ((T(r::Package) << T(r::Ref)[r::Ref]) * T(r::Version) *
                T(r::ImportSeq)[r::ImportSeq] * T(r::Policy)[r::Policy]) >>
          [files, documents](Match& _) -> Node {
          std::string src = _(r::Module)->location().source->origin();
          if (!src.empty())
          {
            files->emplace(src, files->size());
          }

          documents->push_back(VirtualDocument << _(r::Ref) << _(r::Policy));
          return nullptr;
        },

        T(r::String)[String] * In(r::Scalar) >>
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

        T(r::ModuleSeq) >> [](Match&) -> Node { return nullptr; },

        In(IR) *
            (T(EntryPointSeq)[EntryPointSeq] *
             (T(r::DataSeq) << (~T(json::Object)[json::Object] * End))) >>
          [strings, files, documents](Match& _) -> Node {
          Node stringseq = to_stringseq(StringSeq, *strings);
          Node pathseq = to_stringseq(PathSeq, *files);

          Node dataobj = _(json::Object);
          if (dataobj == nullptr)
          {
            dataobj = NodeDef::create(json::Object);
          }

          Node basedoc = BaseDocument << dataobj;
          Node virtualdoc = merge_documents(*documents);

          return Seq << basedoc << virtualdoc
                     << (Policy << (Static << stringseq << pathseq)
                                << _(EntryPointSeq));
        },
      }};

    pass.pre([documents, strings, files](Node) {
      documents->clear();
      strings->clear();
      files->clear();

      strings->emplace("result", 0);

      return 0;
    });

    return pass;
  }

  inline const auto Local =
    TokenDef("rego-ir-local", flag::lookup | flag::shadowing);
  inline const auto LocalRef = TokenDef("rego-ir-localref", flag::print);
  inline const auto Rule =
    TokenDef("rego-ir-rule", flag::lookup | flag::lookdown);
  inline const auto RuleSeq = TokenDef("rego-ir-ruleseq");
  inline const auto EntryPoint = TokenDef("rego-ir-entrypoint");

  inline const auto wf_ir_functions_locals_stmts = AssignVarOnceStmt |
    IsDefinedStmt | MakeNullStmt | MakeNumberRefStmt | ReturnLocalStmt;

  // clang-format off
  inline const auto wf_ir_lift_functions =
    wf_ir_strings_files_documents
    | (Function <<= (Name >>= String) * ParameterSeq * LocalSeq * Local * BlockSeq)
    | (Rule <<= Ident * String)[Ident]
    | (ParameterSeq <<= Local++)
    | (BlockSeq <<= Block++)
    | (Block <<= wf_ir_functions_locals_stmts++[1])
    | (AssignVarOnceStmt <<= Operand * LocalRef)
    | (IsDefinedStmt <<= LocalRef)
    | (MakeNullStmt <<= LocalRef)
    | (MakeNumberRefStmt <<= NumberIndex * LocalRef)
    | (MakeObjectStmt <<= (Target >>= LocalRef))
    | (ReturnLocalStmt <<= LocalRef)
    | (CallStmt <<= String * OperandSeq * LocalRef)
    | (ObjectInsertStmt <<= Operand * Operand * LocalRef)
    | (Operand <<= LocalRef | Boolean | StringIndex)
    | (EntryPointSeq <<= EntryPoint++[1])
    | (EntryPoint <<= Ident * r::Ref)
    | (Local <<= Ident)[Ident]
    ;
  // clang-format on

  void dump_ref(std::ostream& os, Node ref, const std::string& delim)
  {
    Node head = ref / r::RefHead;
    os << head->front()->location().view();
    Node refargs = ref / r::RefArgSeq;
    if (refargs->empty())
    {
      return;
    }

    for (auto& arg : *refargs)
    {
      os << delim << arg->front()->location().view();
    }
  }

  std::string ruleref_to_string(
    Node ruleref,
    const std::string& prefix = "",
    const std::string& delim = "/")
  {
    Node doc = ruleref->parent(VirtualDocument);
    std::vector<std::string_view> doc_idents;
    while (doc)
    {
      doc_idents.push_back((doc / Ident)->location().view());
      doc = doc->parent(VirtualDocument);
    }

    doc_idents.pop_back(); // remove the "data" document

    std::ostringstream path_buf;
    path_buf << prefix;

    while (!doc_idents.empty())
    {
      path_buf << doc_idents.back();
      doc_idents.pop_back();
      path_buf << delim;
    }

    dump_ref(path_buf, ruleref, delim);
    return path_buf.str();
  }

  /** This pass lifts all rules as functions that can be called from plans.
   */
  PassDef lift_functions()
  {
    std::shared_ptr<Nodes> functions = std::make_shared<Nodes>();
    PassDef pass = {
      "functions_locals",
      wf_ir_lift_functions,
      dir::bottomup | dir::once,
      {
        T(r::Expr)
            << (T(r::Term) << (T(r::Scalar) << T(StringIndex)[StringIndex])) >>
          [](Match& _) {
            Location name = _.fresh({"term"});
            Node term = Local << (Ident ^ name);
            return Seq << term
                       << (Block
                           << (ResetLocalStmt << (LocalRef ^ name))
                           << (AssignVarOnceStmt << (Operand << _(StringIndex))
                                                 << (LocalRef ^ name)));
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

        In(r::Policy) *
            (T(r::Rule)
             << (T(r::False) *
                 (T(r::RuleHead)
                  << ((T(r::RuleRef) << T(r::Ref)[r::Ref]) *
                      (T(r::RuleHeadComp)
                       << (T(Local)[Local] * T(Block)[Block])))) *
                 T(r::RuleBodySeq)[BlockSeq])) >>
          [functions](Match& _) {
            Node param_input = Local << (Ident ^ "input");
            Node param_data = Local << (Ident ^ "data");
            Location return_name = _.fresh({"return"});
            Node local_return = Local << (Ident ^ return_name);
            Node local_head = _(Local);
            Node local_ref = LocalRef ^ (local_head / Ident)->location();
            std::string func_name =
              ruleref_to_string(_(r::Ref), "g0.data.", ".");

            functions->push_back(
              Function << (String ^ func_name)
                       << (ParameterSeq << param_input << param_data)
                       << (LocalSeq << local_head) << local_return
                       << (BlockSeq << _(Block)
                                    << (Block << (IsDefinedStmt << local_ref)
                                              << (AssignVarOnceStmt
                                                  << (Operand << local_ref)
                                                  << (LocalRef ^ return_name)))
                                    << (Block
                                        << (ReturnLocalStmt
                                            << (LocalRef ^ return_name)))));

            Node ident = Ident ^ (_(r::Ref) / r::RefHead)->front();
            return (Rule << (Ident ^ ident) << (String ^ func_name));
          },

        In(EntryPointSeq) * T(String)[String] >>
          [](Match& _) {
            std::string entry_point(_(String)->location().view());
            size_t start = 0;
            size_t end = entry_point.find('/');

            Node refargs = NodeDef::create(r::RefArgSeq);
            while (end != std::string::npos)
            {
              std::string arg = entry_point.substr(start, end - start);
              refargs << (r::RefArgDot << (r::Var ^ arg));
              start = end + 1;
              end = entry_point.find('/', start);
            }

            std::string last_arg = entry_point.substr(start);
            if (!last_arg.empty())
            {
              refargs << (r::RefArgDot << (r::Var ^ last_arg));
            }

            return EntryPoint
              << (Ident ^ _(String))
              << (r::Ref << (r::RefHead << (r::Var ^ "data")) << refargs);
          },

        In(VirtualDocument) * T(r::Policy)[r::Policy] >>
          [](Match& _) { return RuleSeq << *_[r::Policy]; },
      }};

    pass.post([functions](Node top) {
      Node policy = top / IR / Policy;
      Node functionseq = NodeDef::create(FunctionSeq);
      for (Node func : *functions)
      {
        functionseq << func;
      }
      policy << functionseq;
      return 0;
    });

    return pass;
  }

  // clang-format off
  inline const auto wf_ir_add_plans =
    wf_ir_lift_functions
    | (Policy <<= Static * PlanSeq * FunctionSeq)
    | (PlanSeq <<= Plan++)
    | (Plan <<= Ident * LocalSeq * BlockSeq)
    ;
  // clang-format on

  PassDef add_plans()
  {
    // add the plans required to satisfy the entry points
    return {
      "add_plans",
      wf_ir_add_plans,
      dir::bottomup | dir::once,
      {
        In(EntryPointSeq) *
            (T(EntryPoint)
             << (T(Ident)[Ident] *
                 (T(r::Ref)
                  << ((T(r::RefHead) << T(r::Var)[r::Var]) *
                      T(r::RefArgSeq)[r::RefArgSeq])))) >>
          [](Match& _) -> Node {
          Nodes nodes = _(r::Var)->lookup();
          if (nodes.empty())
          {
            return NoChange;
          }

          Node plan = nodes.front();
          for (Node arg : *_(r::RefArgSeq))
          {
            plan = plan->lookdown(arg->front()->location()).front();
          }

          if (plan == Rule)
          {
            Node func_name = String ^ (plan / String);
            Location call_name = _.fresh({"call"});
            Location result_name = _.fresh({"result"});
            Location obj_name = _.fresh({"obj"});
            Node local_call = Local << (Ident ^ call_name);
            Node local_result = Local << (Ident ^ result_name);
            Node local_obj = Local << (Ident ^ obj_name);
            return Plan
              << _(Ident)
              << (LocalSeq << local_call << local_result << local_obj)
              << (BlockSeq
                  << (Block
                      << (CallStmt
                          << (String ^ func_name)
                          << (OperandSeq << (Operand << (LocalRef ^ "input"))
                                         << (Operand << (LocalRef ^ "data")))
                          << (LocalRef ^ call_name))
                      << (AssignVarStmt << (Operand << (LocalRef ^ call_name))
                                        << (LocalRef ^ result_name))
                      << (MakeObjectStmt << (LocalRef ^ obj_name))
                      << (ObjectInsertStmt
                          << (Operand << (StringIndex ^ "0"))
                          << (Operand << (LocalRef ^ result_name))
                          << (LocalRef ^ obj_name))
                      << (ResultSetAddStmt << (LocalRef ^ obj_name))));
          }
          else
          {
            logging::Output() << "Not supported yet";
          }

          return NoChange;
        },

        In(Policy) * T(EntryPointSeq)[EntryPointSeq] >>
          [](Match& _) { return PlanSeq << *_[EntryPointSeq]; },

      }};
  }

  PassDef index_locals()
  {
    // replace all local refs with their indices in the frame
    return {"index_locals", wf_ir, dir::bottomup | dir::once, {}};
  }
}

namespace rego
{
  Rewriter to_ir()
  {
    return {
      "to_ir",
      {strings_files_documents(),
       lift_functions(),
       add_plans(),
       index_locals()},
      wf_ir_input};
  }
}