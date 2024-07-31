#include <vector>
#include <unordered_map>
#include <cassert>
#include <iostream>
#include <set>
#include <random>
#include <algorithm>
#include <tuple>
#include <functional>
#include <regex>
#include <queue>
#include <fstream>

// DEBUG_TRACE Macro. Anything passed in as an arg is sent to a trace file
// if FUNCTIONTRACE is on. If FUNCTIONTRACE is off it throws the code away.
// This wraps up the five lines (ifdef, open output, close, endif) needed
// to add a debug trace into a single line.
// Example usage:
// DEBUG_TRACE( " Var is:" << myVar << endl );
//
// N.B. Anything with a unquoted comma will cause it to barf.
//
// #define FUNCTIONTRACE
#ifdef FUNCTIONTRACE
  #define DEBUG_TRACE( stuff ) \
  { \
    cout << stuff; \
  }
#else
  #define DEBUG_TRACE( stuff ) /* do nothing */
#endif
