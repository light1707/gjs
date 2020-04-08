/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2012  Red Hat, Inc.
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

#include <utility>  // for (implicit) move

#include <glib-object.h>
#include <glib.h>

#include <js/AllocPolicy.h>  // for SystemAllocPolicy
#include <js/CallArgs.h>
#include <js/Class.h>
#include <js/GCHashTable.h>         // for WeakCache
#include <js/PropertyDescriptor.h>  // for JSPROP_PERMANENT
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_GetPropertyById, JS_AtomizeString
#include <mozilla/HashTable.h>

#include "gi/gtype.h"
#include "gjs/atoms.h"
#include "gjs/context-private.h"
#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util.h"

// clang-format off
const JSClass Type::klass = {
    "GIRepositoryGType",
    JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE,
    &Type::class_ops
};
// clang-format on

const js::ClassSpec Type::class_spec = {nullptr,  // createConstructor
                                        nullptr,  // createPrototype
                                        nullptr,  // constructorFunctions
                                        nullptr,  // constructorProperties
                                        Type::proto_funcs,
                                        Type::proto_props,
                                        nullptr,  // finishInit
                                        js::ClassSpec::DontDefineConstructor};

GJS_JSAPI_RETURN_CONVENTION
bool Type::to_string(JSContext* cx, unsigned argc, JS::Value* vp) {
    GJS_GET_THIS(cx, argc, vp, rec, obj);
    GType gtype = Type::value(cx, obj, rec);
    if (gtype == 0)
        return false;

    GjsAutoChar strval = g_strdup_printf("[object GType for '%s']",
                                         g_type_name(gtype));
    return gjs_string_from_utf8(cx, strval, rec.rval());
}

GJS_JSAPI_RETURN_CONVENTION
bool Type::get_name(JSContext* context, unsigned argc, JS::Value* vp) {
    GJS_GET_THIS(context, argc, vp, rec, obj);
    GType gtype = Type::value(context, obj, rec);
    if (gtype == 0)
        return false;

    return gjs_string_from_utf8(context, g_type_name(gtype), rec.rval());
}

/* Properties */
const JSPropertySpec Type::proto_props[] = {
    JS_PSG("name", &Type::get_name, JSPROP_PERMANENT),
    JS_PS_END,
};

/* Functions */
const JSFunctionSpec Type::proto_funcs[] = {
    JS_FN("toString", &Type::to_string, 0, 0), JS_FS_END};

JSObject* Type::create(JSContext* context, GType gtype) {
    g_assert(((void) "Attempted to create wrapper object for invalid GType",
              gtype != 0));

    GjsContextPrivate* gjs = GjsContextPrivate::from_cx(context);
    // We cannot use gtype_table().lookupForAdd() here, because in between the
    // lookup and the add, GCs may take place and mutate the hash table. A GC
    // may only remove an element, not add one, so it's still safe to do this
    // without locking.
    auto p = gjs->gtype_table().lookup(gtype);
    if (p.found())
        return p->value();

    JS::RootedObject proto(context, Type::create_prototype(context));
    if (!proto)
        return nullptr;

    JS::RootedObject gtype_wrapper(
        context, JS_NewObjectWithGivenProto(context, &Type::klass, proto));
    if (!gtype_wrapper)
        return nullptr;

    JS_SetPrivate(gtype_wrapper, GSIZE_TO_POINTER(gtype));

    gjs->gtype_table().put(gtype, gtype_wrapper);

    return gtype_wrapper;
}

bool Type::_get_actual_gtype(JSContext* context, const GjsAtoms& atoms,
                             JS::HandleObject object, GType* gtype_out,
                             int recurse) {
    GType gtype = Type::value(context, object);
    if (gtype > 0) {
        *gtype_out = gtype;
        return true;
    }

    JS::RootedValue gtype_val(context);

    /* OK, we don't have a GType wrapper object -- grab the "$gtype"
     * property on that and hope it's a GType wrapper object */
    if (!JS_GetPropertyById(context, object, atoms.gtype(), &gtype_val))
        return false;
    if (!gtype_val.isObject()) {
        /* OK, so we're not a class. But maybe we're an instance. Check
           for "constructor" and recurse on that. */
        if (!JS_GetPropertyById(context, object, atoms.constructor(),
                                &gtype_val))
            return false;
    }

    if (recurse > 0 && gtype_val.isObject()) {
        JS::RootedObject gtype_obj(context, &gtype_val.toObject());
        return _get_actual_gtype(context, atoms, gtype_obj, gtype_out,
                                 recurse - 1);
    }

    *gtype_out = G_TYPE_INVALID;
    return true;
}

bool Type::get_actual_gtype(JSContext* context, JS::HandleObject object,
                            GType* gtype_out) {
    g_assert(gtype_out && "Missing return location");

    /* 2 means: recurse at most three times (including this
       call).
       The levels are calculated considering that, in the
       worst case we need to go from instance to class, from
       class to GType object and from GType object to
       GType value.
     */

    const GjsAtoms& atoms = GjsContextPrivate::atoms(context);
    return _get_actual_gtype(context, atoms, object, gtype_out, 2);
}
