#include "iris_scm.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <type_traits>

using namespace Scheme;

namespace iris_scm {

/* Registry key holding Session* (as lightuserdata); each VM has its own */
static constexpr const char *kSessionKey = "~iris-scm-session";

/* ===== File-level host functions (RegCProc takes bare function pointers) ===== */

static Session *session_of(VM *vm)
{
  ValueT v = scm_registry_get(vm, kSessionKey);
  return static_cast<Session *>(scm_to_lightuserdata(vm, &v));
}

static ValueT iris_scm_stub_model_info(VM *vm)
{
  const char *s = iris_model_info(session_of(vm)->ctx());
  return scm_make_string(vm, s ? s : "?");
}

static ValueT iris_scm_stub_generate(VM *vm, ValueT *prompt, ValueT *rest)
{
  iris_params p = IRIS_PARAMS_DEFAULT;
  int n = 0;
  const char *text = scm_get_string(vm, prompt, &n);

  /*
   * Optional args are positional: [width height steps seed]. A missing arg
   * means every later one is missing too, so one cursor over the packed
   * rest list is enough.
   */
  ValueT *argv = rest;
  auto take_int = [&](auto &field) {
    if (!scm_is_pair(vm, argv))
      return;
    using T = std::remove_cvref_t<decltype(field)>;
    field = static_cast<T>(scm_get_int(vm, scm_car(vm, argv)));
    argv = scm_cdr(vm, argv);
  };
  take_int(p.width);
  take_int(p.height);
  take_int(p.num_steps);
  take_int(p.seed);

  iris_image *img = iris_generate(session_of(vm)->ctx(), text, &p);
  if (!img)
    scm_error(vm, "iris-generate: %s", iris_get_error());

  session_of(vm)->adopt(img);
  return scm_make_bool(vm, true);
}

static ValueT iris_scm_stub_image_info(VM *vm)
{
  iris_image *img = session_of(vm)->current();
  if (!img)
    scm_error(vm, "iris-image-info: no image yet, call iris-generate first");

  ValueT vec = scm_make_vector(vm, 3);
  scm_vector_set(vm, &vec, 0, scm_make_int(vm, img->width));
  scm_vector_set(vm, &vec, 1, scm_make_int(vm, img->height));
  scm_vector_set(vm, &vec, 2, scm_make_int(vm, img->channels));
  return vec;
}

static ValueT iris_scm_stub_save(VM *vm, ValueT *path)
{
  iris_image *img = session_of(vm)->current();
  int n = 0;
  const char *s = scm_get_string(vm, path, &n);

  if (!img)
    scm_error(vm, "iris-save: no image yet, call iris-generate first");
  if (iris_image_save(img, s) != 0)
    scm_error(vm, "iris-save: %s", iris_get_error());

  return scm_make_bool(vm, true);
}

/* ===== Session ===== */

Session::Session(const std::string &model_dir)
{
  ctx_ = iris_load_dir(model_dir.c_str());
  if (!ctx_) {
    const char *s = iris_get_error();
    err_ = s ? s : "failed to load model";
    return;
  }
  iris_set_mmap(ctx_, 1);
  init();
}

Session::~Session()
{
  if (current_)
    iris_image_free(current_);
  if (vm_)
    scm_destroy_vm(vm_);
  if (ctx_)
    iris_free(ctx_);
}

void Session::init()
{
  vm_ = scm_create_vm();
  if (!vm_) {
    err_ = "Scheme VM initialization failed";
    return;
  }

  static const RegCProc lib[] = {
    RegCProc("iris-model-info", iris_scm_stub_model_info),
    RegCProc("iris-generate",   iris_scm_stub_generate, true),  /* 1 fixed arg + rest */
    RegCProc("iris-image-info", iris_scm_stub_image_info),
    RegCProc("iris-save",       iris_scm_stub_save),
    RegCProc(nullptr, -1)
  };

  scm_register_lib(vm_, lib);
  scm_registry_set(vm_, kSessionKey, scm_make_lightuserdata(vm_, this));
}

void Session::adopt(iris_image *img)
{
  if (current_)
    iris_image_free(current_);
  current_ = img;
}

std::string Session::model_info() const
{
  const char *s = ctx_ ? iris_model_info(ctx_) : nullptr;
  return s ? s : "?";
}

std::string Session::run_file(const std::string &path)
{
  if (!vm_)
    return err_.empty() ? "session is not ready" : err_;

  std::ifstream f(path, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  std::string src = ss.str();
  if (src.empty())
    return "cannot read " + path;

  try {
    scm_eval_string(vm_, src.data(), (int)src.size());
    return {};
  } catch (const char *msg) {
    return msg ? msg : "scheme error";
  } catch (...) {
    return "unknown scheme error";
  }
}

}  // namespace iris_scm
