/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2017  Philip Chimento
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

#ifndef GJS_JSAPI_CLASS_H_
#define GJS_JSAPI_CLASS_H_

#include <config.h>

#include <glib-object.h>
#include <glib.h>

#include <js/TypeDecls.h>

#include "gi/wrapperutils.h"
#include "gjs/global.h"
#include "gjs/jsapi-util.h"
#include "gjs/macros.h"
#include "util/log.h"

GJS_JSAPI_RETURN_CONVENTION
bool gjs_init_class_dynamic(
    JSContext* cx, JS::HandleObject in_object, JS::HandleObject parent_proto,
    const char* ns_name, const char* class_name, const JSClass* clasp,
    JSNative constructor_native, unsigned nargs, JSPropertySpec* ps,
    JSFunctionSpec* fs, JSPropertySpec* static_ps, JSFunctionSpec* static_fs,
    JS::MutableHandleObject prototype, JS::MutableHandleObject constructor);

GJS_USE
bool gjs_typecheck_instance(JSContext       *cx,
                            JS::HandleObject obj,
                            const JSClass   *static_clasp,
                            bool             throw_error);

GJS_JSAPI_RETURN_CONVENTION
JSObject *gjs_construct_object_dynamic(JSContext                  *cx,
                                       JS::HandleObject            proto,
                                       const JS::HandleValueArray& args);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_define_property_dynamic(JSContext       *cx,
                                 JS::HandleObject proto,
                                 const char      *prop_name,
                                 const char      *func_namespace,
                                 JSNative         getter,
                                 JSNative         setter,
                                 JS::HandleValue  private_slot,
                                 unsigned         flags);

template <class Base, typename Wrapped, GjsGlobalSlot SLOT>
class NativeObject {
    GJS_USE
    static bool typecheck(JSContext* cx, JS::HandleObject obj,
                          JS::CallArgs* args = nullptr) {
        return JS_InstanceOf(cx, obj, &Base::klass, args);
    }

    GJS_USE
    static Wrapped* for_js_nocheck(JSObject* obj) {
        return static_cast<Wrapped*>(JS_GetPrivate(obj));
    }

