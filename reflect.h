#ifndef REFLECT_H
#define REFLECT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct reflect_domain reflect_domain_t;
typedef struct reflect_type reflect_type_t;
typedef struct reflect_fn reflect_fn_t;
typedef struct reflect_var reflect_var_t;
typedef struct reflect_member reflect_member_t;
typedef struct reflect_const reflect_const_t;
typedef struct reflect_obj reflect_obj_t;

typedef enum reflect_repr
{
    REFLECT_REPR_UNKNOWN,

    REFLECT_REPR_FLOAT,
    REFLECT_REPR_IMAGINARY,
    REFLECT_REPR_COMPLEX,
    REFLECT_REPR_DECIMAL,
    REFLECT_REPR_INT,
    REFLECT_REPR_UINT,
    REFLECT_REPR_POINTER,
    REFLECT_REPR_BOOLEAN,
    REFLECT_REPR_UCHAR,
    REFLECT_REPR_SCHAR,
    REFLECT_REPR_STRING,
} reflect_repr_t;

/**
 * Loads an ELF file containing debug information and creating a reflection domain.
 *
 * The file is read into memory and then closed.
 *
 * @param file The path to the ELF file.
 * @return The reflection domain, NULL on error.
 */
reflect_domain_t *reflect_domain_load(const char *file);

/**
 * Unloads a reflection domain.
 *
 * This will invalidate all reflect_* objects created from the domain.
 *
 * @param self The domain to unload.
 */
void reflect_domain_unload(reflect_domain_t *self);

size_t reflect_type_size(reflect_type_t *self);
const char *reflect_type_name(reflect_type_t *self);
reflect_repr_t reflect_type_repr(reflect_type_t *self);
bool reflect_type_is_builtin(reflect_type_t *self);
bool reflect_type_is_array(reflect_type_t *self);
bool reflect_type_is_union(reflect_type_t *self);
bool reflect_type_is_struct(reflect_type_t *self);
bool reflect_type_is_enum(reflect_type_t *self);
bool reflect_type_is_typedef(reflect_type_t *self);
bool reflect_type_is_pointer(reflect_type_t *self);
bool reflect_type_is_c_string(reflect_type_t *self);
reflect_type_t *reflect_typedef_type(reflect_type_t *self, reflect_type_t *out);
reflect_type_t *reflect_type(reflect_type_t *self, reflect_domain_t *dom, const char *name);

reflect_member_t *reflect_type_member_by_index(reflect_type_t *self, size_t index, reflect_member_t *out);
reflect_member_t *reflect_type_member_by_name(reflect_type_t *self, const char *name, reflect_member_t *out);
reflect_type_t *reflect_member_type(reflect_member_t *self, reflect_type_t *out);
const char *reflect_member_name(reflect_member_t *self);

void *reflect_get_member(void *object, reflect_member_t *member);

reflect_fn_t *reflect_fn(reflect_fn_t *self, reflect_domain_t *dom, const char *name);
reflect_type_t *reflect_fn_ret_type(reflect_fn_t *self, reflect_type_t *out);
bool reflect_fn_is_extern(reflect_fn_t *self);
reflect_var_t *reflect_fn_param_by_index(reflect_fn_t *self, size_t index, reflect_var_t *out);
reflect_var_t *reflect_fn_param_by_name(reflect_fn_t *self, const char *name, reflect_var_t *out);
reflect_var_t *reflect_fn_var_by_index(reflect_fn_t *self, size_t index, reflect_var_t *out);
reflect_var_t *reflect_fn_var_by_name(reflect_fn_t *self, const char *name, reflect_var_t *out);

reflect_var_t *reflect_var(reflect_var_t *self, reflect_domain_t *dom, const char *name);
reflect_type_t *reflect_var_type(reflect_var_t *self, reflect_type_t *out);
const char *reflect_var_name(reflect_var_t *self);

const char *reflect_file(void *obj);
size_t reflect_line(void *obj);
size_t reflect_column(void *obj);

#include <stdio.h>

typedef struct reflect_serializer
{
    void (*serialize)(void *, reflect_repr_t, size_t, FILE *);
    void (*begin_member)(const char *, FILE *);
    void (*end_member)(const char *, FILE *, bool is_last_member);
    void (*begin_struct)(const char *, FILE *);
    void (*end_struct)(const char *, FILE *);
} reflect_serializer_t;

extern const reflect_serializer_t __libreflect_serializer_json;
extern const reflect_serializer_t __libreflect_serializer_xml;
extern const reflect_serializer_t __libreflect_serializer_c;

#define REFLECT_SERIALIZER_JSON (&__libreflect_serializer_json)
#define REFLECT_SERIALIZER_XML (&__libreflect_serializer_xml)
#define REFLECT_SERIALIZER_C (&__libreflect_serializer_c)

FILE *reflect_serialize(const reflect_serializer_t *self, void *object, reflect_type_t *type, FILE *output);

// TODO:
// inline functions
// extern variables
// artificial functions/variables
// reflect_const_t *reflect_type_const_by_index(reflect_type_t *self, size_t index);
// reflect_const_t *reflect_type_const_by_name(reflect_type_t *self, const char *name);
// reflect_const_t *reflect_type_const_by_value(reflect_type_t *self, long value);
// long reflect_const_value(reflect_const_t *self);
// bool reflect_type_is_const(reflect_type_t *self);
// bool reflect_type_is_volatile(reflect_type_t *self);
// bool reflect_type_is_restrict(reflect_type_t *self);
// bool reflect_type_is_atomic(reflect_type_t *self);

struct reflect_obj
{
    reflect_domain_t *domain;
    uint64_t offset;
};

struct reflect_type
{
    reflect_obj_t _impl;
};

struct reflect_member
{
    reflect_obj_t _impl;
};

struct reflect_fn
{
    reflect_obj_t _impl;
};

struct reflect_var
{
    reflect_obj_t _impl;
};

#endif // REFLECT_H
