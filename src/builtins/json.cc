#include "builtins.h"
#include "rego.hh"

#include <iterator>

namespace
{
  using namespace rego;
  namespace bi = rego::builtins;

  namespace pointer
  {
    class Pointer : public std::vector<Location>
    {
    public:
      Pointer(Location path) noexcept : m_path(path)
      {
        size_t pos = 0;
        while (pos < path.len)
        {
          auto maybe_key = next_key(path, pos);
          if (!maybe_key.has_value())
          {
            m_error = err(JSONString ^ path, "Invalid pointer");
            break;
          }

          logging::Trace() << "Pointer[" << size()
                           << "] = " << maybe_key.value().view();

          push_back(maybe_key.value());
        }
      }

      const Location& path() const
      {
        return m_path;
      }

      bool is_valid() const
      {
        return m_error == nullptr;
      }

      Node error() const
      {
        return m_error;
      }

    private:
      static std::optional<Location> next_key(const Location& path, size_t& pos)
      {
        const std::string_view& path_view = path.view();
        if (path_view[pos] != '/')
        {
          return std::nullopt;
        }

        bool needs_replacement = false;
        size_t end = pos + 1;
        while (end < path.len)
        {
          if (path_view[end] == '/')
          {
            break;
          }

          if (path_view[end] == '~')
          {
            needs_replacement = true;
          }

          end += 1;
        }

        pos += 1; // remove the prepending `/`
        Location key(path.source, path.pos + pos, end - pos);
        pos = end;
        if (!needs_replacement)
        {
          return key;
        }

        auto maybe_unescaped = unescape_key(key.view());
        if (!maybe_unescaped.has_value())
        {
          return std::nullopt;
        }

        return Location(maybe_unescaped.value());
      }

      static std::optional<std::string> unescape_key(
        const std::string_view& pointer)
      {
        std::ostringstream os;
        size_t index = 0;
        while (index < pointer.size())
        {
          char c = pointer[index];
          assert(c != '/');
          if (c == '~')
          {
            if (index + 1 == pointer.size())
            {
              logging::Error() << "Invalid `~` in pointer";
              return std::nullopt;
            }

            char i = pointer[index + 1];
            if (i == '0')
            {
              os << '~';
            }
            else if (i == '1')
            {
              os << '/';
            }
            else
            {
              logging::Error() << "Invalid escape value '" << i << "'";
              return std::nullopt;
            }

            index += 2;
            continue;
          }

          os << c;
          index += 1;
        }

        return os.str();
      }

      Node m_error;
      Location m_path;
    };

    enum class Action
    {
      Insert,
      Read,
      Replace,
      Remove,
      Compare
    };

    struct DebugAction
    {
      Action action;
    };

    static std::ostream& operator<<(std::ostream& os, const DebugAction debug)
    {
      switch (debug.action)
      {
        case Action::Insert:
          return os << "insert";

        case Action::Read:
          return os << "read";

        case Action::Replace:
          return os << "replace";

        case Action::Remove:
          return os << "remove";

        case Action::Compare:
          return os << "compare";
      }

      throw std::runtime_error("Unrecognized Action value");
    }

    class Operation
    {
    public:
      Operation(Location path, Action action, Node value) :
        m_pointer(path), m_action(action), m_value(value)
      {}

      Operation(Location path, Action action) : Operation(path, action, nullptr)
      {}

      Node run(Node document) const
      {
        if (!m_pointer.is_valid())
        {
          return m_pointer.error();
        }

        WFContext ctx(wf_result);
        std::map<Location, Node> set_lookup;

        if (m_pointer.empty())
        {
          switch (m_action)
          {
            case Action::Read:
              return document;

            case Action::Replace:
              return m_value;

            case Action::Remove:
              return err(
                JSONString ^ m_pointer.path(), "Cannot remove the root node");

            case Action::Compare:
              return compare(document, m_value);

            case Action::Insert:
              return m_value;
          }
        }

        Node current = document;
        for (size_t i = 0; i < m_pointer.size() - 1; ++i)
        {
          const Location& key = m_pointer[i];

          if (!current->in({Array, Object, Set}))
          {
            return err(current, "Cannot index into value");
          }

          if (current == Object)
          {
            Nodes results = Resolver::object_lookdown(current, key.view());
            if (results.empty())
            {
              return err(
                current, "No child at path: " + std::string(key.view()));
            }

            current = results.front() / Val;
            continue;
          }

          if (current == Set)
          {
            set_lookup.clear();
            current = set_index(current, key, set_lookup);
            if (current == Error)
            {
              return current;
            }

            continue;
          }

          size_t index = 0;
          current = array_index(current, key, index);
          if (current == Error)
          {
            return current;
          }
        }

        if (!current->in({Object, Array, Set}))
        {
          return err(current, "Cannot index into value");
        }

        const Location& key = m_pointer.back();
        if (current == Object)
        {
          return object_action(current, key);
        }

        if (current == Set)
        {
          return set_action(current, key);
        }

        return array_action(current, key);
      }

