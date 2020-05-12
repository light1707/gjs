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
#include <glib.h>

#include <js/CallArgs.h>
#include <js/Class.h>
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_NewObjectWithGivenProto
#include <jspubtd.h>  // for JSProtoKey

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/macros.h"
#include "modules/cairo-private.h"

JSObject* CairoSurfacePattern::new_proto(JSContext* cx, JSProtoKey) {
    JS::RootedObject parent_proto(cx, CairoPattern::prototype(cx));
    return JS_NewObjectWithGivenProto(cx, nullptr, parent_proto);
}

const js::ClassSpec CairoSurfacePattern::class_spec = {
    nullptr,  // createConstructor
    &CairoSurfacePattern::new_proto,
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    CairoSurfacePattern::proto_funcs,
    nullptr,  // prototypeProperties
    &CairoPattern::define_gtype_prop,
};

const JSClass CairoSurfacePattern::klass = {
    "SurfacePattern", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoPattern::class_ops};

cairo_pattern_t* CairoSurfacePattern::constructor_impl(
    JSContext* context, const JS::CallArgs& argv) {
    cairo_pattern_t *pattern;
    JS::RootedObject surface_wrapper(context);
    if (!gjs_parse_call_args(context, "SurfacePattern", argv, "o",
                             "surface", &surface_wrapper))
        return nullptr;

    cairo_surface_t* surface = CairoSurface::for_js(context, surface_wrapper);
    if (!surface)
        return nullptr;

    pattern = cairo_pattern_create_for_surface(surface);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return nullptr;

    return pattern;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
setExtend_func(JSContext *context,
               unsigned   argc,
               JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, argv, obj);
    cairo_extend_t extend;

    if (!gjs_parse_call_args(context, "setExtend", argv, "i",
                             "extend", &extend))
        return false;

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    cairo_pattern_set_extend(pattern, extend);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
getExtend_func(JSContext *context,
               unsigned   argc,
               JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, rec, obj);
    cairo_extend_t extend;

    if (argc > 0) {
        gjs_throw(context, "SurfacePattern.getExtend() requires no arguments");
        return false;
    }

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    extend = cairo_pattern_get_extend(pattern);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    rec.rval().setInt32(extend);

    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
setFilter_func(JSContext *context,
               unsigned   argc,
               JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, argv, obj);
    cairo_filter_t filter;

    if (!gjs_parse_call_args(context, "setFilter", argv, "i",
                             "filter", &filter))
        return false;

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    cairo_pattern_set_filter(pattern, filter);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
getFilter_func(JSContext *context,
               unsigned   argc,
               JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, rec, obj);
    cairo_filter_t filter;

    if (argc > 0) {
        gjs_throw(context, "SurfacePattern.getFilter() requires no arguments");
        return false;
    }

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    filter = cairo_pattern_get_filter(pattern);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    rec.rval().setInt32(filter);

    return true;
}

const JSFunctionSpec CairoSurfacePattern::proto_funcs[] = {
    JS_FN("setExtend", setExtend_func, 0, 0),
    JS_FN("getExtend", getExtend_func, 0, 0),
    JS_FN("setFilter", setFilter_func, 0, 0),
    JS_FN("getFilter", getFilter_func, 0, 0),
    JS_FS_END};
