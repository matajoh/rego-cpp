#include "builtins.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node decode_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "jwt")
                             << (bi::Description ^ "JWT token to decode")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "output")
                 << (bi::Description ^
                     "`[header, payload, sig]`, where `header` and `payload` "
                     "are objects; `sig` is the hexadecimal representation of "
                     "the signature on the token.")
                 << (bi::Type
                     << (bi::StaticArray
                         << (bi::Type
                             << (bi::DynamicObject << (bi::Type << bi::Any)
                                                   << (bi::Type << bi::Any)))
                         << (bi::Type
                             << (bi::DynamicObject << (bi::Type << bi::Any)
                                                   << (bi::Type << bi::Any)))
                         << (bi::Type << bi::String))));

  Node decode_verify_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "jwt")
                       << (bi::Description ^
                           "JWT token whose signature is to be verified and "
                           "whose claims are to be checked")
                       << (bi::Type << bi::String))
                   << (bi::Arg
                       << (bi::Name ^ "constraints")
                       << (bi::Description ^ "claim verification constraints")
                       << (bi::Type
                           << (bi::DynamicObject << (bi::Type << bi::String)
                                                 << (bi::Type << bi::Any)))))
    << (bi::Result
        << (bi::Name ^ "output")
        << (bi::Description ^
            "`[valid, header, payload]`: if the input token is verified and "
            "meets the requirements of `constraints` then `valid` is `true`; "
            "`header` and `payload` are objects containing the JOSE header and "
            "the JWT claim set; otherwise, `valid` is `false`, `header` and "
            "`payload` are `{}`")
        << (bi::Type
            << (bi::StaticArray
                << (bi::Type << bi::Boolean)
                << (bi::Type
                    << (bi::DynamicObject << (bi::Type << bi::Any)
                                          << (bi::Type << bi::Any)))
                << (bi::Type
                    << (bi::DynamicObject << (bi::Type << bi::Any)
                                          << (bi::Type << bi::Any))))));

  Node verify_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg
                       << (bi::Name ^ "jwt")
                       << (bi::Description ^
                           "JWT token whose signature is to be verified")
                       << (bi::Type << bi::String))
                   << (bi::Arg
                       << (bi::Name ^ "certificate")
                       << (bi::Description ^
                           "PEM encoded certificate, PEM encoded public key, "
                           "or the JWK key (set) used to verify the signature")
                       << (bi::Type << bi::String)))
    << (bi::Result << (bi::Name ^ "result")
                   << (bi::Description ^
                       "`true` if the signature is valid`, `false` otherwise")
                   << (bi::Type << bi::Boolean));

  Node encode_sign_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "headers")
                    << (bi::Description ^ "JWS Protected header")
                    << (bi::Type
                        << (bi::DynamicObject << (bi::Type << bi::String)
                                              << (bi::Type << bi::Any))))
        << (bi::Arg << (bi::Name ^ "payload")
                    << (bi::Description ^ "JWS Payload")
                    << (bi::Type
                        << (bi::DynamicObject << (bi::Type << bi::String)
                                              << (bi::Type << bi::Any))))
        << (bi::Arg << (bi::Name ^ "key")
                    << (bi::Description ^ "JSON Web Key (RFC7517)")
                    << (bi::Type
                        << (bi::DynamicObject << (bi::Type << bi::String)
                                              << (bi::Type << bi::Any)))))
    << (bi::Result << (bi::Name ^ "output") << (bi::Description ^ "signed JWT")
                   << (bi::Type << bi::String));

  Node encode_sign_raw_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg << (bi::Name ^ "headers")
                               << (bi::Description ^ "JWS Protected header")
                               << (bi::Type << bi::String))
                   << (bi::Arg << (bi::Name ^ "payload")
                               << (bi::Description ^ "JWS Payload")
                               << (bi::Type << bi::String))
                   << (bi::Arg << (bi::Name ^ "key")
                               << (bi::Description ^ "JSON Web Key (RFC7517)")
                               << (bi::Type << bi::String)))
    << (bi::Result << (bi::Name ^ "output") << (bi::Description ^ "signed JWT")
                   << (bi::Type << bi::String));

}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> jwt()
    {
      const char* Message = "JSON Web Tokens are not supported";
      return {
        BuiltInDef::placeholder({"io.jwt.decode"}, decode_decl, Message),
        BuiltInDef::placeholder(
          {"io.jwt.decode_verify"}, decode_verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_eddsa"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_es256"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_es384"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_es512"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_hs256"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_hs384"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_hs512"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_ps256"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_ps384"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_ps512"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_rs256"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_rs384"}, verify_decl, Message),
        BuiltInDef::placeholder({"io.jwt.verify_rs512"}, verify_decl, Message),
        BuiltInDef::placeholder(
          {"io.jwt.encode_sign"}, encode_sign_decl, Message),
        BuiltInDef::placeholder(
          {"io.jwt.encode_sign_raw"}, encode_sign_raw_decl, Message),
      };
    }
  }
}