    private:
      static Node compare(Node actual, Node expected)
      {
        if (actual == Error)
        {
          return actual;
        }

        std::string actual_key = to_key(actual);
        std::string expected_key = to_key(expected);
        if (actual_key != expected_key)
        {
          std::ostringstream os;
          os << actual_key << " != " << expected_key;
          return err(actual, os.str());
        }

        return actual;
      }

      Node object_action(Node object, const Location& key) const
      {
        assert(object == Object);

        logging::Trace() << "Pointer: Object action " << DebugAction{m_action}
                         << " on object " << object << " at key " << key.view();

        Node existing;
        Node member;
        Nodes results = Resolver::object_lookdown(object, key.view());
        if (results.empty())
        {
          existing = nullptr;
        }
        else
        {
          member = results.front();
          existing = member / Val;
        }

        switch (m_action)
        {
          case Action::Compare:
            if (results.empty())
            {
              return err(
                object,
                "Member does not exist with key: " + std::string(key.view()));
            }
            return compare(existing, m_value);

          case Action::Insert:
            if (existing == nullptr)
            {
              object << object_item((JSONString ^ key), m_value->clone());
              return m_value;
            }
            else
            {
              member / Val = m_value->clone();
              return existing;
            }

          case Action::Read:
            if (existing == nullptr)
            {
              return err(
                object,
                "Member does not exist with key: " + std::string(key.view()));
            }
            return existing;

          case Action::Remove:
            if (existing == nullptr)
            {
              return err(
                object,
                "Member does not exist with key: " + std::string(key.view()));
            }
            object->replace(member);
            return existing;

          case Action::Replace:
            if (results.empty())
            {
              return err(
                object,
                "Member does not exist with key: " + std::string(key.view()));
            }
            member / Val = m_value->clone();
            return existing;
        }

        throw std::runtime_error("Unsupported Action value");
      }

      Node array_action(Node array, const Location& key) const
      {
        assert(array == Array);
        size_t index = 0;

        logging::Trace() << "Pointer: Array action " << DebugAction{m_action}
                         << " on array " << array << " at index " << key.view();

        Node element = array_index(array, key, index);

        if (m_action == Action::Insert && index == array->size())
        {
          array << m_value->clone();
          return m_value;
        }

        if (element == Error)
        {
          return element;
        }

        switch (m_action)
        {
          case Action::Compare:
            return compare(element, m_value->clone());

          case Action::Insert:
            array->insert(array->begin() + index, m_value->clone());
            return m_value;

          case Action::Read:
            return element;

          case Action::Remove:
            array->replace(element);
            return element;

          case Action::Replace:
            array->replace_at(index, m_value->clone());
            return element;
        }

        throw std::runtime_error("Unsupported Action value");
      }

      Node set_action(Node set, const Location& key) const
      {
        assert(set == Set);

        std::map<Location, Node> set_lookup;
        Node existing = set_index(set, key, set_lookup);

        if (!set_lookup.contains(key))
        {
          existing = nullptr;
        }

        switch (m_action)
        {
          case Action::Compare:
            if (existing == nullptr)
            {
              return err(
                set,
                "Member does not exist with key: " + std::string(key.view()));
            }
            return compare(existing, m_value);

          case Action::Insert:
            if (existing == nullptr)
            {
              set << m_value;
              return m_value;
            }
            else
            {
              return existing;
            }

          case Action::Read:
            if (existing == nullptr)
            {
              return err(
                set,
                "Member does not exist with key: " + std::string(key.view()));
            }
            return existing;

          case Action::Remove:
            if (existing == nullptr)
            {
              return err(
                set,
                "Member does not exist with key: " + std::string(key.view()));
            }
            set->replace(existing);
            return existing;

          case Action::Replace:
            if (existing == nullptr)
            {
              return err(
                set,
                "Member does not exist with key: " + std::string(key.view()));
            }

            if (set_lookup.contains(to_key(m_value)))
            {
              set->replace(existing);
            }
            else
            {
              set->replace(existing, m_value);
            }

            return existing;
        }

        throw std::runtime_error("Unsupported Action value");
      }

