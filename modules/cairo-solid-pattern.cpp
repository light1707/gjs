/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2010 litl, LLC.
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

#include <config.h>

#include <cairo.h>

#include <js/CallArgs.h>
#include <js/Class.h>
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_NewObjectWithGivenProto
#include <jspubtd.h>  // for JSProtoKey

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/macros.h"
#include "modules/cairo-private.h"

JSObject* CairoSolidPattern::new_proto(JSContext* cx, JSProtoKey) {
    JS::RootedObject parent_proto(cx, CairoPattern::prototype(cx));
    return JS_NewObjectWithGivenProto(cx, nullptr, parent_proto);
}

const js::ClassSpec CairoSolidPattern::class_spec = {
    nullptr,  // createConstructor
    &CairoSolidPattern::new_proto,
    CairoSolidPattern::static_funcs,
    nullptr,  // constructorProperties
    nullptr,  // prototypeFunctions
    nullptr,  // prototypeProperties
    &CairoPattern::define_gtype_prop,
};

const JSClass CairoSolidPattern::klass = {
    "SolidPattern", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoPattern::class_ops};

GJS_JSAPI_RETURN_CONVENTION
static bool
createRGB_func(JSContext *context,
               unsigned   argc,
               JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
    double red, green, blue;
    cairo_pattern_t *pattern;

    if (!gjs_parse_call_args(context, "createRGB", argv, "fff",
                             "red", &red,
                             "green", &green,
                             "blue", &blue))
        return false;

    pattern = cairo_pattern_create_rgb(red, green, blue);
    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    JSObject* pattern_wrapper = CairoSolidPattern::from_c_ptr(context, pattern);
    if (!pattern_wrapper)
        return false;
    cairo_pattern_destroy(pattern);

    argv.rval().setObjectOrNull(pattern_wrapper);

    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
createRGBA_func(JSContext *context,
                unsigned   argc,
                JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
    double red, green, blue, alpha;
    cairo_pattern_t *pattern;

    if (!gjs_parse_call_args(context, "createRGBA", argv, "ffff",
                             "red", &red,
                             "green", &green,
                             "blue", &blue,
                             "alpha", &alpha))
        return false;

    pattern = cairo_pattern_create_rgba(red, green, blue, alpha);
    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    JSObject* pattern_wrapper = CairoSolidPattern::from_c_ptr(context, pattern);
    if (!pattern_wrapper)
        return false;
    cairo_pattern_destroy(pattern);

    argv.rval().setObjectOrNull(pattern_wrapper);

    return true;
}

const JSFunctionSpec CairoSolidPattern::static_funcs[] = {
    JS_FN("createRGB", createRGB_func, 0, 0),
    JS_FN("createRGBA", createRGBA_func, 0, 0),
    JS_FS_END};
