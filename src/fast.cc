#include "fast.hh"

#include "internal.hh"
#include "trieste/json.h"
#include "trieste/logging.h"

#include <chrono>

namespace
{
  using namespace rego;

  PassDef prep()
  {
    return {
      "prep",
      wf_fast_prep,
      dir::bottomup,
      {
        In(Rego) *
            (T(ModuleSeq)
             << (T(Module)
                 << ((T(Package)
                      << (T(Ref) << ((T(RefHead) << T(Var)[Var])) *
                            (T(RefArgSeq) << End))) *
                     T(Version) * (T(ImportSeq) << End) *
                     T(Policy)[Policy]))) >>
          [](Match& _) {
            ACTION();
            return FastData << (Key ^ "data")
                            << (FastModule << (Key ^ _(Var)) << _(Policy));
          },

        In(Rego) * (T(DataSeq) << End) >> [](Match&) { return nullptr; },

        In(Rego) * (T(Input) << T(DataTerm)[Val]) >>
          [](Match& _) {
            ACTION();
            return Input << (Key ^ "input") << _(Val);
          },

        In(Query) *
            (T(Literal)
             << (T(Expr)
                 << (T(ExprInfix) << ((T(Expr) << (T(Term) << T(Var)[Var]))) *
                       (T(InfixOperator) << T(AssignOperator)) *
                       T(Expr)[Expr]))) >>
          [](Match& _) {
            ACTION();
            return FastLocal << _(Var) << _(Expr);
          },

        In(Query) * (T(Literal) << (T(Expr) / T(NotExpr))[Expr]) >>
          [](Match& _) {
            ACTION();
            return _(Expr);
          },

        In(RuleRef) *
            (T(Ref) << ((T(RefHead) << T(Var)[Var]) * (T(RefArgSeq) << End))) >>
          [](Match& _) {
            ACTION();
            return _(Var);
          },

        In(Policy) *
            (T(Rule)[Rule]
             << (T(False) *
                 (T(RuleHead)
                  << ((T(RuleRef) << T(Var)[Id]) *
                      (T(RuleHeadComp) << T(Expr)[Expr]))) *
                 T(RuleBodySeq)[RuleBodySeq])) >>
          [](Match& _) {
            ACTION();
            std::size_t num_items = _(RuleBodySeq)->size();

            Node val = _(Expr);
            if (is_constant(val->front()))
            {
              val = val->front();
            }

            if (num_items == 0)
            {
              return FastRule << _(Id) << FastBody << val << (Int ^ "0");
            }

            if (num_items == 1)
            {
              Node item = _(RuleBodySeq)->front();
              Node body = FastBody ^ item;
              body->insert(body->end(), item->begin(), item->end());
              return FastRule << _(Id) << body << val << (Int ^ "0");
            }

            return err(
              _(RuleBodySeq), "Only one rule body is supported in fast rego");
          },

        In(Policy) *
            (T(Rule)[Rule]
             << (T(True) *
                 (T(RuleHead)
                  << ((T(RuleRef) << T(Var)[Id]) *
                      (T(RuleHeadComp) << T(Expr)[Expr]))) *
                 (T(RuleBodySeq) << End))) >>
          [](Match& _) {
            ACTION();
            std::size_t rank = std::numeric_limits<std::uint16_t>::max();
            Node value = _(Expr);
            if (is_constant(value->front()))
            {
              value = value->front();
            }

            return FastRule << _(Id) << FastBody << value
                            << (Int ^ std::to_string(rank));
          },

        In(Expr) *
            (T(ExprCall)
             << ((T(Ref)[Ref]
                  << ((T(RefHead) << T(Var, "time")) *
                      (T(RefArgSeq) << (T(RefArgDot) << T(Var, "now_ns"))))) *
                 (T(ExprSeq) << End))) >>
          [](Match& _) {
            ACTION();
            return FastTimeNowNS ^ _(Ref);
          },

        In(Expr) *
            (T(ExprCall)
             << ((T(Ref)
                  << ((T(RefHead)[RefHead] << T(Var, "regex")) *
                      (T(RefArgSeq) << (T(RefArgDot) << T(Var, "match"))))) *
                 (T(ExprSeq) << (T(Expr)[Lhs] * T(Expr)[Rhs])))) >>
          [](Match& _) {
            ACTION();
            return (FastRegexMatch ^ _(RefHead)) << _(Lhs) << _(Rhs);
          },

        In(DataTerm) * T(DataObject)[DataObject] >>
          [](Match& _) {
            ACTION();
            return FastObject << *_[DataObject];
          },

        In(Term) * T(Object)[Object] >>
          [](Match& _) {
            ACTION();
            return FastObject << *_[Object];
          },

        In(DataObject) * T(DataObjectItem)[DataObjectItem] >>
          [](Match& _) {
            ACTION();
            return FastObjectItem << *_[DataObjectItem];
          },

        In(Object) * T(ObjectItem)[ObjectItem] >>
          [](Match& _) {
            ACTION();
            return FastObjectItem << *_[ObjectItem];
          },

        In(Object) *
            (T(ObjectItem)
             << ((T(Expr) << (T(Term) << (T(Scalar) << T(JSONString)[Key]))) *
                 T(Expr)[Val])) >>
          [](Match& _) {
            ACTION();
            return ObjectItem << _(Key) << _(Val);
          },

        In(Scalar) * (T(String) << T(RawString)[RawString]) >>
          [](Match& _) {
            ACTION();
            std::string raw_string =
              std::string(_(RawString)->location().view());
            raw_string = raw_string.substr(1, raw_string.size() - 2);
            std::string json_string = '"' + json::escape(raw_string) + '"';
            return JSONString ^ json_string;
          },

        In(Scalar) * (T(String) << T(JSONString)[JSONString]) >>
          [](Match& _) {
            ACTION();
            return _(JSONString);
          },
      }};
  }