    // Default implementation for classes with no constructor
    // Can remove once 'if constexpr' can be used in create_prototype()
    GJS_JSAPI_RETURN_CONVENTION
    static Wrapped* constructor_impl(JSContext* cx, const JS::CallArgs& args) {
        gjs_throw_abstract_constructor_error(cx, args);
        return nullptr;
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
        JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

        if (!args.isConstructing()) {
            gjs_throw_constructor_error(cx);
            return false;
        }
        JS::RootedObject object(
            cx, JS_NewObjectForConstructor(cx, &Base::klass, args));
        if (!object)
            return false;

        Wrapped* priv = Base::constructor_impl(cx, args);
        if (!priv)
            return false;
        JS_SetPrivate(object, priv);

        args.rval().setObject(*object);
        return true;
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool abstract_constructor(JSContext* cx, unsigned argc,
                                     JS::Value* vp) {
        JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
        gjs_throw_abstract_constructor_error(cx, args);
        return false;
    }

 protected:
    // Debug methods

    static void debug_lifecycle(
        const void* wrapped_ptr GJS_USED_VERBOSE_LIFECYCLE,
        const void* obj GJS_USED_VERBOSE_LIFECYCLE,
        const char* message GJS_USED_VERBOSE_LIFECYCLE) {
        gjs_debug_lifecycle(Base::debug_topic, "[%p: JS wrapper %p] %s",
                            wrapped_ptr, obj, message);
    }

    static void finalize(JSFreeOp* fop, JSObject* obj) {
        Wrapped* priv = Base::for_js_nocheck(obj);

        // Call only GIWrapperBase's original method here, not any overrides;
        // e.g., we don't want to deal with a read barrier in ObjectInstance.
        NativeObject::debug_lifecycle(priv, obj, "Finalize");

        Base::finalize_impl(fop, priv);

        // Remove the pointer from the JSObject
        JS_SetPrivate(obj, nullptr);
    }

    static constexpr JSClassOps class_ops = {
        nullptr,  // addProperty
        nullptr,  // deleteProperty
        nullptr,  // enumerate
        nullptr,  // newEnumerate
        nullptr,  // resolve
        nullptr,  // mayResolve
        &NativeObject::finalize,
    };

    GJS_JSAPI_RETURN_CONVENTION
    static bool define_gtype_prop(JSContext* cx, JS::HandleObject ctor,
                                  JS::HandleObject proto G_GNUC_UNUSED) {
        return gjs_wrapper_define_gtype_prop(cx, ctor, Base::gtype());
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* prototype(JSContext* cx) {
        JS::RootedValue v_proto(cx, gjs_get_global_slot(cx, SLOT));
        g_assert(!v_proto.isUndefined() &&
                 "create_prototype() must be called before prototype()");
        g_assert(v_proto.isObject() &&
                 "Someone stored some weird value in a global slot");
        return &v_proto.toObject();
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                        bool* resolved) {
        Wrapped* priv = for_js(cx, obj);
        g_assert(priv && "resolve called on wrong object");
        return priv->resolve_impl(cx, obj, id, resolved);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool new_enumerate(JSContext* cx, JS::HandleObject obj,
                              JS::MutableHandleIdVector properties,
                              bool only_enumerable) {
        Wrapped* priv = for_js(cx, obj);
        g_assert(priv && "enumerate called on wrong object");
        return priv->new_enumerate_impl(cx, obj, properties, only_enumerable);
    }

 public:
    GJS_USE
    static Wrapped* for_js(JSContext* cx, JS::HandleObject obj,
                           JS::CallArgs& args) {
        return static_cast<Wrapped*>(
            JS_GetInstancePrivate(cx, obj, &Base::klass, &args));
    }

    GJS_USE
    static Wrapped* for_js(JSContext* cx, JS::HandleObject obj) {
        return static_cast<Wrapped*>(
            JS_GetInstancePrivate(cx, obj, &Base::klass, nullptr));
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool for_js_typecheck(JSContext* cx, JS::HandleObject obj,
                                 Wrapped** out, JS::CallArgs* args = nullptr) {
        if (!typecheck(cx, obj, args)) {
            if (!args) {
                const JSClass* obj_class = JS_GetClass(obj);
                gjs_throw_custom(cx, JSProto_TypeError, nullptr,
                                 "Object %p is not a subclass of %s, it's a %s",
                                 obj.get(), Base::klass.name, obj_class->name);
            }
            return false;
        }

        *out = for_js_nocheck(obj);
        return true;
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* create_prototype(JSContext* cx,
                                      JS::HandleObject module = nullptr) {
        // If we've been here more than once, we already have the proto
        JS::RootedValue v_proto(cx, gjs_get_global_slot(cx, SLOT));
        if (!v_proto.isUndefined()) {
            g_assert(v_proto.isObject() &&
                     "Someone stored some weird value in a global slot");
            return &v_proto.toObject();
        }

        // Create the prototype. If no createPrototype function is provided,
        // then the default is to create a plain object as the prototype.
        JS::RootedObject proto(cx);
        if (Base::class_spec.createPrototype)
            proto = Base::class_spec.createPrototype(
                cx, JSProto_Object /* FIXME */);
        else
            proto = JS_NewPlainObject(cx);
        if (!proto ||
            (Base::class_spec.prototypeProperties &&
             !JS_DefineProperties(cx, proto,
                                  Base::class_spec.prototypeProperties)) ||
            (Base::class_spec.prototypeFunctions &&
             !JS_DefineFunctions(cx, proto,
                                 Base::class_spec.prototypeFunctions)))
            return nullptr;
        gjs_set_global_slot(cx, SLOT, JS::ObjectValue(*proto));

        // Create the constructor. If no createConstructor function is provided,
        // then the default is to call NativeObject::constructor() which calls
        // Base::constructor_impl().
        JS::RootedObject ctor_obj(cx);
        if (!(Base::class_spec.flags & js::ClassSpec::DontDefineConstructor)) {
            if (Base::class_spec.defined()) {
                ctor_obj =
                    Base::class_spec.createConstructor(cx, JSProto_Object);
            } else {
                JSFunction* ctor =
                    JS_NewFunction(cx, &Base::constructor, 0 /* FIXME */,
                                   JSFUN_CONSTRUCTOR, Base::klass.name);
                ctor_obj = JS_GetFunctionObject(ctor);
            }
            if (!ctor_obj ||
                (Base::class_spec.constructorProperties &&
                 !JS_DefineProperties(
                     cx, ctor_obj, Base::class_spec.constructorProperties)) ||
                (Base::class_spec.constructorFunctions &&
                 !JS_DefineFunctions(cx, ctor_obj,
                                     Base::class_spec.constructorFunctions)) ||
                !JS_LinkConstructorAndPrototype(cx, ctor_obj, proto))
                return nullptr;
        }

        if (Base::class_spec.finishInit) {
            if (!Base::class_spec.finishInit(cx, ctor_obj, proto))
                return nullptr;
        }

        // If module is not given, we are defining a global class
        JS::RootedObject in_obj(cx, module);
        if (!in_obj)
            in_obj = gjs_get_import_global(cx);
        // FIXME: JS_InitClass defined the prototype as the constructor if no
        // constructor was given
        if (!ctor_obj)
            ctor_obj = proto;
        JS::RootedId class_name(cx,
                                gjs_intern_string_to_id(cx, Base::klass.name));
        if (class_name == JSID_VOID ||
            !JS_DefinePropertyById(cx, in_obj, class_name, ctor_obj,
                                   GJS_MODULE_PROP_FLAGS))
            return nullptr;

        gjs_debug(GJS_DEBUG_CONTEXT, "Initialized class %s prototype %p",
                  Base::klass.name, proto.get());
        return proto;
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* from_c_ptr(JSContext* cx, Wrapped* ptr) {
        JS::RootedObject proto(cx, Base::prototype(cx));
        if (!proto)
            return nullptr;

        JS::RootedObject wrapper(
            cx, JS_NewObjectWithGivenProto(cx, &Base::klass, proto));
        if (!wrapper)
            return nullptr;

        JS_SetPrivate(wrapper, Base::copy_ptr(ptr));
        return wrapper;
    }
};

template <typename Base, typename Wrapped, GjsGlobalSlot SLOT>
const JSClassOps NativeObject<Base, Wrapped, SLOT>::class_ops;

GJS_USE
JS::Value gjs_dynamic_property_private_slot(JSObject *accessor_obj);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_object_in_prototype_chain(JSContext* cx, JS::HandleObject proto,
                                   JS::HandleObject check_obj,
                                   bool* is_in_chain);

#endif  // GJS_JSAPI_CLASS_H_
