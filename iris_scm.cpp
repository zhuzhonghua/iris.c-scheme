#include "iris_scm.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace Scheme;

namespace iris_scm {

/* Registry key holding Session* (as lightuserdata); each VM has its own */
static constexpr const char *kSessionKey = "~iris-scm-session";

/*
 * Step images. iris.c only fires the callback while it is registered, so
 * registering / unregistering it is the switch and no extra state is needed.
 * The image is freed by iris_sample.c right after the callback returns, so it
 * must be saved here synchronously and never be retained.
 */
static void step_image_trampoline(int step, int total, const iris_image *img)
{
  std::string path = "step-" + std::to_string(step) + "-of-" +
                     std::to_string(total) + ".png";
  iris_image_save(img, path.c_str());
}

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

/*
 * Optional args are positional: [width height steps seed]. A missing arg means
 * every later one is missing too, so one cursor over the packed rest list is
 * enough. Returns NULL when nothing was given, so iris.c applies its defaults.
 */
static const iris_params *parse_params(VM *vm, ValueT *rest, iris_params *p)
{
  *p = IRIS_PARAMS_DEFAULT;
  if (!scm_is_pair(vm, rest))
    return nullptr;

  ValueT *argv = rest;
  auto take_int = [&](auto &field) {
    if (!scm_is_pair(vm, argv))
      return;
    using T = std::remove_cvref_t<decltype(field)>;
    field = static_cast<T>(scm_get_int(vm, scm_car(vm, argv)));
    argv = scm_cdr(vm, argv);
  };
  take_int(p->width);
  take_int(p->height);
  take_int(p->num_steps);
  take_int(p->seed);
  return p;
}

static ValueT iris_scm_stub_generate(VM *vm, ValueT *prompt, ValueT *rest)
{
  int n = 0;
  const char *text = scm_get_string(vm, prompt, &n);

  iris_params p;
  const iris_params *p_ptr = parse_params(vm, rest, &p);

  iris_image *img = iris_generate(session_of(vm)->ctx(), text, p_ptr);
  if (!img)
    scm_error(vm, "iris-generate: %s", iris_get_error());

  session_of(vm)->adopt(img);
  return scm_make_bool(vm, true);
}

/* Source image drives the output; it is local to this call and freed at once */
static ValueT iris_scm_stub_img2img(VM *vm, ValueT *path, ValueT *prompt,
                                    ValueT *rest)
{
  int n = 0;
  const char *file = scm_get_string(vm, path, &n);
  const char *text = scm_get_string(vm, prompt, &n);

  iris_params p;
  const iris_params *p_ptr = parse_params(vm, rest, &p);

  iris_image *src = iris_image_load(file);
  if (!src)
    scm_error(vm, "iris-img2img: cannot load %s", file);

  iris_image *img = iris_img2img(session_of(vm)->ctx(), text, src, p_ptr);
  iris_image_free(src);
  if (!img)
    scm_error(vm, "iris-img2img: %s", iris_get_error());

  session_of(vm)->adopt(img);
  return scm_make_bool(vm, true);
}

/*
 * Leading string args are reference images, the remaining tail is
 * [width height steps seed]. Loaded refs are borrowed by iris.c and freed here.
 */
static ValueT iris_scm_stub_multiref(VM *vm, ValueT *prompt, ValueT *rest)
{
  int n = 0;
  const char *text = scm_get_string(vm, prompt, &n);

  std::vector<iris_image *> refs;
  ValueT *argv = rest;
  while (scm_is_pair(vm, argv) && scm_is_string(vm, scm_car(vm, argv))) {
    const char *file = scm_get_string(vm, scm_car(vm, argv), &n);
    iris_image *ref = iris_image_load(file);
    if (!ref) {
      for (iris_image *r : refs)
        iris_image_free(r);
      scm_error(vm, "iris-multiref: cannot load %s", file);
    }
    refs.push_back(ref);
    argv = scm_cdr(vm, argv);
  }

  iris_params p;
  const iris_params *p_ptr = parse_params(vm, argv, &p);

  std::vector<const iris_image *> cref(refs.begin(), refs.end());
  iris_image *img = iris_multiref(session_of(vm)->ctx(), text, cref.data(),
                                  static_cast<int>(cref.size()), p_ptr);
  for (iris_image *r : refs)
    iris_image_free(r);
  if (!img)
    scm_error(vm, "iris-multiref: %s", iris_get_error());

  session_of(vm)->adopt(img);
  return scm_make_bool(vm, true);
}

static ValueT iris_scm_stub_encode_text(VM *vm, ValueT *prompt)
{
  int n = 0;
  const char *text = scm_get_string(vm, prompt, &n);
  int seq = 0;
  const float *emb = iris_encode_text(session_of(vm)->ctx(), text, &seq);
  if (!emb)
    scm_error(vm, "iris-encode-text: %s", iris_get_error());
  return scm_make_int(vm, seq);
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

static ValueT iris_scm_stub_step_save(VM *vm, ValueT *on)
{
  iris_set_step_image_callback(session_of(vm)->ctx(),
    scm_is_true(vm, on) ? step_image_trampoline : nullptr);
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
  if (ctx_) {
    iris_set_step_image_callback(ctx_, nullptr);  /* clear iris.c's globals */
    iris_free(ctx_);
  }
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
    RegCProc("iris-img2img",    iris_scm_stub_img2img, true),   /* 2 fixed args + rest */
    RegCProc("iris-multiref",   iris_scm_stub_multiref, true),  /* 1 fixed arg + rest */
    RegCProc("iris-encode-text", iris_scm_stub_encode_text),
    RegCProc("iris-image-info", iris_scm_stub_image_info),
    RegCProc("iris-save",       iris_scm_stub_save),
    RegCProc("iris-step-save",  iris_scm_stub_step_save),
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
