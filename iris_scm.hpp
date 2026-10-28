#pragma once

#include <string>

#include "vm.h"
#include "iris.h"

namespace iris_scm {

/*
 * Session -- one VM, one model, one "current image" 
 */
class Session {
public:
  explicit Session(const std::string &model_dir);
  ~Session();

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  bool ok() const { return ctx_ != nullptr; }
  std::string error() const { return err_; }
  std::string model_info() const;

  /* Empty string on success, error description on failure */
  std::string run_file(const std::string &path);

  /* Used by the file-level host functions */
  iris_ctx *ctx() const { return ctx_; }
  iris_image *current() const { return current_; }
  void adopt(iris_image *img);

private:
  void init();   /* create the VM and register host functions */

  iris_ctx *ctx_ = nullptr;
  Scheme::VM *vm_ = nullptr;
  iris_image *current_ = nullptr;   /* the image the script sees */
  std::string err_;
};

}  // namespace iris_scm
