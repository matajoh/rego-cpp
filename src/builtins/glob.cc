#include "builtins.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node match_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "pattern")
                    << (bi::Description ^ "glob pattern")
                    << (bi::Type << bi::String))
        << (bi::Arg << (bi::Name ^ "delimiters")
                    << (bi::Description ^
                        "glob pattern delimiters, e.g. `[\".\", \":\"]`, "
                        "defaults to `[\".x\"]` if unset. If `delimiters` is "
                        "`null`, glob match without a delimiter.")
                    << (bi::Type
                        << (bi::TypeSeq << (bi::Type << bi::Null)
                                        << (bi::Type
                                            << (bi::DynamicArray
                                                << (bi::Type << bi::String))))))
        << (bi::Arg << (bi::Name ^ "match")
                    << (bi::Description ^ "string to match against `pattern`")))
    << (bi::Result << (bi::Name ^ "result")
                   << (bi::Description ^
                       "true if `match` can be found in `pattern` which is "
                       "separated by `delimiters`")
                   << (bi::Type << bi::Boolean));

  Node quote_meta_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "pattern")
                             << (bi::Description ^ "glob patter")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "output")
                 << (bi::Description ^ "the escaped string of `pattern`")
                 << (bi::Type << bi::String));
}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> glob()
    {
      const char* Message = "glob not supported";
      return {
        BuiltInDef::placeholder({"glob.match"}, match_decl, Message),
        BuiltInDef::placeholder({"glob.quote_meta"}, quote_meta_decl, Message),
      };
    }
  }
}