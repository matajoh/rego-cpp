#include "ir.hh"

#include "trieste/json.h"

namespace
{
  using namespace trieste;
  using namespace rego::ir;

  PassDef from_ir_json()
  {
    PassDef pass = {
      "from_ir_json",
      json::wf,
      dir::bottomup | dir::once,
      {
        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "strings") * T(json::Array)[StringSeq])) >>
          [](Match& _) {
            Node stringseq = NodeDef::create(StringSeq);
            for (size_t idx = 0; idx < _(StringSeq)->size(); ++idx)
            {
              Node child = _(StringSeq)->at(idx);
              if (child->type() != json::Object || child->size() != 1)
              {
                return rego::err(child, "Invalid static string");
              }

              Node value = child->front();
              Node key = value / json::Key;
              Node string = value / json::Value;
              if (key->location().view() != "value")
              {
                return rego::err(key, "Invalid static string key");
              }

              stringseq
                << (String << (Idx ^ std::to_string(idx)) << (Value ^ string));
            }

            return stringseq;
          },

        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "files") * T(json::Array)[PathSeq])) >>
          [](Match& _) {
            Node pathseq = NodeDef::create(PathSeq);
            for (auto& child : *_(PathSeq))
            {
              if (child->type() != json::Object || child->size() != 1)
              {
                return rego::err(child, "Invalid file path");
              }

              Node value = child->front();
              Node key = value / json::Key;
              Node string = value / json::Value;
              if (key->location().view() != "value")
              {
                return rego::err(key, "Invalid file path key");
              }

              pathseq << (Path ^ string);
            }

            return pathseq;
          },

        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "static") * T(json::Object)[Static])) >>
          [](Match& _) { return Static << *_[Static]; },

        In(Top) * T(json::Object)[Policy] >>
          [](Match& _) { return Policy << *_[Policy]; },
      }};

    return pass;
  }
}

namespace rego
{
  Rewriter from_ir_json()
  {
    return {"from_ir_json", {::from_ir_json()}, wf_ir};
  }
}