      static Node set_index(
        Node set, Location key, std::map<Location, Node>& lookup)
      {
        Node result;
        for (Node element : *set)
        {
          Location e_key(to_key(element));
          lookup[e_key] = element;
          if (e_key == key)
          {
            result = element;
          }
        }

        if (result == nullptr)
        {
          std::ostringstream error;
          error << "No child with key `" << key.view() << "` exists in set";
          return err(set, error.str());
        }

        return result;
      }

      static Node array_index(Node array, Location key, size_t& index)
      {
        std::string_view index_view = key.view();
        if (index_view == "-")
        {
          index = array->size();
          return err(
            JSONString ^ key,
            "End-of-array selector `-` cannot appear inside a pointer, only "
            "at the end");
        }

        if (index_view.front() == '-')
        {
          return err(
            JSONString ^ key, "unable to parse array index (prepended by `-`)");
        }

        if (index_view.front() == '0' && index_view.size() > 1)
        {
          return err(JSONString ^ key, "Leading zeros");
        }

        index = 0;
        for (auto it = index_view.begin(); it < index_view.end(); ++it)
        {
          char c = *it;
          if (!std::isdigit(c))
          {
            return err(
              JSONString ^ key, "unable to parse array index (not a digit)");
          }

          index =
            index * 10 + static_cast<size_t>(c) - static_cast<size_t>('0');
        }

        if (index >= array->size())
        {
          return err(
            Int ^ key, "index is greater than number of items in array");
        }

        return array->at(index);
      }

      Pointer m_pointer;
      Action m_action;
      Node m_value;
    };
  }

  namespace ptree
  {
    struct PNodeDef;

    typedef std::shared_ptr<PNodeDef> PNode;
    struct PNodeDef
    {
      std::map<Location, PNode> children;
    };

    void insert(PNode root, const std::vector<Location>& path)
    {
      PNode current = root;
      for (const Location& loc : path)
      {
        logging::Trace() << "path: " << loc.view();
        if (!current->children.contains(loc))
        {
          current->children[loc] = std::make_shared<PNodeDef>();
        }

        current = current->children[loc];
      }
    }

    void insert(PNode root, const Node& array)
    {
      assert(array == Array);
      std::vector<Location> path;
      std::transform(
        array->begin(),
        array->end(),
        std::back_inserter(path),
        [](const Node& n) {
          auto maybe_key = unwrap(n, {Int, JSONString});
          if (maybe_key.success)
          {
            return maybe_key.node->location();
          }

          return Location(to_key(n));
        });
      insert(root, path);
    }

    void insert(PNode root, const Location& path)
    {
      std::string_view path_view = path.view();
      std::vector<Location> parts;
      size_t pos = 0;
      size_t end = path_view.find('/');
      while (end != std::string::npos)
      {
        Location part = path;
        part.pos += pos;
        part.len = end - pos;
        parts.push_back(part);
        pos = end + 1;
        end = path_view.find('/', pos);
      }

      Location last = path;
      last.pos += pos;
      last.len = path.len - pos;
      parts.push_back(last);
      insert(root, parts);
    }
  }

