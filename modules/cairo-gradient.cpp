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
#include <jsapi.h>    // for JS_NewObjectWithGivenProto
#include <jspubtd.h>  // for JSProtoKey

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/macros.h"
#include "modules/cairo-private.h"

const js::ClassSpec CairoGradient::class_spec = {
    nullptr,  // createConstructor
    &CairoGradient::new_proto,
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    CairoGradient::proto_funcs,
    nullptr,  // prototypeProperties
    &CairoPattern::define_gtype_prop,
};

const JSClass CairoGradient::klass = {
    "Gradient", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoPattern::class_ops};

JSObject* CairoGradient::new_proto(JSContext* cx, JSProtoKey) {
    JS::RootedObject parent_proto(cx, CairoPattern::prototype(cx));
    return JS_NewObjectWithGivenProto(cx, nullptr, parent_proto);
}

/* Methods */

GJS_JSAPI_RETURN_CONVENTION
static bool
addColorStopRGB_func(JSContext *context,
                     unsigned   argc,
                     JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, argv, obj);
    double offset, red, green, blue;

    if (!gjs_parse_call_args(context, "addColorStopRGB", argv, "ffff",
                             "offset", &offset,
                             "red", &red,
                             "green", &green,
                             "blue", &blue))
        return false;

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    cairo_pattern_add_color_stop_rgb(pattern, offset, red, green, blue);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
addColorStopRGBA_func(JSContext *context,
                      unsigned   argc,
                      JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, argv, obj);
    double offset, red, green, blue, alpha;

    if (!gjs_parse_call_args(context, "addColorStopRGBA", argv, "fffff",
                             "offset", &offset,
                             "red", &red,
                             "green", &green,
                             "blue", &blue,
                             "alpha", &alpha))
        return false;

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    cairo_pattern_add_color_stop_rgba(pattern, offset, red, green, blue, alpha);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    argv.rval().setUndefined();
    return true;
}

const JSFunctionSpec CairoGradient::proto_funcs[] = {
    JS_FN("addColorStopRGB", addColorStopRGB_func, 0, 0),
    JS_FN("addColorStopRGBA", addColorStopRGBA_func, 0, 0),
    // getColorStopRGB
    // getColorStopRGBA
    JS_FS_END};
