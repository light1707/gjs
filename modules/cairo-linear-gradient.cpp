/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2010 litl, LLC.

#include <config.h>

#include <cairo.h>
#include <glib.h>

#include <js/Class.h>
#include <js/PropertyDescriptor.h>  // for JSPROP_READONLY
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_NewObjectWithGivenProto

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "modules/cairo-private.h"

[[nodiscard]] static JSObject* gjs_cairo_linear_gradient_get_proto(JSContext*);

GJS_DEFINE_PROTO_WITH_PARENT("LinearGradient", cairo_linear_gradient,
                             cairo_gradient, JSCLASS_BACKGROUND_FINALIZE)

GJS_NATIVE_CONSTRUCTOR_DECLARE(cairo_linear_gradient)
{
    GJS_NATIVE_CONSTRUCTOR_VARIABLES(cairo_linear_gradient)
    double x0, y0, x1, y1;
    cairo_pattern_t *pattern;

    GJS_NATIVE_CONSTRUCTOR_PRELUDE(cairo_linear_gradient);

    if (!gjs_parse_call_args(context, "LinearGradient", argv, "ffff",
                             "x0", &x0,
                             "y0", &y0,
                             "x1", &x1,
                             "y1", &y1))
        return false;

    pattern = cairo_pattern_create_linear(x0, y0, x1, y1);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    gjs_cairo_pattern_construct(object, pattern);
    cairo_pattern_destroy(pattern);

    GJS_NATIVE_CONSTRUCTOR_FINISH(cairo_linear_gradient);

    return true;
}

static void
gjs_cairo_linear_gradient_finalize(JSFreeOp *fop,
                                   JSObject *obj)
{
    gjs_cairo_pattern_finalize_pattern(fop, obj);
}

JSPropertySpec gjs_cairo_linear_gradient_proto_props[] = {
    JS_STRING_SYM_PS(toStringTag, "LinearGradient", JSPROP_READONLY),
    JS_PS_END};

JSFunctionSpec gjs_cairo_linear_gradient_proto_funcs[] = {
    // getLinearPoints
    JS_FS_END
};

JSFunctionSpec gjs_cairo_linear_gradient_static_funcs[] = { JS_FS_END };

JSObject *
gjs_cairo_linear_gradient_from_pattern(JSContext       *context,
                                       cairo_pattern_t *pattern)
{
    g_return_val_if_fail(context, nullptr);
    g_return_val_if_fail(pattern, nullptr);
    g_return_val_if_fail(
        cairo_pattern_get_type(pattern) == CAIRO_PATTERN_TYPE_LINEAR, nullptr);

    JS::RootedObject proto(context,
                           gjs_cairo_linear_gradient_get_proto(context));
    JS::RootedObject object(context,
        JS_NewObjectWithGivenProto(context, &gjs_cairo_linear_gradient_class,
                                   proto));
    if (!object) {
        gjs_throw(context, "failed to create linear gradient pattern");
        return nullptr;
    }

    gjs_cairo_pattern_construct(object, pattern);

    return object;
}

