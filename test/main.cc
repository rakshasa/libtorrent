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

int main(int argc, char* argv[])
{
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  progress_listener progress;

  controller.addListener( &result );        
  controller.addListener( &progress );

  // Adds the test to the list of test to run
  CppUnit::TextUi::TestRunner runner;

  runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());

  // Change the default outputter to a compiler error format outputter
  // runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(),
  //                                                      std::cerr ) );

  try {
    std::cout << "Running ";
    runner.run( controller );
 
    std::cerr << std::endl;
 
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
