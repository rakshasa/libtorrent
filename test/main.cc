#include <cstdlib>
#include <stdexcept>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "helpers/progress_listener.h"

CPPUNIT_REGISTRY_ADD_TO_DEFAULT("torrent/net");

#include <algorithm>

static void
dump_failure_log(const failure_type& failure) {
  if (failure.log->empty())
    return;

  std::cout << std::endl << failure.name << std::endl;

  // Doesn't print dump messages as log_buffer drops them.
  std::for_each(failure.log->begin(), failure.log->end(), [](const torrent::log_entry& entry) {
      std::cout << entry.timestamp << ' ' << entry.message << '\n';
    });

  std::cout << std::flush;
}

static void
dump_failures(const failure_list_type& failures) {
  if (failures.empty())
    return;

  std::cout << std::endl
            << "=================" << std::endl
            << "Failed Test Logs:" << std::endl
            << "=================" << std::endl;

  std::for_each(failures.begin(), failures.end(), [](const failure_type& failure) {
      dump_failure_log(failure);
    });
  std::cout << std::endl;
}

static
void add_tests(CppUnit::TextUi::TestRunner& runner, const char* c_test_names) {
  if (c_test_names == NULL || std::string(c_test_names).empty()) {
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
    return;
  }

  const std::string& test_names(c_test_names);

  size_t pos = 0;
  size_t next = 0;

  while ((next = test_names.find(',', pos)) < test_names.size()) {
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry(test_names.substr(pos, next - pos)).makeTest());
    pos = next + 1;
  }

  runner.addTest(CppUnit::TestFactoryRegistry::getRegistry(test_names.substr(pos)).makeTest());
}

int main(int argc, char* argv[])
{
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  progress_listener progress;

  controller.addListener( &result );        
  controller.addListener( &progress );

  // Adds the test to the list of test to run
  CppUnit::TextUi::TestRunner runner;
  add_tests(runner, std::getenv("TEST_NAME"));

  // Change the default outputter to a compiler error format outputter
  // runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(),
  //                                                      std::cerr ) );

  try {
    std::cout << "Running ";
    runner.run( controller );
 
    // TODO: Make outputter.
    dump_failures(progress.failures());

    // Print test in a compiler compatible format.
    CppUnit::CompilerOutputter outputter( &result, std::cerr );
    outputter.write();                      

  } catch ( std::invalid_argument &e ) { // Test path not resolved
    std::cerr  <<  std::endl  
               <<  "ERROR: "  <<  e.what()
               << std::endl;
    return 1;
  }

  return result.wasSuccessful() ? 0 : 1;
}
