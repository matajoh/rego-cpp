#include "ir.hh"

#include "trieste/json.h"

namespace
{
  using namespace trieste;
  using namespace rego::ir;

  Node err(Node r, const std::string& msg)
  {
    return Error << (ErrorMsg ^ msg) << (ErrorAst << r);
  }

  Node object_lookdown(
    Node object,
    const std::string& key,
    const Token& token,
    const std::function<Node(const Node&)>& transform = [](const Node& n) {
      return n;
    })
  {
    if (object->type() != json::Object)
    {
      std::ostringstream buf;
      buf << "Expected " << token << ", found " << object->type();
      return err(object, buf.str());
    }

    auto nodes = object->lookdown({key});
    if (nodes.empty())
    {
      // OPA bundles sometimes capitalise keys
      std::string key_capital = key;
      key_capital[0] = std::toupper(key_capital[0]);
      nodes = object->lookdown({key_capital});
      if (nodes.empty())
      {
        return err(object, "Object lookdown failed for key: " + key);
      }
    }

    Node result = nodes.front()->back();
    if (result != token)
    {
      std::ostringstream buf;
      buf << "Expected " << token << ", found " << result->type();
      return err(result, buf.str());
    }

    return transform(result);
  }

  PassDef from_ir_json()
  {
    PassDef pass = {
      "from_ir_json",
      wf_ir,
      dir::bottomup | dir::once,
      {
        In(json::Member, json::Array) *
            (T(json::Object)
             << ((T(json::Member)
                  << (T(json::Key, "type") * T(json::String, "\"local\""))) *
                 (T(json::Member)
                  << (T(json::Key, "value") * T(json::Number)[Local])))) >>
          [](Match& _) { return Operand << (Local ^ _(Local)); },

        In(json::Member, json::Array) *
            (T(json::Object)
             << ((T(json::Member)
                  << (T(json::Key, "type") * T(json::String, "\"bool\""))) *
                 (T(json::Member)
                  << (T(json::Key, "value") *
                      T(json::True, json::False)[Boolean])))) >>
          [](Match& _) { return Operand << (Boolean ^ _(Boolean)); },

        In(json::Member, json::Array) *
            (T(json::Object)
             << ((T(json::Member)
                  << (T(json::Key, "type") *
                      T(json::String, "\"string_index\""))) *
                 (T(json::Member)
                  << (T(json::Key, "value") *
                      T(json::Number)[StringIndex])))) >>
          [](Match& _) { return Operand << (StringIndex ^ _(StringIndex)); },

        In(json::Array) *
            (T(json::Object)[CallStmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ArrayAppendStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(CallStmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node array =
              object_lookdown(stmt, "array", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            Node value = object_lookdown(stmt, "value", Operand);
            return ArrayAppendStmt << array << value;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"AssignVarOnceStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node source = object_lookdown(stmt, "source", Operand);
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return AssignVarOnceStmt << source << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"AssignVarStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node source = object_lookdown(stmt, "source", Operand);
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return AssignVarStmt << source << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"BlockStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node blockseq = object_lookdown(stmt, "blocks", BlockSeq);

            return BlockStmt << blockseq;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"BreakStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node index =
              object_lookdown(stmt, "index", json::Number, [](const Node& n) {
                return UInt32 ^ n;
              });
            return BreakStmt << index;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"CallStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node func =
              object_lookdown(stmt, "func", json::String, [](const Node& n) {
                return String ^ n;
              });
            Node args =
              object_lookdown(stmt, "args", json::Array, [](const Node& n) {
                return OperandSeq << *n;
              });
            Node result =
              object_lookdown(stmt, "result", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return CallStmt << func << args << result;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") * T(json::String, "\"DotStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node source = object_lookdown(stmt, "source", Operand);
            Node key = object_lookdown(stmt, "key", Operand);
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return DotStmt << source << key << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"IsDefinedStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node source =
              object_lookdown(stmt, "source", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return IsDefinedStmt << source;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"MakeArrayStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node capacity = object_lookdown(
              stmt, "capacity", json::Number, [](const Node& n) {
                return Int32 ^ n;
              });
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return MakeArrayStmt << capacity << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"MakeNullStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return MakeNullStmt << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"MakeNumberRefStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node index =
              object_lookdown(stmt, "index", json::Number, [](const Node& n) {
                return Int32 ^ n;
              });
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return MakeNumberRefStmt << index << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"MakeObjectStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return MakeObjectStmt << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ObjectInsertStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node key = object_lookdown(stmt, "key", Operand);
            Node value = object_lookdown(stmt, "value", Operand);
            Node object =
              object_lookdown(stmt, "object", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return ObjectInsertStmt << key << value << object;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ObjectMergeStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node a = object_lookdown(
              stmt, "a", json::Number, [](const Node& n) { return Local ^ n; });
            Node b = object_lookdown(
              stmt, "b", json::Number, [](const Node& n) { return Local ^ n; });
            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return ObjectMergeStmt << a << b << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ResetLocalStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node target =
              object_lookdown(stmt, "target", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return ResetLocalStmt << target;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ResultSetAddStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node value =
              object_lookdown(stmt, "value", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return ResultSetAddStmt << value;
          },

        In(json::Array) *
            (T(json::Object)[Stmt]
             << (T(json::Member)
                 << (T(json::Key, "type") *
                     T(json::String, "\"ReturnLocalStmt\"")))) >>
          [](Match& _) {
            Node stmt = object_lookdown(_(Stmt), "stmt", json::Object);
            if (stmt == Error)
            {
              return stmt;
            }

            Node source =
              object_lookdown(stmt, "source", json::Number, [](const Node& n) {
                return Local ^ n;
              });
            return ReturnLocalStmt << source;
          },

        In(json::Array) *
            (T(json::Object)
             << (T(json::Member)
                 << (T(json::Key, "stmts") * T(json::Array)[Block]))) >>
          [](Match& _) { return Block << *_[Block]; },

        In(json::Member) *
            (T(json::Key, "blocks")[json::Key] * T(json::Array)[BlockSeq]) >>
          [](Match& _) {
            return Seq << _(json::Key) << (BlockSeq << *_[BlockSeq]);
          },

        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "strings") * T(json::Array)[StringSeq])) >>
          [](Match& _) {
            Node stringseq = NodeDef::create(StringSeq);
            for (auto& child : *_(StringSeq))
            {
              stringseq << object_lookdown(
                child, "value", json::String, [](const Node& n) {
                  return String ^ n;
                });
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
              pathseq << object_lookdown(
                child, "value", json::String, [](const Node& n) {
                  return String ^ n;
                });
            }

            return pathseq;
          },

        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "plans") * T(json::Object)[PlanSeq])) >>
          [](Match& _) {
            Node plans_array =
              object_lookdown(_(PlanSeq), "plans", json::Array);
            if (plans_array == Error)
            {
              return plans_array;
            }

            Node planseq = NodeDef::create(PlanSeq);
            for (auto& child : *plans_array)
            {
              Node name =
                object_lookdown(child, "name", json::String, [](const Node& n) {
                  return String ^ n;
                });
              Node blockseq = object_lookdown(child, "blocks", BlockSeq);
              planseq << (Plan << name << blockseq);
            }

            return planseq;
          },

        In(json::Object) *
            (T(json::Member)
             << (T(json::Key, "funcs") * T(json::Object)[FunctionSeq])) >>
          [](Match& _) {
            Node funcs_array =
              object_lookdown(_(FunctionSeq), "funcs", json::Array);
            if (funcs_array == Error)
            {
              return funcs_array;
            }

            Node funcseq = NodeDef::create(FunctionSeq);
            for (auto& child : *funcs_array)
            {
              Node name =
                object_lookdown(child, "name", json::String, [](const Node& n) {
                  return String ^ n;
                });
              Node path =
                object_lookdown(child, "path", json::Array, [](const Node& n) {
                  Node stringseq = NodeDef::create(StringSeq);
                  for (const auto& str : *n)
                  {
                    stringseq << (String ^ str);
                  }
                  return stringseq;
                });
              Node params = object_lookdown(
                child, "params", json::Array, [](const Node& n) {
                  Node localseq = NodeDef::create(LocalSeq);
                  for (const auto& param : *n)
                  {
                    localseq << (Local ^ param);
                  }
                  return localseq;
                });
              Node local = object_lookdown(
                child, "return", json::Number, [](const Node& n) {
                  return Local ^ n;
                });
              Node blockseq = object_lookdown(child, "blocks", BlockSeq);
              funcseq
                << (Function << name << path << params << local << blockseq);
            }

            return funcseq;
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
    return {"from_ir_json", {::from_ir_json()}, json::wf};
  }
}