#pragma once
#include <string>
// Returns -1 when content is ready, otherwise the process exit code.
int StarPrepareContent(int argc,char** argv,std::string& discRoot,std::string& nandRoot);
