#include "builtins.hh"
#include "rego.hh"
#include "trieste/parse.h"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node is_valid_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "query")
                    << (bi::Description ^ "the GraphQA query")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any))))))
        << (bi::Arg << (bi::Name ^ "schema")
                    << (bi::Description ^ "the GraphQL schema")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any)))))))
    << (bi::Result << (bi::Name ^ "output")
                   << (bi::Description ^
                       "`true` if the query is valid under the given schema. "
                       "`false` otherwise.")
                   << (bi::Type << bi::Boolean));

  Node parse_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "query")
                    << (bi::Description ^ "the GraphQA query")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any))))))
        << (bi::Arg << (bi::Name ^ "schema")
                    << (bi::Description ^ "the GraphQL schema")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any)))))))
    << (bi::Result << (bi::Name ^ "output")
                   << (bi::Description ^
                       "`output` is of the form `[query_ast, schema_ast]`. If "
                       "the GraphQL query is valid given the provided schema, "
                       "then `query_ast` and `schema_ast` are objects "
                       "describing the ASTs for the query and schema.")
                   << (bi::Type
                       << (bi::StaticArray
                           << (bi::Type
                               << (bi::DynamicObject << (bi::Type << bi::Any)
                                                     << (bi::Type << bi::Any)))
                           << (bi::Type
                               << (bi::DynamicObject
                                   << (bi::Type << bi::Any)
                                   << (bi::Type << bi::Any))))));

  Node parse_and_verify_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "query")
                    << (bi::Description ^ "the GraphQA query")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any))))))
        << (bi::Arg << (bi::Name ^ "schema")
                    << (bi::Description ^ "the GraphQL schema")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any)))))))
    << (bi::Result
        << (bi::Name ^ "output")
        << (bi::Description ^
            "`output` is of the form `[valid, query_ast, schema_ast]`. If the "
            "query is valid given the provided schema, then `valid` is `true`, "
            "and `query_ast` and `schema_ast` are objects describing the ASTs "
            "for the GraphQL query and schema. Otherwise, `valid` is `false` "
            "and `query_ast` and `schema_ast` are `{}`.")
        << (bi::Type
            << (bi::StaticArray
                << (bi::Type << bi::Boolean)
                << (bi::Type
                    << (bi::DynamicObject << (bi::Type << bi::Any)
                                          << (bi::Type << bi::Any)))
                << (bi::Type
                    << (bi::DynamicObject << (bi::Type << bi::Any)
                                          << (bi::Type << bi::Any))))));

  Node parse_query_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "query")
                             << (bi::Description ^ "GraphQL query string")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "output")
                 << (bi::Description ^ "AST object for the GraphQL query")
                 << (bi::Type
                     << (bi::DynamicObject << (bi::Type << bi::Any)
                                           << (bi::Type << bi::Any))));

  Node parse_schema_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "query")
                             << (bi::Description ^ "GraphQL schema string")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "output")
                 << (bi::Description ^ "AST object for the GraphQL schema")
                 << (bi::Type
                     << (bi::DynamicObject << (bi::Type << bi::Any)
                                           << (bi::Type << bi::Any))));

  Node schema_is_valid_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "query")
                             << (bi::Description ^ "GraphQL schema string")
                             << (bi::Type << bi::String)))
             << (bi::Result << (bi::Name ^ "output")
                            << (bi::Description ^
                                "`true` if the schema is a valid GraphQL "
                                "schema. `false` otherwise.")
                            << (bi::Type << bi::Boolean));
}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> graphql()
    {
      const char* Message = "GraphQL is not supported";
      return {
        BuiltInDef::placeholder({"graphql.is_valid"}, is_valid_decl, Message),
        BuiltInDef::placeholder({"graphql.parse"}, parse_decl, Message),
        BuiltInDef::placeholder(
          {"graphql.parse_and_verify"}, parse_and_verify_decl, Message),
        BuiltInDef::placeholder(
          {"graphql.parse_query"}, parse_query_decl, Message),
        BuiltInDef::placeholder(
          {"graphql.parse_schema"}, parse_schema_decl, Message),
        BuiltInDef::placeholder(
          {"graphql.schema_is_valid"}, schema_is_valid_decl, Message),
      };
    }
  }
}