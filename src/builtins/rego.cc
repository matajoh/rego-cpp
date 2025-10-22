#include "builtins.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  namespace metadata
  {
    Node chain_decl =
      bi::Decl << bi::ArgSeq
               << (bi::Result
                   << (bi::Name ^ "chain")
                   << (bi::Description ^
                       "each array entry represents a chain in the path "
                       "ancestry (chain) "
                       "of the active rule that also has declared annotations")
                   << (bi::Type
                       << (bi::DynamicArray << (bi::Type << bi::Any))));

    Node rule_decl = bi::Decl
      << bi::ArgSeq
      << (bi::Result << (bi::Name ^ "output")
                     << (bi::Description ^
                         "\"rule\" scope annotations for this rule; empty "
                         "object if no annotations exist")
                     << (bi::Type << bi::Any));
  }

  Node parse_module_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "filename")
                       << (bi::Description ^
                           "file name to attached to AST nodes' lcoations")
                       << (bi::Type << bi::String))
                   << (bi::Arg << (bi::Name ^ "rego")
                               << (bi::Description ^ "Rego module")
                               << (bi::Type << bi::String)))
    << (bi::Result << (bi::Name ^ "output")
                   << (bi::Description ^ "AST object for the Rego module")
                   << (bi::Type
                       << (bi::DynamicObject << (bi::Type << bi::String)
                                             << (bi::Type << bi::Any))));
}

namespace rego {
  namespace builtins {
    std::vector<BuiltIn> rego()
    {
      return {
        BuiltInDef::placeholder({"rego.metadata.chain"}, metadata::chain_decl, "Rego metadata not supported"),
        BuiltInDef::placeholder({"rego.metadata.rule"}, metadata::rule_decl, "Rego metadata not supported"),
        BuiltInDef::placeholder({"rego.parse_module"}, parse_module_decl, "Rego module parsing not supported"),
      };
    }
  }
}