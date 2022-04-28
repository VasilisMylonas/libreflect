#ifndef REFLECT_H
#define REFLECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct reflect_type reflect_type_t;
typedef struct reflect_fn reflect_fn_t;
typedef struct reflect_var reflect_var_t;
typedef struct reflect_member reflect_member_t;
typedef struct reflect_const reflect_const_t;
typedef struct reflect_obj reflect_obj_t;

typedef enum reflect_repr
{
    REFLECT_REPR_UNKNOWN = 0,

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
 * Initializes the library.
 *
 * This should be called before using any of the other reflect_* routines.
 *
 * @param argc The argc parameter from main.
 * @param argv The argv parameter from main.
 * @return 0 on success, non-zero on failure.
 */
int reflect_init(int argc, const char* argv[]);

/**
 * Performs library cleanup.
 */
void reflect_fini();

/**
 * Initializes a reflect_type_t object with information about a type.
 *
 * @param self Pointer to the reflect_type_t object to initialize.
 * @param name The name of the type.
 * @return NULL on error, otherwise self.
 */
reflect_type_t* reflect_type(reflect_type_t* self, const char* name);

/**
 * Initializes a reflect_type_t object with information about the underlying type of the typedef
 * represented by another reflect_type_t object.
 *
 * @param self The object representing the typedef.
 * @param out Pointer to the object to initialize.
 * @return NULL on error, otherwise out.
 */
reflect_type_t* reflect_typedef_type(reflect_type_t* self, reflect_type_t* out);

/**
 * Returns the size of a type.
 *
 * @param self The type.
 * @return The size.
 */
size_t reflect_type_size(reflect_type_t* self);

/**
 * Returns the name of a type.
 *
 * @param self The type.
 * @return The type name.
 */
const char* reflect_type_name(reflect_type_t* self);

/**
 * Returns the representation of a builtin type.
 *
 * @param self The type.
 * @return One of the reflect_repr_t constants.
 */
reflect_repr_t reflect_type_repr(reflect_type_t* self);

bool reflect_type_is_builtin(reflect_type_t* self);
bool reflect_type_is_array(reflect_type_t* self);
bool reflect_type_is_union(reflect_type_t* self);
bool reflect_type_is_struct(reflect_type_t* self);
bool reflect_type_is_enum(reflect_type_t* self);
bool reflect_type_is_typedef(reflect_type_t* self);
bool reflect_type_is_pointer(reflect_type_t* self);
bool reflect_type_is_c_string(reflect_type_t* self);

reflect_member_t* reflect_type_member_by_index(reflect_type_t* self,
                                               size_t index,
                                               reflect_member_t* out);
reflect_member_t* reflect_type_member_by_name(reflect_type_t* self,
                                              const char* name,
                                              reflect_member_t* out);

/**
 * Initializes a reflect_type_t object with information about the type of a member.
 *
 * @param self The member.
 * @param out Pointer to the reflect_type_t object to initialize.
 * @return NULL on error, otherwise out.
 */
reflect_type_t* reflect_member_type(reflect_member_t* self, reflect_type_t* out);

/**
 * Returns the name of a member.
 *
 * @param self The member.
 * @return The member name.
 */
const char* reflect_member_name(reflect_member_t* self);

/**
 * Returns the offset of a member from the beginning of its parent object.
 *
 * @param self The member.
 * @return The offset or -1 on error.
 */
ptrdiff_t reflect_member_offset(reflect_member_t* self);

/**
 * Initialize a reflect_fn_t object with information about a function.
 *
 * @param self Pointer to the reflect_fn_t object to initialize.
 * @param name The name of the function.
 * @return NULL on error, otherwise self.
 */
reflect_fn_t* reflect_fn(reflect_fn_t* self, const char* name);

/**
 * Initialize a reflect_type_t object with information about a function's return type.
 *
 * @param self The function.
 * @param out Pointer to the reflect_type_t object to initialize.
 * @return NULL on error, otherwise out.
 */
reflect_type_t* reflect_fn_ret_type(reflect_fn_t* self, reflect_type_t* out);

bool reflect_fn_is_extern(reflect_fn_t* self);
reflect_var_t* reflect_fn_param_by_index(reflect_fn_t* self, size_t index, reflect_var_t* out);
reflect_var_t* reflect_fn_param_by_name(reflect_fn_t* self, const char* name, reflect_var_t* out);
reflect_var_t* reflect_fn_var_by_index(reflect_fn_t* self, size_t index, reflect_var_t* out);
reflect_var_t* reflect_fn_var_by_name(reflect_fn_t* self, const char* name, reflect_var_t* out);

reflect_var_t* reflect_var(reflect_var_t* self, const char* name);
reflect_type_t* reflect_var_type(reflect_var_t* self, reflect_type_t* out);
const char* reflect_var_name(reflect_var_t* self);

void* reflect_get_member(void* object, reflect_member_t* member);

const char* reflect_file(void* obj);
size_t reflect_line(void* obj);
size_t reflect_column(void* obj);

#include <stdio.h>

typedef struct reflect_serializer
{
    void (*serialize)(void*, reflect_repr_t, size_t, FILE*);
    void (*begin_member)(const char*, FILE*);
    void (*end_member)(const char*, FILE*, bool is_last_member);
    void (*begin_struct)(const char*, FILE*);
    void (*end_struct)(const char*, FILE*);
} reflect_serializer_t;

#define REFLECT_SERIALIZER_JSON ((reflect_serializer_t*)1)
#define REFLECT_SERIALIZER_XML  ((reflect_serializer_t*)2)
#define REFLECT_SERIALIZER_C    ((reflect_serializer_t*)3)

FILE* reflect_serialize(const reflect_serializer_t* self,
                        void* object,
                        reflect_type_t* type,
                        FILE* output);

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
    void* domain;
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
