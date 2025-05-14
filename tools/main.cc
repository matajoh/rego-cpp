#include <CLI/CLI.hpp>
#include <chrono>
#include <rego/rego.hh>
#include <sstream>
#include <trieste/logging.h>

class Timer
{
private:
  std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;
  std::string m_name;
  bool m_print;

public:
  Timer(std::string name, bool print) : m_name(name)
  {
    m_start_time = std::chrono::high_resolution_clock::now();
    m_print = print;
  }

  ~Timer()
  {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - m_start_time);
    if (m_print)
    {
      trieste::logging::Output() << m_name << " took " << duration.count()
                                 << " microseconds" << std::endl;
    }
  }
};

int main(int argc, char** argv)
{
  std::cout << "rego " << REGOCPP_VERSION << " (" << REGOCPP_BUILD_NAME << ", "
            << REGOCPP_BUILD_DATE << ")"
            << "[" << REGOCPP_BUILD_TOOLCHAIN << "] on " << REGOCPP_PLATFORM
            << std::endl;
  CLI::App app;

  app.set_help_all_flag("--help-all", "Expand all help");

  std::string query_expr;
  app.add_option("query,-q,--query", query_expr, "Query")->required();

  std::vector<std::filesystem::path> data_paths;
  app.add_option("-d,--data", data_paths, "Data/Policy files");

  std::filesystem::path input_path;
  app.add_option("-i,--input", input_path, "Input JSON file");

  bool wf_checks{false};
  app.add_flag("-w,--wf", wf_checks, "Enable well-formedness checks");

  std::filesystem::path output;
  app.add_option("-a,--ast", output, "Folder to use for AST output");

  std::string log_level;
  app
    .add_option(
      "-l,--log_level",
      log_level,
      "Set Log Level to one of "
      "Trace, Debug (includes log of unification),"
      "Info, Warning, Output, Error, "
      "None")
    ->check(rego::set_log_level_from_string);

  bool v0_compatible{false};
  app.add_flag(
    "-1,--v0-compatible", v0_compatible, "opt-in to OPA features and behaviors prior to the OPA v1.0 release");

  bool fast{false};
  app.add_flag("-f,--fast", fast, "Use fast query mode");

  bool timing{false};
  app.add_flag("-t,--timing", timing, "Print timing information");

#ifndef NDEBUG
  if (timing)
  {
    trieste::logging::Warn()
      << "Timing requested on a debug build!" << std::endl;
  }
#endif

  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return app.exit(e);
  }

  std::shared_ptr<rego::Interpreter> interpreter;
  {
    Timer timer("Interpreter creation", timing);
    interpreter = std::make_shared<rego::Interpreter>(!v0_compatible || fast);
  }

  interpreter->wf_check_enabled(wf_checks);
  if (!output.empty())
  {
    interpreter->debug_enabled(true);
    interpreter->debug_path(output);
  }

  if (!input_path.empty())
  {
    if (!std::filesystem::exists(input_path))
    {
      trieste::logging::Error() << std::filesystem::weakly_canonical(input_path)
                                << " does not exist" << std::endl;
      return 1;
    }

    std::string json_contents;
    {
      std::ifstream input_file(input_path);
      std::ostringstream os;
      os << input_file.rdbuf();
      json_contents = os.str();
    }

    rego::Node result;
    try
    {
      Timer timer("Set input JSON", timing);
      result = interpreter->set_input_json(json_contents);
    }
    catch (const std::exception& e)
    {
      trieste::logging::Error() << e.what() << std::endl;
      return 1;
    }

    if (result != nullptr)
    {
      trieste::logging::Error()
        << "Invalid input file: "
        << std::filesystem::weakly_canonical(input_path) << std::endl;
      return 1;
    }
  }

  for (auto& path : data_paths)
  {
    if (!std::filesystem::exists(path))
    {
      trieste::logging::Error() << std::filesystem::weakly_canonical(path)
                                << " does not exist" << std::endl;
      return 1;
    }

    try
    {
      rego::Node result;
      if (path.extension() == ".json")
      {
        Timer timer("Add data JSON file: " + path.string(), timing);
        result = interpreter->add_data_json_file(path);
      }
      else
      {
        Timer timer("Add data module file: " + path.string(), timing);
        result = interpreter->add_module_file(path);
      }

      if (result != nullptr)
      {
        trieste::logging::Error()
          << "Invalid data file:" << std::filesystem::weakly_canonical(path)
          << std::endl;
        return 1;
      }
    }
    catch (const std::exception& e)
    {
      trieste::logging::Error() << e.what() << std::endl;
      return 1;
    }
  }

  try
  {
    if (fast)
    {
      {
        Timer timer("Set query", timing);
        interpreter->set_query(query_expr);
      }
      rego::Node result;
      {
        Timer timer("Fast query", timing);
        result = interpreter->fast_query();
      }
      trieste::logging::Output()
        << interpreter->output_to_string(result) << std::endl;
    }
    else
    {
      Timer timer("Query", timing);
      trieste::logging::Output() << interpreter->query(query_expr) << std::endl;
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    trieste::logging::Error() << e.what() << std::endl;
    return 1;
  }
}
