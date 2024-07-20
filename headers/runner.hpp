#ifndef _RUNNER_H_
#define _RUNNER_H_

#include <string>
using namespace std;

void run(int only_sid = -1, int maxdepth = -1);
void runSingleFile(const string& aFileName, int maxdepth = -1);

#endif