  Node filter_decl = bi::Decl
    << (bi::ArgSeq
        << (bi::Arg << (bi::Name ^ "object")
                    << (bi::Description ^ "object to filter")
                    << (bi::Type
                        << (bi::DynamicObject << (bi::Type << bi::Any)
                                              << (bi::Type << bi::Any))))
        << (bi::Arg
            << (bi::Name ^ "paths") << (bi::Description ^ "JSON string paths")
            << (bi::Type
                << (bi::TypeSeq
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
                                            << (bi::Type << bi::Any)))))))))))
    << (bi::Result
        << (bi::Name ^ "filtered")
        << (bi::Description ^
            "remaining data from `object` with only keys specified in `paths`")
        << (bi::Type << bi::Any));

  typedef std::tuple<ptree::PNode, Node, Node> filter_state;

  Node filter(Nodes args)
  {
    Node object =
      unwrap_arg(args, UnwrapOpt(0).type(Object).func("json.filter"));
    if (object == Error)
    {
      return object;
    }

    Node paths =
      unwrap_arg(args, UnwrapOpt(1).types({Array, Set}).func("json.filter"));
    if (paths == Error)
    {
      return paths;
    }

    ptree::PNode root = std::make_shared<ptree::PNodeDef>();
    for (Node path : *paths)
    {
      auto maybe_string = unwrap(path, JSONString);
      if (maybe_string.success)
      {
        insert(root, maybe_string.node->location());
        continue;
      }

      auto maybe_array = unwrap(path, Array);
      if (maybe_array.success)
      {
        insert(root, maybe_array.node);
        continue;
      }

      return err(path, "Invalid JSON path");
    }

    Node result = NodeDef::create(Object);
    std::vector<filter_state> frontier;
    frontier.emplace_back(root, object, result);

    while (!frontier.empty())
    {
      auto [pnode, src, dst] = frontier.back();
      frontier.pop_back();

      if (src == Object)
      {
        for (Node src_item : *src)
        {
          auto maybe_key = unwrap(src_item / Key, JSONString);
          if (!maybe_key.success)
          {
            continue;
          }

          Location key = maybe_key.node->location();
          if (!pnode->children.contains(key))
          {
            continue;
          }

          auto pchild = pnode->children[key];
          if(pchild->children.empty())
          {
            dst << src_item->clone();
            continue;
          }

          auto maybe_indexable = unwrap(src_item / Val, {Object, Array, Set});
          if (!maybe_indexable.success)
          {
            dst << src_item->clone();
            continue;
          }

          Node src_term = maybe_indexable.node;
          Node dst_term = NodeDef::create(src_term->type());
          dst
            << (ObjectItem << (Term << (Scalar << (JSONString ^ key)))
                           << (Term << dst_term));

          frontier.emplace_back(pchild, src_term, dst_term);
        }
        continue;
      }

      if (src == Array)
      {
        for (size_t i = 0; i < src->size(); ++i)
        {
          Location key(std::to_string(i));
          if (!pnode->children.contains(key))
          {
            continue;
          }

          Node src_term = src->at(i);

          auto pchild = pnode->children[key];
          if(pchild->children.empty())
          {
            dst << src_term->clone();
            continue;
          }

          auto maybe_indexable = unwrap(src_term, {Object, Array, Set});
          if (!maybe_indexable.success)
          {
            dst << src_term->clone();
            continue;
          }

          src_term = maybe_indexable.node;
          Node dst_term = NodeDef::create(src_term->type());
          dst << (Term << dst_term);

          frontier.emplace_back(pchild, src_term, dst_term);
        }
      }

      if (src == Set)
      {
        for (Node src_term : *src)
        {
          Location key(to_key(src_term));
          if (!pnode->children.contains(key))
          {
            continue;
          }

          auto pchild = pnode->children[key];
          if(pchild->children.empty())
          {
            dst << src_term->clone();
            continue;
          }

          auto maybe_indexable = unwrap(src_term, {Object, Array, Set});
          if (!maybe_indexable.success)
          {
            dst << src_term->clone();
            continue;
          }

          src_term = maybe_indexable.node;
          Node dst_term = NodeDef::create(src_term->type());
          dst << (Term << dst_term);

          frontier.emplace_back(pchild, src_term, dst_term);
        }
      }
    }

    return result;
  }

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
      // TODO
      // the schema implementation
      // can piggy-back on Trieste json, but the others need to operate on Rego
      // objects as if they were JSON, which is obviously subtly different.
      // Patch will basically need to be a functional duplicate but operating
      // over raw Rego objects instead of JSON (to accommodate the set
      // behaviour, among other things) Remove can be implemented with Patch
      // Filter can also be implemented with Pointer primitives (with reads and
      // adds to a separate document)

      return {
        BuiltInDef::create(Location("json.filter"), filter_decl, filter),
        BuiltInDef::placeholder(
          Location("json.match_schema"),
          match_schema_decl,
          "JSON schemas are not supported"),
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
          "JSON schemas are not supported")};
    }
  }
}