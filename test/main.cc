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

int main(int argc, char* argv[])
{
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  progress_listener progress;

  controller.addListener( &result );        
  controller.addListener( &progress );

  // Adds the test to the list of test to run
  CppUnit::TextUi::TestRunner runner;

  auto test_name = std::getenv("TEST_NAME");

  if (test_name != NULL && std::string(test_name) != "")
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry(test_name).makeTest());
  else
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());

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
