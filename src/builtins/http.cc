#include "builtins.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node send_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg
                     << (bi::Name ^ "request")
                     << (bi::Description ^ "the HTTP request object")
                     << (bi::Type
                         << (bi::DynamicObject << (bi::Type << bi::String)
                                               << (bi::Type << bi::Any)))))
             << (bi::Result
                 << (bi::Name ^ "response")
                 << (bi::Description ^ "the HTTP response object")
                 << (bi::Type
                     << (bi::DynamicObject << (bi::Type << bi::Any)
                                           << (bi::Type << bi::Any))));
}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> http()
    {
      return {
        BuiltInDef::placeholder(
          {"http.send"}, send_decl, "HTTP builtins are not supported"),
      };
    }
  }
}