#include "ir.hh"

#include <cstdint>

namespace
{
  using namespace trieste;

  enum class VMError
  {
    Undefined,
    Return
  };

  Location source(Node stmt)
  {
    return (stmt / rego::ir::Source)->location();
  }

  Location target(Node stmt)
  {
    return (stmt / rego::ir::Target)->location();
  }

  std::int32_t int32(Node value)
  {
    std::string value_str(value->location().view());
    return std::stoi(value_str);
  }

  std::uint32_t uint32(Node value)
  {
    std::string value_str(value->location().view());
    return std::stoul(value_str);
  }

  std::int64_t int64(Node value)
  {
    std::string value_str(value->location().view());
    return std::stoll(value_str);
  }

  Node dot(Node source, Node key)
  {
    if (source == json::Object)
    {
      for (Node& member : *source)
      {
        Node member_key = member / json::Key;
        if (member_key->location() == key->location())
        {
          return member / json::Value;
        }
      }

      throw VMError::Undefined;
    }
    else if (source == json::Array)
    {
      std::uint32_t index = uint32(key);
      if (index < source->size())
      {
        return source->at(index);
      }

      throw std::runtime_error("Index out of bounds");
    }
    else
    {
      throw std::runtime_error("Invalid source type for dot operation");
    }
  }
}

namespace rego::ir
{
  VirtualMachine::VirtualMachine(Node ir)
  {
    WFContext ctx(wf_ir);
    m_data = (ir / Data)->front();
    Node policy = ir / Policy;
    Node strings = policy / Static / StringSeq;
    for (Node str : *strings)
    {
      m_strings.emplace_back(str->location().view());
    }

    Node plans = policy / PlanSeq;
    for (Node plan : *plans)
    {
      Location name = (plan / Name)->location();
      m_plans[name] = plan / BlockSeq;
    }

    Node functions = policy / FunctionSeq;
    for (Node func : *functions)
    {
      Location name = (func / Name)->location();
      m_functions[name] = func;
    }
  }

  Node VirtualMachine::read_local(const Location& key)
  {
    auto it = m_locals.find(key);
    if (it != m_locals.end())
    {
      return it->second;
    }

    return nullptr;
  }

  void VirtualMachine::write_local(
    const Location& key, const Node& value, bool once)
  {
    if (once && m_locals.find(key) != m_locals.end())
    {
      throw std::runtime_error("Local variable already defined");
    }

    m_locals[key] = value;
  }

  bool VirtualMachine::is_defined(const Location& key)
  {
    return m_locals.find(key) != m_locals.end();
  }

  void VirtualMachine::reset_local(const Location& key)
  {
    m_locals.erase(key);
  }

  Node VirtualMachine::unpack_operand(const Node& operand)
  {
    Node value = operand->front();
    if (value == LocalIndex)
    {
      Location key = value->location();
      return read_local(key);
    }
    else if (value == StringIndex)
    {
      std::string index_str(value->location().view());
      size_t index = std::stoul(index_str);
      return json::String ^ m_strings[index];
    }
    else if (operand == Boolean)
    {
      if (value->location().view() == "true")
      {
        return json::True;
      }
      else
      {
        return json::False;
      }
    }
    else
    {
      throw std::runtime_error("Invalid operand type");
    }
  }

  Node VirtualMachine::run(const Location& plan, Node input)
  {
    m_input = input;
    m_locals.clear();
    m_result_set.clear();

    auto it = m_plans.find(plan);
    if (it == m_plans.end())
    {
      throw std::runtime_error("Plan not found");
    }

    for (Node block : *it->second)
    {
      run_block(block);
    }

    return json::Array << m_result_set;
  }

  void VirtualMachine::run_block(Node block)
  {
    for (Node stmt : *block)
    {
      try
      {
        run_stmt(stmt);
      }
      catch (const VMError& code)
      {
        if (code == VMError::Undefined)
        {
          return;
        }
      }
    }
  }

  Node VirtualMachine::call_function(
    const Location& func_name, const Node& args)
  {
    auto it = m_functions.find(func_name);
    if (it == m_functions.end())
    {
      throw std::runtime_error("Function not found");
    }

    Node function = it->second;
    Node parameters = function / ParameterSeq;

    for (size_t i = 0; i < parameters->size(); ++i)
    {
      Node param = parameters->at(i);
      Location key = param->location();
      write_local(key, args->at(i));
    }

    Node blockseq = function / BlockSeq;

    // Run the function's block
    for (Node block : *blockseq)
    {
      run_block(block);
    }

    Node value = read_local((function / Return)->location());
    return value;
  }

  void VirtualMachine::run_stmt(Node stmt)
  {
    Token type = stmt->type();
    if (type == MakeObjectStmt)
    {
      write_local(target(stmt), NodeDef::create(json::Object));
    }
    else if (type == MakeArrayStmt)
    {
      write_local(target(stmt), NodeDef::create(json::Array));
    }
    else if (type == MakeNullStmt)
    {
      write_local(target(stmt), json::Null);
    }
    else if (type == MakeNumberRefStmt)
    {
      std::int32_t index = int32(stmt / Index);
      write_local(target(stmt), json::Number ^ m_strings[index]);
    }
    else if (type == BlockStmt)
    {
      Node blockseq = stmt / Blocks;
      for (Node block : *blockseq)
      {
        run_block(block);
      }
    }
    else if (type == CallStmt)
    {
      Location func_name = (stmt / Func)->location();
      Node value = call_function(func_name, stmt / Args);
      if (value != nullptr)
      {
        write_local(target(stmt), value);
      }
    }
    else if (type == ResetLocalStmt)
    {
      reset_local(target(stmt));
    }
    else if (type == AssignVarOnceStmt)
    {
      Node source = unpack_operand(stmt / Source);
      write_local(target(stmt), source, true);
    }
    else if (type == IsDefinedStmt)
    {
      if (!is_defined(source(stmt)))
      {
        throw VMError::Undefined;
      }
    }
    else if (type == ReturnLocalStmt)
    {
      throw VMError::Return;
    }
    else if (type == ObjectInsertStmt)
    {
      Node key = unpack_operand(stmt / Key);
      Node value = unpack_operand(stmt / Value);
      Node object = read_local((stmt / Object)->location());
      if (object != nullptr)
      {
        object << (json::Member << key << value);
      }
    }
    else if (type == ArrayAppendStmt)
    {
      Node value = unpack_operand(stmt / Value);
      Node array = read_local((stmt / Array)->location());
      if (array != nullptr)
      {
        array << value;
      }
    }
    else if (type == DotStmt)
    {
      Node source = unpack_operand(stmt / Source);
      Node key = unpack_operand(stmt / Key);
      Node value = dot(source, key);
      write_local(target(stmt), value);
    }
    else if (type == AssignVarStmt)
    {
      Node value = unpack_operand(stmt / Source);
      write_local(target(stmt), value);
    }
    else if (type == ResultSetAddStmt)
    {
      Node value = read_local((stmt / Value)->location());
      if (value != nullptr)
      {
        m_result_set.push_back(value);
      }
    }
    else
    {
      throw std::runtime_error("Unsupported statement type");
    }
  }
}