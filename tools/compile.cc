#include <CLI/CLI.hpp>
#include <chrono>
#include <rego/rego.hh>
#include <sstream>
#include <trieste/json.h>
#include <trieste/logging.h>

using namespace trieste;

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
  std::cout << "regoc " << REGOCPP_VERSION << " (" << REGOCPP_BUILD_NAME << ", "
            << REGOCPP_BUILD_DATE << ")"
            << "[" << REGOCPP_BUILD_TOOLCHAIN << "] on " << REGOCPP_PLATFORM
            << std::endl;
  CLI::App app;

  app.set_help_all_flag("--help-all", "Expand all help");

  std::filesystem::path bundle_path;
  app.add_option("bundle,-b,--bundle", bundle_path, "Bundle Directory")
    ->required();

  std::filesystem::path output_path;
  app.add_option("output,-o,--output", output_path, "Output file for compiled bundle")
    ->required();

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
    "-1,--v0-compatible",
    v0_compatible,
    "opt-in to OPA features and behaviors prior to the OPA v1.0 release");

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

  auto data_json_reader = json::reader()
                            .wf_check_enabled(wf_checks)
                            .debug_enabled(!output.empty())
                            .debug_path(output / "data_json");
  auto ir_json_reader = json::reader()
                          .wf_check_enabled(wf_checks)
                          .debug_enabled(!output.empty())
                          .debug_path(output / "ir_json");
  auto json_to_ir = rego::from_ir_json()
                      .wf_check_enabled(wf_checks)
                      .debug_enabled(!output.empty())
                      .debug_path(output / "ir");
  auto data = data_json_reader.file(bundle_path / "data.json").read();
  auto ir = ir_json_reader.file(bundle_path / "plan.json") >> json_to_ir;
}
