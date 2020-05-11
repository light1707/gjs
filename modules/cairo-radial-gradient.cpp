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

#include <js/Class.h>
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_NewObjectWithGivenProto
#include <jspubtd.h>  // for JSProtoKey

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "modules/cairo-private.h"

namespace JS {
class CallArgs;
}

JSObject* CairoRadialGradient::new_proto(JSContext* cx, JSProtoKey) {
    JS::RootedObject parent_proto(cx, CairoGradient::prototype(cx));
    return JS_NewObjectWithGivenProto(cx, nullptr, parent_proto);
}

const js::ClassSpec CairoRadialGradient::class_spec = {
    nullptr,  // createConstructor
    &CairoRadialGradient::new_proto,
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    nullptr,  // prototypeFunctions
    nullptr,  // prototypeProperties
    &CairoPattern::define_gtype_prop,
};

const JSClass CairoRadialGradient::klass = {
    "RadialGradient", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoPattern::class_ops};

cairo_pattern_t* CairoRadialGradient::constructor_impl(
    JSContext* context, const JS::CallArgs& argv) {
    double cx0, cy0, radius0, cx1, cy1, radius1;
    cairo_pattern_t *pattern;
    if (!gjs_parse_call_args(context, "RadialGradient", argv, "ffffff",
                             "cx0", &cx0,
                             "cy0", &cy0,
                             "radius0", &radius0,
                             "cx1", &cx1,
                             "cy1", &cy1,
                             "radius1", &radius1))
        return nullptr;

    pattern = cairo_pattern_create_radial(cx0, cy0, radius0, cx1, cy1, radius1);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return nullptr;

    return pattern;
}

const JSFunctionSpec CairoRadialGradient::proto_funcs[] = {
    // getRadialCircles
    JS_FS_END};
