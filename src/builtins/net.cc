#include "builtins.hh"
#include "rego.hh"

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  Node cidr_contains_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg << (bi::Name ^ "cidr")
                               << (bi::Description ^ "CIDR to check against")
                               << (bi::Type << bi::String))
                   << (bi::Arg << (bi::Name ^ "cidr_or_ip")
                               << (bi::Description ^ "CIDR or IP to check")
                               << (bi::Type << bi::String)))
    << (bi::Result << (bi::Name ^ "result")
                   << (bi::Description ^
                       "`true` if `cidr_or_ip` is contained within `cidr`")
                   << (bi::Type << bi::Boolean));

  Node cidr_contains_matches_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg
            << (bi::Name ^ "cidrs")
            << (bi::Description ^ "CIDRs to check against")
            << (bi::Type
                << (bi::TypeSeq
                    << (bi::Type << bi::String)
                    << (bi::Type
                        << (bi::DynamicArray
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any)))))))
                    << (bi::Type
                        << (bi::Set
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any)))))))
                    << (bi::Type
                        << (bi::DynamicObject
                            << (bi::Type << bi::String)
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any))))))))))
        << (bi::Arg
            << (bi::Name ^ "cidrs_or_ips")
            << (bi::Description ^ "CIDRs or IPs to check")
            << (bi::Type
                << (bi::TypeSeq
                    << (bi::Type << bi::String)
                    << (bi::Type
                        << (bi::DynamicArray
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any)))))))
                    << (bi::Type
                        << (bi::Set
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any)))))))
                    << (bi::Type
                        << (bi::DynamicObject
                            << (bi::Type << bi::String)
                            << (bi::Type
                                << (bi::TypeSeq
                                    << (bi::Type << bi::String)
                                    << (bi::Type
                                        << (bi::DynamicArray
                                            << (bi::Type << bi::Any)))))))))))
    << (bi::Result << (bi::Name ^ "result")
                   << (bi::Description ^
                       "tuples identifying matches where `cidrs_or_ips` are "
                       "contained within `cidrs`")
                   << (bi::Type
                       << (bi::Set
                           << (bi::Type
                               << (bi::StaticArray
                                   << (bi::Type << bi::Any)
                                   << (bi::Type << bi::Any))))));

  Node cidr_expand_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "cidr")
                             << (bi::Description ^ "CIDR to expand")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "hosts")
                 << (bi::Description ^
                     "set of IP addresses the CIDR `cidr` expands to")
                 << (bi::Type << (bi::Set << (bi::Type << bi::String))));

  Node cidr_intersects_decl = bi::Decl
    << (bi::ArgSeq << (bi::Arg << (bi::Name ^ "cidr1")
                               << (bi::Description ^ "first CIDR")
                               << (bi::Type << bi::String))
                   << (bi::Arg << (bi::Name ^ "cidr2")
                               << (bi::Description ^ "second CIDR")
                               << (bi::Type << bi::String)))
    << (bi::Result << (bi::Name ^ "result")
                   << (bi::Description ^
                       "`true` if `cidr1` intersects with `cidr2`")
                   << (bi::Type << bi::Boolean));

  Node cidr_is_valid_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "cidr")
                             << (bi::Description ^ "CIDR to validate")
                             << (bi::Type << bi::String)))
             << (bi::Result
                 << (bi::Name ^ "result")
                 << (bi::Description ^ "`true` if `cidr` is a valid CIDR")
                 << (bi::Type << bi::Boolean));

  Node cidr_merge_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg
                     << (bi::Name ^ "addrs")
                     << (bi::Description ^ "CIDRs or IP addresses")
                     << (bi::Type
                         << (bi::TypeSeq
                             << (bi::Type
                                 << (bi::DynamicArray
                                     << (bi::Type << bi::String)))
                             << (bi::Type
                                 << (bi::Set << (bi::Type << bi::String)))))))
             << (bi::Result
                 << (bi::Name ^ "output")
                 << (bi::Description ^
                     "smallest possible set of CIDRs obtained after merging "
                     "the provided list of IP addresses and subnets in `addrs`")
                 << (bi::Type << (bi::Set << (bi::Type << bi::String))));

  Node lookup_ip_addr_decl =
    bi::Decl << (bi::ArgSeq
                 << (bi::Arg << (bi::Name ^ "name")
                             << (bi::Description ^ "domain name to resolve")
                             << (bi::Type << bi::String)))
             << (bi::Return
                 << (bi::Name ^ "addrs")
                 << (bi::Description ^
                     "IP addresses (v4 and v6) that `name` resolves to")
                 << (bi::Type << (bi::Set << (bi::Type << bi::String))));
}

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> net()
    {
      const char* Message = "Networking builtins are not supported";
      return {
        BuiltInDef::placeholder(
          {"net.cidr_contains"}, cidr_contains_decl, Message),
        BuiltInDef::placeholder(
          {"net.cidr_contains_matches"}, cidr_contains_matches_decl, Message),
        BuiltInDef::placeholder({"net.cidr_expand"}, cidr_expand_decl, Message),
        BuiltInDef::placeholder(
          {"net.cidr_intersects"}, cidr_intersects_decl, Message),
        BuiltInDef::placeholder(
          {"net.cidr_is_valid"}, cidr_is_valid_decl, Message),
        BuiltInDef::placeholder({"net.cidr_merge"}, cidr_merge_decl, Message),
        BuiltInDef::placeholder(
          {"net.lookup_ip_addr"}, lookup_ip_addr_decl, Message)};
    }
  }
}