#include "fast.hh"

#include "internal.hh"
#include "re2/re2.h"
#include "rego.hh"
#include "trieste/json.h"
#include "trieste/logging.h"
#include "trieste/rewrite.h"

#include <chrono>
#include <optional>

namespace
{
  using namespace rego;

  using NodeCache = std::shared_ptr<std::map<Location, Node>>;

  bool fast_is_constant(Node node)
  {
    std::vector<Node> stack = {node};
    while (!stack.empty())
    {
      node = stack.back();
      stack.pop_back();
      if (node->type() == Expr)
      {
        logging::Debug() << "Not constant: Expr";
        return false;
      }

      if (node->in({Term, DataTerm}))
      {
        node = node->front();
      }

      if (node->in({Undefined, Scalar}))
      {
        continue;
      }

      if (node->in({Ref, Var}))
      {
        logging::Debug() << "Not constant: Ref or Var";
        return false;
      }

      if (node->type() == Array)
      {
        stack.insert(stack.end(), node->begin(), node->end());
      }
      else if (node->in({FastObject, FastDataObject}))
      {
        for (auto& child : *node)
        {
          stack.push_back(child / Val);
        }
      }
      else
      {
        logging::Debug() << "Not constant: " << node->type().str();
        return false;
      }
    }

    return true;
  }

  bool contains_multiple_outputs(Node term)
  {
    if (term->type() == TermSet)
    {
      return true;
    }

    for (auto& child : *term)
    {
      if (contains_multiple_outputs(child))
      {
        return true;
      }
    }

    return false;
  }

  Node resolve_rule(Nodes values)
  {
    logging::Debug() << "resolving rule";
    Nodes best_terms;
    std::size_t rank = std::numeric_limits<std::uint16_t>::max();
    BigInt best_index(rank);

    for (auto& value : values)
    {
      Node rule_body = value / FastBody;
      Node rule_value = value / Val;
      Node rule_index = value / Idx;
      if (rule_body->type() == FastBody)
      {
        logging::Debug() << "rule body is unresolved";
        return nullptr;
      }

      if (rule_body->type() == False)
      {
        continue;
      }

      BigInt index = BigInt(rule_index->location());
      if (rule_body->type() == True)
      {
        if (is_undefined(rule_value))
        {
          if (best_terms.empty())
          {
            best_terms.push_back(rule_value);
            best_index = index;
          }
        }
        else
        {
          if (index == best_index)
          {
            best_terms.push_back(rule_value);
          }
          else if (index < best_index)
          {
            best_terms.clear();
            best_terms.push_back(rule_value);
            best_index = index;
          }
        }
      }
    }

    if (best_terms.size() == 1)
    {
      return best_terms.front();
    }

    if (best_terms.size() == 0)
    {
      return Term << Undefined;
    }

    return Resolver::reduce_termset(TermSet << best_terms);
  }

  std::optional<Node> get_head(Nodes nodes)
  {
    Node node = nodes.front();

    if (node->type() == FastRule)
    {
      node = resolve_rule(nodes);
      if (node == nullptr)
      {
        return std::nullopt;
      }

      if (node->type() == TermSet)
      {
        return std::nullopt;
      }
    }
    else if (nodes.size() > 1)
    {
      logging::Debug() << "Multiple nodes found: " << nodes.size();
      return std::nullopt;
    }

    if (node->in(
          {FastDataObjectItem,
           FastObjectItem,
           Input,
           FastDataModule,
           Binding,
           FastLocal}))
    {
      node = node / Val;
    }

    return node;
  }

