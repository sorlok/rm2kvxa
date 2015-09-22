#include <ruby.h>

//NOTE: A lot of this code comes from the mkxp project.
//      Consider this particular file a derivative work, 
//      licensed under the same terms:
//      https://github.com/Ancurio/mkxp

void Init_rmvx_small() {
  //Set up our bindings
  VALUE klass = rb_define_class("Table", rb_cObject);

  //TODO: The rest:
  //rb_define_alloc_func(klass, classAllocate<&TableType>);
  //serializableBindingInit<Table>(klass);
  //rb_define_class_method(klass, "_load", TableLoad);
  //_rb_define_method(klass, "initialize", tableInitialize);
  //_rb_define_method(klass, "initialize_copy", TableInitializeCopy);
  //_rb_define_method(klass, "resize", tableResize);
  //_rb_define_method(klass, "xsize", tableXSize);
  //_rb_define_method(klass, "ysize", tableYSize);
  //_rb_define_method(klass, "zsize", tableZSize);
  //_rb_define_method(klass, "[]", tableGetAt);
  //_rb_define_method(klass, "[]=", tableSetAt);
}