  Node resolve_rule(Nodes values)
  {
    std::size_t rank = std::numeric_limits<std::uint16_t>::max();
    BigInt best_index(rank);
    Nodes best_terms;
    for (auto& value : values)
    {
      Node rule_body = value / FastBody;
      Node rule_value = value / Val;
      Node rule_index = value / Idx;
      if (rule_body->type() == FastBody)
      {
        return nullptr;
      }

      if (rule_body->type() == False)
      {
        continue;
      }

      BigInt index = BigInt(rule_index->location());
      if (rule_body->type() == True)
      {
        if (index == best_index)
        {
          best_terms.push_back(rule_value);
        }
        else
        {
          best_terms.clear();
          best_terms.push_back(rule_value);
          best_index = index;
        }
      }
      else
      {
        logging::Output() << rule_body;
      }
    }

    if(best_terms.size() == 1){
      return best_terms.front();
    }

    return TermSet << best_terms;
  }

  Nodes follow_ref(Node ref)
  {
    logging::Output() << ref;
    Node head_var = ref->front()->front();
    Node args = ref->back();
    Nodes nodes = head_var->lookup();
    if (nodes.size() == 0)
    {
      return nodes;
    }

    Node node = nodes.front();

    if (node->in({FastObjectItem, Input}))
    {
      node = node / Val;
    }

    for (auto& arg : *args)
    {
      if (node->in({DataTerm, Term}))
      {
        node = node->front();
      }

      Node arg_var = arg->front();
      nodes = node->lookdown(arg_var->location());
      if (nodes.size() != 1)
      {
        break;
      }

      node = nodes.front();
      if (node->type() == FastObjectItem)
      {
        node = node / Val;
      }
    }

    return nodes;
  }

