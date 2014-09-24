#include <stdexcept>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

int main(int argc, char* argv[])
{
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  CppUnit::BriefTestProgressListener progressListener;

  controller.addListener( &result );        
  controller.addListener( &progressListener );

  // Get the top level suite from the registry
  CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();

  // Adds the test to the list of test to run
  CppUnit::TextUi::TestRunner runner;
  runner.addTest( suite );

  // Change the default outputter to a compiler error format outputter
  runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(),
                                                       std::cerr ) );
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