  std::optional<Nodes> follow_ref(Node ref)
  {
    Node head_var = ref->front()->front();
    Node args = ref->back();
    logging::Debug() << "Following ref head=" << head_var->location().view()
                     << " with " << args->size() << " args";
    Nodes nodes = head_var->lookup();
    if (nodes.size() == 0)
    {
      // head does not exist
      return nodes;
    }

    if (args->empty())
    {
      // no args, we're done
      return nodes;
    }

    for (auto& arg : *args)
    {
      auto maybe_head = get_head(nodes);
      if (!maybe_head.has_value())
      {
        return std::nullopt;
      }

      Node head = maybe_head.value();

      if (head->in({DataTerm, Term}))
      {
        head = head->front();
      }

      if (arg->type() == RefArgDot)
      {
        Node arg_var = arg->front();
        logging::Debug() << "." << arg_var->location().view();
        nodes = head->lookdown(arg_var->location());
      }
      else if (arg->type() == RefArgBrack)
      {
        Node key = arg->front();
        logging::Debug() << "[" << key->location().view() << "]";
        if (!key->in({Int, JSONString}))
        {
          // only int and string keys are supported
          return std::nullopt;
        }

        if (head->type() == Array)
        {
          logging::Debug() << "Array lookup";
          // array
          nodes = Nodes{};
          auto maybe_index = try_get_int(key);
          if (maybe_index.has_value())
          {
            auto index = maybe_index.value().to_size();
            if (index < head->size())
            {
              nodes.push_back(head->at(index));
            }
            else
            {
              nodes.push_back(err(
                key,
                "Only integer keys are supported on arrays",
                RegoTypeError));
              return nodes;
            }
          }
        }
        else if (head->in({FastObject, FastDataObject}))
        {
          logging::Debug() << "Object lookup";
          // object
          if (key->type() == JSONString)
          {
            Location key_loc = key->location();
            if (is_quoted(key_loc.view()))
            {
              key_loc.pos += 1;
              key_loc.len -= 2;
            }
            nodes = head->lookdown(key->location());
          }
          else
          {
            nodes.push_back(err(
              key, "Only string keys are supported on objects", RegoTypeError));
            return nodes;
          }
        }
        else
        {
          logging::Debug() << "Unexpected collection: " << head;
          return std::nullopt;
        }
      }
      else
      {
        nodes.push_back(err(arg, "Unexpected ref arg", RegoTypeError));
        return nodes;
      }
    }

    return nodes;
  }

  Node resolve_ref(Node ref)
  {
    logging::Debug() << "Validating ref: " << ref->location().view();

    Nodes nodes;
    if (ref->type() == Ref)
    {
      auto maybe_nodes = follow_ref(ref);
      if (!maybe_nodes.has_value())
      {
        return nullptr;
      }

      nodes = maybe_nodes.value();
    }
    else
    {
      nodes = ref->lookup();
    }

    if (nodes.empty())
    {
      logging::Debug() << "No nodes found";
      return nullptr;
    }

    auto maybe_head = get_head(nodes);
    if (!maybe_head.has_value())
    {
      logging::Debug() << "Cannot resolve head";
      return nullptr;
    }

    Node node = maybe_head.value();
    if (node->in({Term, DataTerm}) && fast_is_constant(node))
    {
      node = node->front();
    }
    else
    {
      logging::Debug() << "Unresolved expression: " << node;
      return nullptr;
    }

    logging::Debug() << "resolved ref: " << ref->location().view()
                     << " to: " << node;
    return node;
  }

