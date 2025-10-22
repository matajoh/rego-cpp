#pragma once

#include "internal.hh"

namespace rego
{
  namespace builtins
  {
    std::vector<BuiltIn> aggregates();
    std::vector<BuiltIn> aws();
    std::vector<BuiltIn> arrays();
    std::vector<BuiltIn> bits();
    std::vector<BuiltIn> comparison();
    std::vector<BuiltIn> conversions();
    std::vector<BuiltIn> crypto();
    std::vector<BuiltIn> encoding();
    std::vector<BuiltIn> glob();
    std::vector<BuiltIn> graph();
    std::vector<BuiltIn> graphql();
    std::vector<BuiltIn> http();
    std::vector<BuiltIn> internal();
    std::vector<BuiltIn> json();
    std::vector<BuiltIn> jwt();
    std::vector<BuiltIn> net();
    std::vector<BuiltIn> numbers();
    std::vector<BuiltIn> objects();
    std::vector<BuiltIn> regex();
    std::vector<BuiltIn> rego();
    std::vector<BuiltIn> semver();
    std::vector<BuiltIn> sets();
    std::vector<BuiltIn> strings();
    std::vector<BuiltIn> time();
    std::vector<BuiltIn> types();
    std::vector<BuiltIn> units();
    std::vector<BuiltIn> uuid();
  }
}