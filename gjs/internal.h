// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#ifndef GJS_INTERNAL_H_
#define GJS_INTERNAL_H_

#include <config.h>

#include <js/TypeDecls.h>
#include <jsapi.h>

bool gjs_load_internal_module(JSContext* cx, const char* identifier);

bool gjs_internal_compile_module(JSContext* cx, unsigned argc, JS::Value* vp);

bool gjs_internal_compile_internal_module(JSContext* cx, unsigned argc,
                                          JS::Value* vp);

bool gjs_internal_global_get_registry(JSContext* cx, unsigned argc,
                                      JS::Value* vp);

bool gjs_internal_global_import_sync(JSContext* cx, unsigned argc,
                                     JS::Value* vp);

bool gjs_internal_global_set_module_hook(JSContext* cx, unsigned argc,
                                         JS::Value* vp);

bool gjs_internal_global_set_module_meta_hook(JSContext* cx, unsigned argc,
                                              JS::Value* vp);

bool gjs_internal_set_module_private(JSContext* cx, unsigned argc,
                                     JS::Value* vp);

bool gjs_internal_global_set_module_resolve_hook(JSContext* cx, unsigned argc,
                                                 JS::Value* vp);

#endif  // GJS_INTERNAL_H_
