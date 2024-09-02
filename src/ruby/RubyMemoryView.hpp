/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __RUBY_MEMORY_H
#define __RUBY_MEMORY_H

#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <poesie/MemoryView.hpp>

struct RubyMemoryView {
    uint8_t* data;
    size_t size;
};

// Free function to be called when mruby's garbage collector cleans up the object.
static inline void memory_view_free(mrb_state *mrb, void *ptr) {
    struct RubyMemoryView *memview = (struct RubyMemoryView*)ptr;
    if (memview) {
        mrb_free(mrb, memview);
    }
}

// Data type definition for the MemoryView struct in mruby.
// Constructor: MemoryView.new(size)
static inline mrb_value memory_view_new(
        mrb_state *mrb,
        struct RClass *memory_view_class,
        const poesie::MemoryView& view) {

    static const struct mrb_data_type memory_view_data_type = {
        "MemoryView", memory_view_free,
    };

    auto rb_view = new RubyMemoryView{(uint8_t*)view.data(), view.size()};
    auto obj     = mrb_data_object_alloc(mrb, memory_view_class, rb_view, &memory_view_data_type);
    auto val     = mrb_obj_value(obj);

    return val;
}

// to_s method: Converts the data into a Ruby String.
static inline mrb_value memory_view_to_s(mrb_state *mrb, mrb_value self) {
    struct RubyMemoryView *memview = (RubyMemoryView*)DATA_PTR(self);
    return mrb_str_new(mrb, (const char*)memview->data, memview->size);
}

// size method: Get the size of the data.
static inline mrb_value memory_view_size(mrb_state *mrb, mrb_value self) {
    (void)mrb;
    struct RubyMemoryView *memview = (RubyMemoryView*)DATA_PTR(self);
    return mrb_fixnum_value(memview->size);
}

// [] operator: Access the byte at a given index.
static inline mrb_value memory_view_get_byte(mrb_state *mrb, mrb_value self) {
    RubyMemoryView *memview = (struct RubyMemoryView*)DATA_PTR(self);
    mrb_int index;

    mrb_get_args(mrb, "i", &index);

    if (index < 0 || (size_t)index >= memview->size) {
        mrb_raise(mrb, E_INDEX_ERROR, "index out of bounds");
    }

    return mrb_fixnum_value(memview->data[index]);
}

// []= operator: Modify the byte at a given index.
static inline mrb_value memory_view_set_byte(mrb_state *mrb, mrb_value self) {
    RubyMemoryView *memview = (RubyMemoryView*)DATA_PTR(self);
    mrb_int index;
    mrb_int value;

    mrb_get_args(mrb, "ii", &index, &value);

    if (index < 0 || (size_t)index >= memview->size) {
        mrb_raise(mrb, E_INDEX_ERROR, "index out of bounds");
    }

    if (value < 0 || value > 255) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "value out of byte range (0-255)");
    }

    memview->data[index] = (uint8_t)value;
    return mrb_fixnum_value(value);
}

// Define the RubyMemoryView class and its methods in mruby.
static inline struct RClass* mrb_mruby_memory_view_gem_init(mrb_state *mrb) {
    struct RClass *memory_view_class;

    memory_view_class = mrb_define_class(mrb, "MemoryView", mrb->object_class);
    MRB_SET_INSTANCE_TT(memory_view_class, MRB_TT_DATA);

    mrb_define_method(mrb, memory_view_class, "to_s", memory_view_to_s, MRB_ARGS_NONE());
    mrb_define_method(mrb, memory_view_class, "size", memory_view_size, MRB_ARGS_NONE());
    mrb_define_method(mrb, memory_view_class, "[]", memory_view_get_byte, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, memory_view_class, "[]=", memory_view_set_byte, MRB_ARGS_REQ(2));

    return memory_view_class;
}

#endif