  PassDef resolve()
  {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto now_ns = duration_cast<std::chrono::nanoseconds>(now).count();
    auto cached_time = std::to_string(now_ns);

    return {
      "unify",
      wf_fast_unify,
      dir::bottomup,
      {
        T(Expr)
            << (T(Term)[Term]
                << T(Scalar, Array, FastObject, Set, Undefined)) >>
          [](Match& _) {
            ACTION();
            return _(Term);
          },

        T(NotExpr) << T(Term)[Term] >>
          [](Match& _) {
            ACTION();
            if (is_falsy(_(Term)))
            {
              return Term << (Scalar << (True ^ "true"));
            }

            return Term << (Scalar << (False ^ "false"));
          },

        In(Expr) * T(FastTimeNowNS) >>
          [cached_time](Match& _) {
            ACTION();
            return Term << (Scalar << (Int ^ cached_time));
          },

        In(Term) * T(Var)[Var]([](NodeRange n) {
          Node var = n.front();
          Nodes nodes = var->lookup();
          if (nodes.empty())
          {
            return false;
          }

          Node maybe_rule = nodes.front();
          if (maybe_rule->type() == FastRule)
          {
            Node value = resolve_rule(nodes);
            return value != nullptr;
          }

          if (nodes.size() != 1)
          {
            return false;
          }

          Node node = nodes.front() / Val;
          return node->in({DataTerm, Term});
        }) >>
          [](Match& _) {
            ACTION();
            Nodes nodes = (_(Var))->lookup();
            Node node = nodes.front();
            if (node->type() == FastRule)
            {
              node = resolve_rule(nodes);
            }
            else if (node->type() == FastObjectItem)
            {
              node = node->back();
            }

            if (node->in({Term, DataTerm}))
            {
              return node->front();
            }

            return node;
          },

        In(Term) * T(Ref)[Ref]([](NodeRange n) {
          Nodes nodes = follow_ref(n.front());
          if (nodes.size() == 0)
          {
            return false;
          }

          Node node = nodes.front();
          if (nodes.size() == 1)
          {
            node = node->back();
            if (node->in({Term, DataTerm}))
            {
              return true;
            }
          }

          logging::Output() << node;
          if (node->type() == FastRule)
          {
            Node value = resolve_rule(nodes);
            return value != nullptr;
          }

          return false;
        }) >>
          [](Match& _) {
            ACTION();
            Nodes nodes = follow_ref(_(Ref));
            Node node = nodes.front();
            if (node->type() == FastRule)
            {
              node = resolve_rule(nodes);
            }
            else if (node->type() == FastObjectItem)
            {
              node = node->back();
            }

            if (node->in({Term, DataTerm}))
            {
              return node->front();
            }

            return node;
          },

        In(Expr) *
            (T(ExprInfix)
             << (T(Term)[Lhs] * (T(InfixOperator) << T(ArithOperator)[Op]) *
                 T(Term)[Rhs])) >>
          [](Match& _) {
            ACTION();
            return Term
              << (Scalar << Resolver::arithinfix(
                    _(Op)->front(), _(Lhs), _(Rhs)));
          },

        In(Expr) *
            (T(ExprInfix)
             << (T(Term)[Lhs] * (T(InfixOperator) << T(BoolOperator)[Op]) *
                 T(Term)[Rhs])) >>
          [](Match& _) {
            ACTION();
            return Term
              << (Scalar << Resolver::boolinfix(
                    _(Op)->front(), _(Lhs), _(Rhs)));
          },

        In(Expr) *
            (T(ExprInfix)
             << (T(Term)[Lhs] * (T(InfixOperator) << T(BinOperator)[Op]) *
                 T(Term)[Rhs])) >>
          [](Match& _) {
            ACTION();
            return Term
              << (Scalar << Resolver::bininfix(_(Op)->front(), _(Lhs), _(Rhs)));
          },

        In(FastRule) * (T(FastBody)[FastBody] << (T(Term)++ * End)) >>
          [](Match& _) {
            ACTION();
            for (auto& node : *_(FastBody))
            {
              if (is_falsy(node))
              {
                return False ^ "false";
              }
            }

            return True ^ "true";
          },

        In(Rego) *
            ((T(Query)[Query] << (T(Term, Binding)++ * End)) * T(Input) * T(FastData)) >>
          [](Match& _) {
            ACTION();
            // TODO add code to rewrite the query as a list of results (crib from resolve_query)
            return _(Query);
          },
      }};
  }

  PassDef results()
  {
    return {
      "results",
      wf_result,
      dir::bottomup,
      {
        In(Top) * (T(Rego) << (T(Query) << T(rego::Result)++[rego::Result])) >>
          [](Match& _) {
            ACTION();
            if (_[rego::Result].empty())
            {
              return Undefined ^ "";
            }
            return Results << _[rego::Result];
          },

        (T(Array) / T(Set)) * T(Error)[Error] >>
          [](Match& _) {
            ACTION();
            return _(Error);
          },

        T(Object) * (T(ObjectItem) << (T(Key) * T(Error)[Error])) >>
          [](Match& _) {
            ACTION();
            return _(Error);
          },

        T(Scalar) * T(Error)[Error] >>
          [](Match& _) {
            ACTION();
            return _(Error);
          },

        T(Term) * T(Error)[Error] >>
          [](Match& _) {
            ACTION();
            return _(Error);
          },
      }};    
  }
}

namespace rego
{
  Rewriter fast()
  {
    return {
      "fast_rego",
      {
        prep(),
        resolve(),
        results()
      },
      rego::wf_fast_input};
  }
}