/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef GI_FUNCTION_H_
#define GI_FUNCTION_H_

#include <config.h>

#include <stdint.h>

#include <ffi.h>
#include <girepository.h>
#include <girffi.h>
#include <glib-object.h>

#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>

#include "gjs/jsapi-class.h"
#include "gjs/macros.h"

namespace JS {
class CallArgs;
}
namespace js {
struct ClassSpec;
}
struct JSClass;
struct JSClassOps;

typedef enum {
    PARAM_NORMAL,
    PARAM_SKIPPED,
    PARAM_ARRAY,
    PARAM_CALLBACK,
    PARAM_UNKNOWN,
} GjsParamType;

struct GjsCallbackTrampoline {
    int ref_count;
    GICallableInfo *info;

    GClosure *js_function;

    ffi_cif cif;
    ffi_closure *closure;
    GIScopeType scope;
    bool is_vfunc;
    GjsParamType *param_types;
};

GJS_JSAPI_RETURN_CONVENTION
GjsCallbackTrampoline* gjs_callback_trampoline_new(
    JSContext* cx, JS::HandleFunction function, GICallableInfo* callable_info,
    GIScopeType scope, bool has_scope_object, bool is_vfunc);

void gjs_callback_trampoline_unref(GjsCallbackTrampoline *trampoline);
void gjs_callback_trampoline_ref(GjsCallbackTrampoline *trampoline);

class Function;
using FunctionBase =
    NativeObject<Function, Function, GJS_GLOBAL_SLOT_PROTOTYPE_function>;

class Function : public FunctionBase {
    friend FunctionBase;

    GICallableInfo* m_info;

    GjsParamType* m_param_types;

    uint8_t m_expected_js_argc;
    uint8_t m_js_out_argc;
    GIFunctionInvoker m_invoker;

    explicit Function(GICallableInfo* info);
    ~Function();

    GJS_JSAPI_RETURN_CONVENTION
    bool init(JSContext* cx, GType gtype = G_TYPE_NONE);

    GJS_JSAPI_RETURN_CONVENTION
    static Function* for_js(JSContext* cx, JS::HandleObject obj,
                            JS::CallArgs& args);

    GJS_JSAPI_RETURN_CONVENTION
    static bool call(JSContext* cx, unsigned argc, JS::Value* vp);

    static void finalize_impl(JSFreeOp*, Function* priv);

    GJS_JSAPI_RETURN_CONVENTION
    static bool get_length(JSContext* cx, unsigned argc, JS::Value* vp);

    GJS_USE int32_t get_length_impl();

    GJS_JSAPI_RETURN_CONVENTION
    static bool to_string(JSContext* cx, unsigned argc, JS::Value* vp);

    GJS_JSAPI_RETURN_CONVENTION
    bool to_string_impl(JSContext* cx, const JS::CallArgs& args);

    static const JSClass klass;
    static const JSClassOps class_ops;
    static const JSPropertySpec proto_props[];
    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;

 public:
    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* create(JSContext* cx, GType gtype, GICallableInfo* info);

    GJS_JSAPI_RETURN_CONVENTION
    bool fill_method_instance(JSContext* cx, JS::HandleObject obj,
                              GIArgument* out_arg, bool& is_gobject);

    GJS_USE char* format_name();

    GJS_JSAPI_RETURN_CONVENTION
    bool invoke(JSContext* cx, const JS::CallArgs& args,
                JS::HandleObject this_obj = nullptr,
                GIArgument* r_value = nullptr);

    GJS_JSAPI_RETURN_CONVENTION
    static bool invoke_constructor_uncached(JSContext* cx, GIFunctionInfo* info,
                                            JS::HandleObject obj,
                                            const JS::CallArgs& args,
                                            GIArgument* rvalue);
};

GJS_JSAPI_RETURN_CONVENTION
JSObject *gjs_define_function(JSContext       *context,
                              JS::HandleObject in_object,
                              GType            gtype,
                              GICallableInfo  *info);

#endif  // GI_FUNCTION_H_
