#include "unify.hh"

namespace
{
  using namespace rego;
  using namespace wf::ops;

  using SkipSet = std::shared_ptr<std::set<std::string>>;

  struct SkipPrefix
  {
    std::size_t length;
    std::string path;
  };

  SkipPrefix skip_prefix_ref(const SkipSet& skips, Node ref)
  {
    Node head = (ref / RefHead)->front();
    std::string path = std::string(head->location().view());
    Node refargseq = (ref / RefArgSeq);
    std::size_t skip = 0;
    std::string longest_path = "";
    for (std::size_t i = 0; i < refargseq->size(); i++)
    {
      Node refarg = refargseq->at(i);
      std::string refarg_str;
      if (refarg->type() == RefArgDot)
      {
        refarg_str = "." + std::string(refarg->front()->location().view());
      }
      else if (refarg->type() == RefArgBrack)
      {
        refarg_str = strip_quotes(to_key(refarg->front()));
        if (!all_alnum(refarg_str))
        {
          refarg_str = "[\"" + refarg_str + "\"]";
        }
        else
        {
          refarg_str = "." + refarg_str;
        }
      }
      else
      {
        return {0, ""};
      }

      path = path + refarg_str;
      if (contains(skips, path))
      {
        skip = i + 1;
        longest_path = path;
      }
    }

    return {skip, longest_path};
  }

  void find_used_builtins(
    const Node& node, BuiltIns builtins, std::set<std::string>& used_builtins)
  {
    if (node->type() == Var)
    {
      std::string path = std::string(node->location().view());
      if (builtins->is_builtin(path))
      {
        used_builtins.insert(path);
      }
    }
    else if (node->type() == Ref)
    {
      Node refhead = (node / RefHead)->front();
      if (refhead->type() == Var)
      {
        std::ostringstream oss;
        oss << refhead->location().view();
        for (auto& refarg : *(node / RefArgSeq))
        {
          if (refarg->type() == RefArgDot)
          {
            oss << "." << refarg->front()->location().view();
          }
          else if (refarg->type() == RefArgBrack)
          {
            oss << "[" << strip_quotes(to_key(refarg->front())) << "]";
          }
        }

        std::string path = oss.str();
        if (builtins->is_builtin(path))
        {
          used_builtins.insert(path);
        }
      }
      else
      {
        find_used_builtins(refhead, builtins, used_builtins);
      }
    }
    else
    {
      for (auto& child : *node)
      {
        find_used_builtins(child, builtins, used_builtins);
      }
    }
  }
}

namespace rego
{
  // Elides references to skips for efficiency. Includes built-ins.
  PassDef skip_refs(BuiltIns builtins)
  {
    SkipSet skips = std::make_shared<std::set<std::string>>();

    PassDef skip_refs{
      "skip_refs",
      wf_pass_skip_refs,
      dir::bottomup | dir::once,
      {
        T(Ref)[Ref]([skips](auto& n) {
          return skip_prefix_ref(skips, n.front()).length > 0;
        }) >>
          [skips](Match& _) {
            ACTION();
            SkipPrefix skip = skip_prefix_ref(skips, _(Ref));
            Node refargseq = (_(Ref) / RefArgSeq);
            refargseq->erase(
              refargseq->begin(), refargseq->begin() + skip.length);

            return Ref << (RefHead << (Var ^ skip.path)) << refargseq;
          },

      }};

    skip_refs.pre(Rego, [skips](Node node) {
      Node skipseq = node / SkipSeq;
      for (Node skip : *skipseq)
      {
        std::string path = std::string((skip / Key)->location().view());
        skips->insert(path);
      }

      // get all rule symbols
      Nodes rules;
      node->get_symbols(
        rules, [](Node n) { return contains(RuleTypes, n->type()); });
      for (auto rule : rules)
      {
        std::string path = std::string((rule / Var)->location().view());
        skips->insert(path);
      }

      // get all module symbols
      std::set<Token> module_types = {Submodule};
      Nodes submodules;
      node->get_symbols(submodules, [](Node n) { return n == Submodule; });
      for (auto submodule : submodules)
      {
        std::string path = std::string((submodule / Key)->location().view());
        skips->insert(path);
      }

      return 0;
    });

    skip_refs.post(Rego, [skips, builtins](Node node) {
      std::set<std::string> used_builtins;
      find_used_builtins(node, builtins, used_builtins);
      auto added_builtins = std::make_shared<std::set<std::string>>();
      Node skipseq = node / SkipSeq;
      int changes = 0;
      for (auto& builtin : used_builtins)
      {
        if (!contains(added_builtins, builtin))
        {
          skipseq << (Skip << (Key ^ builtin) << (BuiltInHook ^ builtin));
          added_builtins->insert(builtin);
          changes++;
        }
      }

      skips->clear();

      return changes;
    });

    return skip_refs;
  }
}