  PassDef prep()
  {
    return {
      "prep",
      wf_fast_prep,
      dir::bottomup | dir::once,
      {
        In(Rego) *
            (T(Query)[Query] * T(Input)[Input] *
             (T(DataSeq)[DataSeq] << (T(FastDataModule)++ * End)) *
             (T(ModuleSeq)[ModuleSeq] << (T(FastModule)++ * End))) >>
          [](Match& _) {
            ACTION();
            auto input = Input << (Key ^ "input") << *_[Input];
            auto moduleseq = FastModuleSeq << *_[ModuleSeq] << *_[DataSeq];
            auto fastdata = FastData << (Key ^ "data") << moduleseq;
            return Seq << _(Query) << input << fastdata;
          },

        In(ModuleSeq) *
            (T(Module)
             << ((T(Package)
                  << (T(Ref) << ((T(RefHead) << T(Var)[Var])) *
                        (T(RefArgSeq) << End))) *
                 T(Version) * (T(ImportSeq) << End) * T(Policy)[Policy])) >>
          [](Match& _) {
            ACTION();
            return FastModule << (Key ^ _(Var)) << _(Policy);
          },

        In(DataSeq) *
            (T(Data) << (T(DataTerm) << T(FastDataObject)[FastDataObject])) >>
          [](Match& _) {
            ACTION();
            Node seq = NodeDef::create(Seq);
            for (auto& item : *_(FastDataObject))
            {
              auto key = item / Key;
              auto val = item / Val;
              seq << (FastDataModule << key << (DataTerm << *val));
            }
            return seq;
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
            if (fast_is_constant(val->front()))
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

            // Else precedence is per rule chain. This means that
            // each set of else sequences must be evaluated individually
            // and resolve to a value (or undefined). This is to deal
            // with rules of the following kind:
            //   conflict_1 {
            //     false
            //   } else = true {
            //     true
            //   }
            //
            //   conflict_1 = false
            //
            // In this case, conflict_1 will resolve to two (conflicting)
            // values. This is why we have to create a proxy rule below.
            // Do NOT optimise this away.
            Node rule_seq = NodeDef::create(Seq);

            // first we create all the variants of the rule
            Node rule_id = Var ^ _.fresh(_(Id)->location());
            for (std::size_t i = 0; i < num_items; ++i)
            {
              Node item = _(RuleBodySeq)->at(i);
              Node body;
              if (item == Query)
              {
                body = FastBody ^ item;
                body->insert(body->end(), item->begin(), item->end());
                rule_seq
                  << (FastRule << rule_id->clone() << body << val->clone()
                               << (Int ^ std::to_string(i)));
              }
              else if (item == Else)
              {
                Node item_val = item / Expr;
                if (is_constant(item_val->front()))
                {
                  item_val = item_val->front();
                }

                Node query = item / Query;
                body = FastBody ^ query;
                body->insert(body->end(), query->begin(), query->end());

                rule_seq
                  << (FastRule << rule_id->clone() << body << item_val
                               << (Int ^ std::to_string(i)));
              }
            }

            // then we create the proxy rule
            rule_seq
              << (FastRule << _(Id) << FastBody
                           << (Expr << (Term << (Var ^ rule_id)))
                           << (Int ^ "0"));

            return rule_seq;
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
            if (fast_is_constant(value->front()))
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

        In(Expr) *
            (T(ExprCall)
             << ((T(Ref)
                  << ((T(RefHead)[RefHead] << T(Var, "regex")) *
                      (T(RefArgSeq) << (T(RefArgDot) << T(Var, "is_valid"))))) *
                 (T(ExprSeq) << (T(Expr)[Lhs])))) >>
          [](Match& _) {
            ACTION();
            return (FastRegexIsValid ^ _(RefHead)) << _(Lhs);
          },

        T(ExprParens) << T(Expr)[Expr] >>
          [](Match& _) {
            ACTION();
            return _(Expr)->front();
          },

        T(Object)[Object] >>
          [](Match& _) {
            ACTION();
            return FastObject << *_[Object];
          },

        T(DataObject)[DataObject] >>
          [](Match& _) {
            ACTION();
            return FastDataObject << *_[DataObject];
          },

        In(DataObject) *
            (T(DataObjectItem)
             << ((T(DataTerm) << (T(Scalar) << T(JSONString)[Key]))) *
               T(DataTerm)[Val]) >>
          [](Match& _) {
            ACTION();
            Location loc = _(Key)->location();
            if (is_quoted(loc.view()))
            {
              loc.pos += 1;
              loc.len -= 2;
            }
            return FastDataObjectItem << (Key ^ loc) << (DataTerm << *_[Val]);
          },

        In(Object) *
            (T(ObjectItem)
             << ((T(Expr) << (T(Term) << (T(Scalar) << T(JSONString)[Key]))) *
                 T(Expr)[Val])) >>
          [](Match& _) {
            ACTION();
            Location loc = _(Key)->location();
            if (is_quoted(loc.view()))
            {
              loc.pos += 1;
              loc.len -= 2;
            }
            return FastObjectItem << (Key ^ loc) << _(Val);
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

  PassDef resolve()
  {
    auto fast_cache = std::make_shared<std::map<Location, Node>>();
    PassDef pass = {
      "resolve",
      wf_fast_resolve,
      dir::bottomup,
      {
        T(Expr)
            << (T(Term)[Term]
                << T(Scalar, Array, FastObject, FastDataObject, Undefined)) >>
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

        In(Expr) * T(FastTimeNowNS)[Key] >>
          [fast_cache](Match& _) {
            ACTION();
            auto key = _(Key)->location();
            Node time;
            if (!contains(fast_cache, key))
            {
              auto now = std::chrono::system_clock::now();
              auto now_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
              auto time_str = std::to_string(now_ns.time_since_epoch().count());
              time = Term << (Scalar << (Int ^ time_str));
              fast_cache->insert({key, time});
            }
            else
            {
              time = fast_cache->at(key)->clone();
            }

            return time;
          },

        In(Expr) *
            (T(FastRegexIsValid)
             << ((T(Term) << (T(Scalar) << T(JSONString)[Pattern])))) >>
          [](Match& _) {
            ACTION();
            std::string pattern(_(Pattern)->location().view());
            re2::RE2 re(pattern);
            if (re.ok())
            {
              return Term << (Scalar << (True ^ "true"));
            }

            return Term << (Scalar << (False ^ "false"));
          },

        In(Expr) *
            (T(FastRegexMatch)
             << (((T(Term) << (T(Scalar) << T(JSONString)[Pattern]))) *
                 ((T(Term) << (T(Scalar) << T(JSONString)[Val]))))) >>
          [](Match& _) {
            ACTION();
            std::string pattern(_(Pattern)->location().view());
            std::string value(_(Val)->location().view());
            pattern = strip_quotes(json::unescape(pattern));
            value = strip_quotes(json::unescape(value));
            re2::RE2 re(pattern);
            if (re.ok() && RE2::FullMatch(value, re))
            {
              return Term << (Scalar << (True ^ "true"));
            }

            return Term << (Scalar << (False ^ "false"));
          },

        In(Term) * T(Var)[Var] >> [](Match& _) -> Node {
          ACTION();
          Node node = resolve_ref(_(Var));
          if (node == nullptr)
          {
            return NoChange;
          }

          return node->clone();
        },

        In(Term) * T(Ref)[Ref] >> [](Match& _) -> Node {
          ACTION();
          Node node = resolve_ref(_(Ref));
          if (node == nullptr)
          {
            return NoChange;
          }
          return node->clone();
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

        In(Expr) * (T(UnaryExpr) << T(Term)[Term]) >>
          [](Match& _) {
            ACTION();
            return Term << (Scalar << Resolver::unary(_(Term)));
          },

        In(RefArgBrack) * (T(Term) << (T(Scalar) << T(Int, JSONString)[Key])) >>
          [](Match& _) {
            ACTION();
            return _(Key);
          },

        In(FastRule) *
            (T(FastBody)[FastBody] << (T(Term, FastLocal)++ * End)) >>
          [](Match& _) {
            ACTION();
            for (auto& node : *_(FastBody))
            {
              if (node->type() == Binding)
              {
                continue;
              }

              if (is_falsy(node))
              {
                return False ^ "false";
              }
            }

            return True ^ "true";
          },

        In(Query) * (T(FastLocal) << (T(Var)[Var] * T(Term)[Val])) >>
          [](Match& _) -> Node {
          ACTION();
          if (fast_is_constant(_(Val)))
          {
            return Binding << _(Var) << _(Val);
          }

          return NoChange;
        },

        In(Rego) *
            ((T(Query)[Query] << (T(Term, Binding)++ * End)) * T(Input) *
             T(FastData)) >>
          [](Match& _) {
            ACTION();
            Nodes results{rego::Result << Terms << Bindings};
            Node rulebody = _(Query);

            for (auto& child : *rulebody)
            {
              if (child->type() == Error)
              {
                results.back() / Terms << child;
                continue;
              }

              Node var = nullptr;
              Node term;
              if (child->type() == Binding)
              {
                var = (child / Var)->clone();
                term = (child / Val)->clone();
              }
              else
              {
                term = child->clone();
              }

              if (term->type() == TermSet)
              {
                if (term->size() == 0)
                {
                  term = Undefined;
                }
                else
                {
                  term = Resolver::reduce_termset(term);
                }
              }

              if (is_undefined(term))
              {
                continue;
              }

              std::string name = "";
              if (var != nullptr)
              {
                name = std::string(var->location().view());
              }

              if (term == TermSet)
              {
                Node termset = term;
                // there are multiple bindings/terms
                while (results.size() < termset->size())
                {
                  results.push_back(rego::Result << Terms << Bindings);
                }

                for (std::size_t i = 0; i < termset->size(); i++)
                {
                  term = termset->at(i);
                  if (contains_multiple_outputs(term))
                  {
                    results[i] / Terms << err(
                      term,
                      "complete rules must not produce multiple outputs",
                      EvalConflictError);
                  }
                  else if (name.empty())
                  {
                    results[i] / Terms << term;
                  }
                  else
                  {
                    results[i] / Bindings << (Binding << var->clone() << term);
                  }
                }
              }
              else if (results.size() == 1)
              {
                if (contains_multiple_outputs(term))
                {
                  results.back() / Terms << err(
                    term,
                    "complete rules must not produce multiple outputs",
                    EvalConflictError);
                }
                else if (name.empty())
                {
                  results.back() / Terms << term;
                }
                else if (name.find('$') == std::string::npos || name[0] == '$')
                {
                  results.back() / Bindings << (Binding << var << term);
                }
              }
            }

            if (
              results.size() == 1 && (results.back() / Terms)->empty() &&
              (results.back() / Bindings)->empty())
            {
              results.pop_back();
            }

            return Query << results;
          },
      }};

    pass.pre([fast_cache](Node) {
      fast_cache->clear();
      return 0;
    });

    return pass;
  }

  PassDef results()
  {
    return {
      "results",
      wf_result,
      dir::bottomup | dir::once,
      {
        In(Top) * (T(Rego) << (T(Query) << T(rego::Result)++ [rego::Result])) >>
          [](Match& _) {
            ACTION();
            if (_[rego::Result].empty())
            {
              return Undefined ^ "";
            }
            return Results << _[rego::Result];
          },

        T(FastObjectItem) << (T(Key)[Key] * T(Term)[Val]) >>
          [](Match& _) {
            ACTION();
            return ObjectItem << (Term << (Scalar << (JSONString ^ _(Key))))
                              << _(Val);
          },

        T(FastDataObjectItem) << (T(Key)[Key] * T(DataTerm)[Val]) >>
          [](Match& _) {
            ACTION();
            return ObjectItem << (Term << (Scalar << (JSONString ^ _(Key))))
                              << (Term << *_[Val]);
          },

        T(FastObject, FastDataObject)[Object] >>
          [](Match& _) {
            ACTION();
            return Object << *_[Object];
          },
      }};
  }
}

namespace rego
{
  Rewriter fast()
  {
    return {"fast_rego", {prep(), resolve(), results()}, rego::wf_fast_input};
  }
}