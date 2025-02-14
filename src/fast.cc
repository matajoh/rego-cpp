#include "fast.hh"

#include "rego.hh"

namespace
{
  using namespace rego;

  PassDef prep()
  {
    return {
      "prep",
      wf_fast_prep,
      dir::bottomup | dir::once,
      {
        In(FastRego) *
            (T(Module)
             << ((T(Package) << T(Ref)[Ref]) * T(Version)[Version] *
                 T(Policy)[Policy])) >>
          [](Match& _) {
            ACTION();
            return _(Module);
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

        In(Query) * (T(Literal) << (T(Expr)/T(NotExpr))[Expr]) >>
          [](Match& _) {
            ACTION();
            return _(Expr);
          },
      }};
  }
}

namespace rego
{
  Rewriter fast()
  {
    return {
      "fast_rego",
      {
        prep(),
      },
      rego::wf_fast_input};
  }
}