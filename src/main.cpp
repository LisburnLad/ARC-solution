#include "precompiled_stl.hpp"
#include "runner.hpp"
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string>
#include <iostream>
using namespace std;

int main(int argc, char**argv) {
  //rankFeatures();
  //evalNormalizeRigid();
  //evalTasks();
  //bruteSubmission();
  //bruteSolve();
  int only_sid = -1;
  string filename = "";
  if (argc >= 2) {

    filename = argv[1];
    bool has_only_digits = (filename.find_first_not_of( "0123456789" ) == string::npos);
    if( has_only_digits )
    {
      only_sid = atoi(filename.c_str());
      DEBUG_TRACE("Running only task # " << only_sid << endl);
    }
    else
    {
      DEBUG_TRACE( "Runing for single file: " << filename << endl);
    }
  }
  int maxdepth = -1;
  if (argc >= 3) {
    maxdepth = atoi(argv[2]);
    DEBUG_TRACE("Using max depth " << maxdepth << endl);
  }

  if( only_sid == -1 && filename.length() > 0)
  {
    runSingleFile(filename, maxdepth);
  }
  else
  {
    run(only_sid, maxdepth);
  }
}
