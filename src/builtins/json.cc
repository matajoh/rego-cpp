#include "builtins.h"
#include "rego.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node filter_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "object")
                       << (bi::Description ^ "object to filter")
                       << (bi::Type
                           << (bi::DynamicObject << (bi::Type << bi::Any)
                                                 << (bi::Type << bi::Any))))
                   << (bi::Arg
                       << (bi::Name ^ "paths")
                       << (bi::Description ^ "JSON string paths")
                       << (bi::Type
                           << (bi::TypeSeq
                               << (bi::Type
                                   << (bi::DynamicArray
                                       << (bi::TypeSeq
                                           << (bi::Type << String)
                                           << (bi::Type
                                               << (bi::DynamicArray
                                                   << (bi::Type << bi::Any))))))
                               << (bi::Type
                                   << (bi::Set
                                       << (bi::TypeSeq
                                           << (bi::Type << String)
                                           << (bi::Type
                                               << (bi::DynamicArray
                                                   << (bi::Type
                                                       << bi::Any))))))))))
    << (bi::Result
        << (bi::Name ^ "filtered")
        << (bi::Description ^
            "remaining data from `object` with only keys specified in `paths`")
        << (bi::Type << bi::Any));

  Node match_schema_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "document")
                       << (bi::Description ^ "document to verify by schema")
                       << (bi::Type
                           << (bi::TypeSeq << (bi::Type << bi::String)
                                           << (bi::Type
                                               << (bi::DynamicObject
                                                   << (bi::Type << bi::Any)
                                                   << (bi::Type << bi::Any))))))
                   << (bi::Arg
                       << (bi::Name ^ "schema")
                       << (bi::Description ^ "schema to verify document by")
                       << (bi::Type
                           << (bi::TypeSeq << (bi::Type << bi::String)
                                           << (bi::DynamicObject
                                               << (bi::Type << bi::Any)
                                               << (bi::Type << bi::Any))))))
    << (bi::Result
        << (bi::Name ^ "output")
        << (bi::Description ^
            "`output` is of the form `[match, errors]`. If the document is "
            "valid given the schema, then `match` is `true`, and `errors` is "
            "an empty array. Otherwise, `match` is `false` and `errors` is an "
            "array of objects describing the error(s).")
        << (bi::Type
            << (bi::StaticArray
                << (bi::Type << bi::Boolean)
                << (bi::Type
                    << (bi::DynamicArray
                        << (bi::Type
                            << (bi::StaticObject
                                << (bi::ObjectItem << (bi::Name ^ "desc")
                                                   << (bi::Type << bi::String))
                                << (bi::ObjectItem << (bi::Name ^ "error")
                                                   << (bi::Type << bi::String))
                                << (bi::ObjectItem << (bi::Name ^ "field")
                                                   << (bi::Type << bi::String))
                                << (bi::ObjectItem
                                    << (bi::Name ^ "type")
                                    << (bi::Type << bi::String)))))))));

  Node patch_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "object")
                             << (bi::Description ^ "the object to patch")
                             << (bi::Type << bi::Any))
                 << (bi::Arg
                     << (bi::Name ^ "patches")
                     << (bi::Description ^ "the JSON patches to apply")
                     << (bi::Type
                         << (bi::DynamicArray
                             << (bi::Type
                                 << (bi::HybridObject
                                     << (bi::DynamicObject
                                         << (bi::Type << bi::Any)
                                         << (bi::Type << bi::Any))
                                     << (bi::StaticObject
                                         << (bi::ObjectItem
                                             << (bi::Name ^ "op")
                                             << (bi::Type << bi::String))
                                         << (bi::ObjectItem
                                             << (bi::Name ^ "path")
                                             << (bi::Type << bi::String)))))))))
             << (bi::Result << (bi::Name ^ "output")
                            << (bi::Description ^
                                "result obtained after consecutively applying "
                                "all patch operations in `patches`")
                            << (bi::Type << bi::Any));

  Node remove_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "object")
                       << (bi::Description ^ "object to remove paths from")
                       << (bi::Type
                           << (bi::DynamicObject << (bi::Type << bi::Any)
                                                 << (bi::Type << bi::Any))))
                   << (bi::Arg
                       << (bi::Name ^ "paths")
                       << (bi::Description ^ "JSON string paths")
                       << (bi::Type
                           << (bi::TypeSeq
                               << (bi::Type
                                   << (bi::DynamicArray
                                       << (bi::TypeSeq
                                           << (bi::Type << String)
                                           << (bi::Type
                                               << (bi::DynamicArray
                                                   << (bi::Type << bi::Any))))))
                               << (bi::Type
                                   << (bi::Set
                                       << (bi::TypeSeq
                                           << (bi::Type << String)
                                           << (bi::Type
                                               << (bi::DynamicArray
                                                   << (bi::Type
                                                       << bi::Any))))))))))
    << (bi::Result << (bi::Name ^ "filtered")
                   << (bi::Description ^
                       "result of removing all keys specified in `paths`")
                   << (bi::Type << bi::Any));

  Node verify_schema_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "schema")
                    << (bi::Description ^ "the schema to verify")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::String)
                                        << (bi::Type
                                            << (bi::DynamicObject
                                                << (bi::Type << bi::Any)
                                                << (bi::Type << bi::Any)))))))
    << (bi::Result
        << (bi::Name ^ "output")
        << (bi::Description ^
            "`output` is of the form `[valid, error]`. If the schema is valid, "
            "then `valid` is `true`, and `error` is `null`. Otherwise, `valid` "
            "is false and `error` is a string describing the error.")
        << (bi::Type
            << (bi::StaticArray
                << (bi::Type << bi::Boolean)
                << (bi::Type
                    << (bi::TypeSeq << (bi::Type << bi::Null)
                                    << (bi::Type << bi::String))))));

}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> json()
    {
      return {
        BuiltInDef::placeholder(
          Location("json.filter"),
          filter_decl,
          "JSON builtins not available on this platform"),
        BuiltInDef::placeholder(
          Location("json.match_schema"),
          match_schema_decl,
          "JSON builtins not available on this platform"),
        BuiltInDef::placeholder(
          Location("json.patch"),
          patch_decl,
          "JSON builtins not available on this platform"),
        BuiltInDef::placeholder(
          Location("json.remove"),
          remove_decl,
          "JSON builtins not available on this platform"),
        BuiltInDef::placeholder(
          Location("json.verify_schema"),
          verify_schema_decl,
          "JSON builtins not available on this platform")};
    }
  }
}