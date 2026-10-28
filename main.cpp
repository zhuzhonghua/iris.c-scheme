#include <cstdio>
#include <string>

#include "iris_scm.hpp"

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s MODEL_DIR SCRIPT.scm\n", argv[0]);
    return 2;
  }

  iris_scm::Session session(argv[1]);
  if (!session.ok()) {
    std::fprintf(stderr, "failed to load model: %s\n", session.error().c_str());
    return 2;
  }

  std::string err = session.run_file(argv[2]);
  if (!err.empty()) {
    std::fprintf(stderr, "scheme error: %s\n", err.c_str());
    return 3;
  }
  return 0;
}
