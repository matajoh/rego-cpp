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
  inline const auto wf_ir_documents =
    wf_ir_input
    | (IR <<= BaseDocument * VirtualDocument * Policy)
    | (BaseDocument <<= json::Object)
    | (Policy <<= Static * EntryPointSeq)
    | (VirtualDocument <<= Ident * r::Policy * DocumentSeq)[Ident]
    | (DocumentSeq <<= VirtualDocument++)
    | (PathSeq <<= String++)
    ;
  // clang-format on

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
                                 << DocumentSeq;
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

  /** This pass extracts all string literals, merges the base documents, and
   * nests the virtual documents.
   */
  PassDef documents()
  {
    auto files = std::make_shared<std::map<std::string, int>>();
    auto documents = std::make_shared<Nodes>();
    PassDef pass = {
      "documents",
      wf_ir_documents,
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

          documents->push_back(
            VirtualDocument << _(r::Ref) << _(r::Policy) << DocumentSeq);
          return nullptr;
        },

        T(r::ModuleSeq) >> [](Match&) -> Node { return nullptr; },

        In(IR) *
            (T(EntryPointSeq)[EntryPointSeq] *
             (T(r::DataSeq) << (~T(json::Object)[json::Object] * End))) >>
          [files, documents](Match& _) -> Node {
          Node pathseq = to_stringseq(PathSeq, *files);

          Node dataobj = _(json::Object);
          if (dataobj == nullptr)
          {
            dataobj = NodeDef::create(json::Object);
          }

          Node basedoc = BaseDocument << dataobj;
          Node virtualdoc = merge_documents(*documents);

          return Seq << basedoc << virtualdoc
                     << (Policy << (Static << pathseq) << _(EntryPointSeq));
        },
      }};

    pass.pre([documents, files](Node) {
      documents->clear();
      files->clear();

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
  inline const auto FuncName = TokenDef("rego-ir-funcname");

  // clang-format off
  inline const auto wf_ir_lift_functions =
    wf_ir_documents
    | (VirtualDocument <<= Ident * RuleSeq * DocumentSeq)[Ident]
    | (Function <<= (Name >>= String) * ParameterSeq * LocalSeq * Local * BlockSeq)
    | (Rule <<= Ident * (FuncName >>= String))[Ident]
    | (ParameterSeq <<= Local++)
    | (BlockSeq <<= Block++)
    | (Block <<= Statement++[1])
    | (AssignVarOnceStmt <<= Operand * LocalRef)
    | (IsDefinedStmt <<= LocalRef)
    | (MakeNullStmt <<= LocalRef)
    | (MakeNumberRefStmt <<= NumberIndex * LocalRef)
    | (MakeObjectStmt <<= (Target >>= LocalRef))
    | (ReturnLocalStmt <<= LocalRef)
    | (CallStmt <<= String * OperandSeq * LocalRef)
    | (ObjectInsertStmt <<= Operand * Operand * LocalRef)
    | (Operand <<= LocalRef | Boolean | String)
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
    PassDef pass =
      {"functions_locals",
       wf_ir_lift_functions,
       dir::bottomup | dir::once,
       {
         T(r::Expr)
             << (T(r::Term)
                 << (T(r::Scalar) << T(r::String, r::RawString)[String])) >>
           [](Match& _) {
             Location name = _(String)->parent(r::Rule)->fresh({"term"});
             Node term = Local << (Ident ^ name);
             return Seq << term
                        << (Block << (ResetLocalStmt << (LocalRef ^ name))
                                  << (AssignVarOnceStmt
                                      << (Operand << (String ^ _(String)))
                                      << (LocalRef ^ name)));
           },

         T(r::Expr)
             << (T(r::Term)
                 << (T(r::Scalar) << T(r::True, r::False)[Boolean])) >>
           [](Match& _) {
             Location name = _(Boolean)->parent(r::Rule)->fresh({"term"});
             Node term = Local << (Ident ^ name);
             return Seq << term
                        << (Block << (ResetLocalStmt << (LocalRef ^ name))
                                  << (AssignVarOnceStmt
                                      << (Operand << (Boolean ^ _(Boolean)))
                                      << (LocalRef ^ name)));
           },

         T(r::Expr)
             << (T(r::Term) << (T(r::Scalar) << T(r::Int, r::Float)[String])) >>
           [](Match& _) {
             Location number_name =
               _(String)->parent(r::Rule)->fresh({"number"});
             Location term_name = _(String)->parent(r::Rule)->fresh({"term"});
             Node number = Local << (Ident ^ number_name);
             Node term = Local << (Ident ^ term_name);
             return Seq << term
                        << (Block
                            << (ResetLocalStmt << (LocalRef ^ term_name))
                            << (MakeNumberRefStmt << (String ^ _(String))
                                                  << (LocalRef ^ number_name))
                            << (AssignVarOnceStmt
                                << (Operand << (LocalRef ^ number_name))
                                << (LocalRef ^ term_name)));
           },

         T(r::Expr) << (T(r::Term) << (T(r::Scalar) << T(r::Null)[r::Null])) >>
           [](Match& _) {
             Location null_name = _(r::Null)->parent(r::Rule)->fresh({"null"});
             Location term_name = _(r::Null)->parent(r::Rule)->fresh({"term"});
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
             (T(r::Rule)[r::Rule]
              << (T(r::False) *
                  (T(r::RuleHead)
                   << ((T(r::RuleRef) << T(r::Ref)[r::Ref]) *
                       (T(r::RuleHeadComp)
                        << (T(Local)[Local] * T(Block)[Block])))) *
                  T(r::RuleBodySeq)[BlockSeq])) >>
           [functions](Match& _) {
             Node param_input = Local << (Ident ^ "input");
             Node param_data = Local << (Ident ^ "data");
             Location return_name = _(r::Rule)->fresh({"return"});
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

  Node rule_block(
    Node frame,
    Nodes& locals,
    Node rule,
    const Location& key,
    const Location& obj_name)
  {
    Location func_name = (rule / FuncName)->location();
    Location value_name = frame->fresh({"value"});
    locals.push_back(Local << (Ident ^ value_name));
    return Block << (CallStmt
                     << (String ^ func_name)
                     << (OperandSeq << (Operand << (LocalRef ^ "input"))
                                    << (Operand << (LocalRef ^ "data")))
                     << (LocalRef ^ value_name))
                 << (ObjectInsertStmt << (Operand << (String ^ key))
                                      << (Operand << (LocalRef ^ value_name))
                                      << (LocalRef ^ obj_name));
  }

  Node document_block(
    Node frame,
    Nodes& locals,
    Node document,
    const Location& key,
    const Location& obj_name)
  {
    Location doc_name = frame->fresh((document / Ident)->location());
    locals.push_back(Local << (Ident ^ doc_name));
    Node block = Block << (MakeObjectStmt << (LocalRef ^ doc_name));
    Node children = document / DocumentSeq;
    if (!children->empty())
    {
      for (Node child : *children)
      {
        block
          << (BlockStmt
              << (BlockSeq << document_block(
                    frame,
                    locals,
                    child,
                    (child / Ident)->location(),
                    doc_name)));
      }
    }

    Node rules = document / RuleSeq;
    if (!rules->empty())
    {
      for (Node rule : *(document / RuleSeq))
      {
        block
          << (BlockStmt
              << (BlockSeq << rule_block(
                    frame,
                    locals,
                    rule,
                    (rule / Ident)->location(),
                    doc_name)));
      }
    }

    block
      << (ObjectInsertStmt << (Operand << (String ^ key))
                           << (Operand << (LocalRef ^ doc_name))
                           << (LocalRef ^ obj_name));
    return block;
  }

  PassDef add_plans()
  {
    // add the plans required to satisfy the entry points
    return {
      "add_plans",
      wf_ir_add_plans,
      dir::bottomup | dir::once,
      {
        In(EntryPointSeq) *
            (T(EntryPoint)[EntryPoint]
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

          Node frame = _(EntryPoint);
          Nodes locals;
          Location obj_name = frame->fresh({"obj"});
          locals.push_back(Local << (Ident ^ obj_name));
          Node block = Block << (MakeObjectStmt << (LocalRef ^ obj_name));
          if (plan == Rule)
          {
            block
              << (BlockStmt
                  << (BlockSeq << rule_block(
                        frame, locals, plan, {"result"}, obj_name)));
          }
          else
          {
            block
              << (BlockStmt
                  << (BlockSeq << document_block(
                        frame, locals, plan, {"result"}, obj_name)));
          }

          block << (ResultSetAddStmt << (LocalRef ^ obj_name));

          return Plan << (Ident ^ _(Ident)) << (LocalSeq << locals)
                      << (BlockSeq << block);
        },

        In(Policy) * T(EntryPointSeq)[EntryPointSeq] >>
          [](Match& _) { return PlanSeq << *_[EntryPointSeq]; },

      }};
  }

  PassDef index_strings_locals()
  {
    auto locals = std::make_shared<std::map<std::string, int>>();
    auto strings = std::make_shared<std::map<std::string, int>>();
    // replace all local refs with their indices in the frame
    PassDef pass = {
      "index_strings_locals",
      wf_ir,
      dir::bottomup | dir::once,
      {
        In(Operand) * T(String)[String] >>
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

        In(Policy) * (T(Static) << T(PathSeq)[PathSeq]) >>
          [strings](Match& _) {
            return Static << to_stringseq(StringSeq, *strings) << _(PathSeq);
          },

        T(LocalRef)[LocalRef] >>
          [locals](Match& _) {
            std::string name(_(LocalRef)->location().view());
            auto it = locals->find(name);
            if (it == locals->end())
            {
              size_t index = locals->size();
              (*locals)[name] = index;
              return LocalIndex ^ std::to_string(index);
            }
            else
            {
              return LocalIndex ^ std::to_string(it->second);
            }
          },

        In(Function, Plan) * T(LocalSeq) >>
          [](Match& _) -> Node { return nullptr; },

        In(Function, ParameterSeq) * (T(Local) << T(Ident)[Ident]) >>
          [locals](Match& _) {
            std::string name(_(Ident)->location().view());
            auto it = locals->find(name);
            if (it == locals->end())
            {
              size_t index = locals->size();
              (*locals)[name] = index;
              return LocalIndex ^ std::to_string(index);
            }
            else
            {
              return LocalIndex ^ std::to_string(it->second);
            }
          },

        In(IR) * (T(BaseDocument) << T(json::Object)[Data]) *
            T(VirtualDocument) * T(Policy)[Policy] >>
          [](Match& _) { return Seq << (Data << _(Data)) << _(Policy); },
      }};

    pass.pre([strings, locals](Node) {
      strings->clear();
      locals->clear();

      locals->emplace("input", 0);
      locals->emplace("data", 1);
      return 0;
    });

    return pass;
  }
}

namespace rego
{
  Rewriter to_ir()
  {
    return {
      "to_ir",
      {documents(), lift_functions(), add_plans(), index_strings_locals()},
      wf_ir_input};
  }
}