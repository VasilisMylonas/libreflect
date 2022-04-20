#include "reflect.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dwarf.h>
#include <inttypes.h>
#include <elfutils/libdw.h>

#ifdef LIBREFLECT_USE_EXCEPTIONS
#include <libexcept.h>
#define REFLECT_RAISE(error) throw(error)
#else
#include <errno.h>
#define REFLECT_RAISE(error)                    \
    __libreflect_report_error(error, __func__); \
    return 0
#endif

#define NOT_NULL(param)        \
    if (param == NULL)         \
    {                          \
        REFLECT_RAISE(EFAULT); \
    }

#define CHECK_NULL(value)       \
    void *temp = (void *)value; \
    if (temp == NULL)           \
    {                           \
        REFLECT_RAISE(ENODATA); \
    }                           \
    return temp

#define DIE_TO_REFLECT_OBJ(self, die, out)    \
    out->_impl.domain = self->_impl.domain;   \
    out->_impl.offset = dwarf_dieoffset(die); \
    return out;

#define REFLECT_OBJ_TO_DIE(self, die)                                               \
    if (dwarf_offdie((Dwarf *)self->_impl.domain, self->_impl.offset, die) == NULL) \
    {                                                                               \
        REFLECT_RAISE(EINVAL);                                                      \
    }

#define OBJ_BY_NAME(self, dom, tag, name)                                                    \
    NOT_NULL(self);                                                                          \
    NOT_NULL(dom);                                                                           \
    NOT_NULL(name);                                                                          \
    Dwarf_Die cu_die;                                                                        \
    Dwarf_Die die;                                                                           \
    Dwarf_CU *cu = NULL;                                                                     \
    while (dwarf_get_units((Dwarf *)dom, cu, &cu, NULL, NULL, &cu_die, NULL) == 0)           \
    {                                                                                        \
        if (dwarf_child(&cu_die, &die) != 0)                                                 \
        {                                                                                    \
            REFLECT_RAISE(ENODATA);                                                          \
        }                                                                                    \
        while (dwarf_siblingof(&die, &die) == 0)                                             \
        {                                                                                    \
            const char *die_name = dwarf_diename(&die);                                      \
            if ((dwarf_tag(&die) == tag) && die_name != NULL && strcmp(die_name, name) == 0) \
            {                                                                                \
                self->_impl.domain = dom;                                                    \
                self->_impl.offset = dwarf_dieoffset(&die);                                  \
                return self;                                                                 \
            }                                                                                \
        }                                                                                    \
    }                                                                                        \
    REFLECT_RAISE(ESRCH)

static void __libreflect_report_error(int error, const char *func)
{
    const char *msg = NULL;

    switch (error)
    {
    case ENODATA:
        msg = "No data available";
        break;
    case EINVAL:
        msg = "Invalid parameter";
        break;
    case ESRCH:
        msg = "No such entry";
        break;
    case EBADF:
        msg = "Could not open file";
        break;
    case EMEDIUMTYPE:
        msg = "Could not read debugging info";
        break;
    case EFAULT:
        msg = "NULL argument would cause access violation";
        break;
    default:
        return;
    }

    fprintf(stderr, "libreflect: %s at %s()\n", msg, func);
}

static bool obj_is(reflect_obj_t *self, int tag)
{
    Dwarf_Die die;
    if (dwarf_offdie((Dwarf *)self->domain, self->offset, &die) == NULL)
    {
        return false;
    }
    return dwarf_tag(&die) == tag;
}

static Dwarf_Die *die_type(Dwarf_Die *die, Dwarf_Die *out)
{
    Dwarf_Attribute attr;
    if (dwarf_attr(die, DW_AT_type, &attr) == NULL)
    {
        return NULL;
    }

    if (dwarf_formref_die(&attr, out) == NULL)
    {
        return NULL;
    }

    return out;
}

static bool die_is_type(Dwarf_Die *die)
{
    switch (dwarf_tag(die))
    {
    case DW_TAG_base_type:
    case DW_TAG_array_type:
    case DW_TAG_union_type:
    case DW_TAG_structure_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_subroutine_type:
    case DW_TAG_pointer_type:
        return true;
    default:
        return false;
    };
}

static reflect_type_t *peel_type(reflect_type_t *type)
{
    if (type == NULL)
    {
        return NULL;
    }

    Dwarf_Die die;
    if (dwarf_offdie((Dwarf *)type->_impl.domain, type->_impl.offset, &die) == NULL)
    {
        return NULL;
    }

    Dwarf_Die result;
    if (dwarf_peel_type(&die, &result) == -1)
    {
        return NULL;
    }

    type->_impl.offset = dwarf_dieoffset(&result);
    return type;
}

static const char *get_name(reflect_obj_t *obj)
{
    Dwarf_Die die;
    if (dwarf_offdie((Dwarf *)obj->domain, obj->offset, &die) == NULL)
    {
        return NULL;
    }

    const char *name = dwarf_diename(&die);
    if (name == NULL)
    {
        return NULL;
    }

    return name;
}

static reflect_obj_t *get_type(reflect_obj_t *self, reflect_obj_t *out)
{
    Dwarf_Die obj_die;
    if (dwarf_offdie((Dwarf *)self->domain, self->offset, &obj_die) == NULL)
    {
        return NULL;
    }

    Dwarf_Die type;
    if (die_type(&obj_die, &type) == NULL)
    {
        return NULL;
    }

    out->domain = self->domain;
    out->offset = dwarf_dieoffset(&type);
    return out;
}

static reflect_obj_t *child_by_name(reflect_obj_t *obj, int tag, const char *name, reflect_obj_t *out)
{
    // Extract DWARF DIE from object.
    Dwarf_Die obj_die;
    if (dwarf_offdie((Dwarf *)obj->domain, obj->offset, &obj_die) == NULL)
    {
        return NULL;
    }

    // Extract child DIE.
    Dwarf_Die child;
    if (dwarf_child(&obj_die, &child) != 0)
    {
        return NULL;
    }

    // Search
    do
    {
        if (dwarf_tag(&child) == tag && strcmp(dwarf_diename(&child), name) == 0)
        {
            out->domain = obj->domain;
            out->offset = dwarf_dieoffset(&child);
            return out;
        }
    } while (dwarf_siblingof(&child, &child) == 0);

    return NULL;
}

static reflect_obj_t *child_by_index(reflect_obj_t *obj, int tag, size_t index, reflect_obj_t *out)
{
    // Extract DWARF DIE from object.
    Dwarf_Die obj_die;
    if (dwarf_offdie((Dwarf *)obj->domain, obj->offset, &obj_die) == NULL)
    {
        return NULL;
    }

    // Extract child DIE.
    Dwarf_Die child;
    if (dwarf_child(&obj_die, &child) != 0)
    {
        return NULL;
    }

    // Search
    if (dwarf_tag(&child) == tag && index == 0)
    {
        out->domain = obj->domain;
        out->offset = dwarf_dieoffset(&child);
        return out;
    }

    for (size_t i = 1; dwarf_siblingof(&child, &child) == 0; i++)
    {
        if (dwarf_tag(&child) == tag && index == i)
        {
            out->domain = obj->domain;
            out->offset = dwarf_dieoffset(&child);
            return out;
        }
    }

    return NULL;
}

size_t reflect_line(void *obj)
{
    NOT_NULL(obj);

    int line = 0;
    if (dwarf_decl_line(obj, &line) != 0)
    {
        REFLECT_RAISE(ENODATA);
    }

    return (size_t)line;
}

size_t reflect_column(void *obj)
{
    NOT_NULL(obj);

    int col = 0;
    if (dwarf_decl_column(obj, &col) != 0)
    {
        REFLECT_RAISE(ENODATA);
    }

    return (size_t)col;
}

const char *reflect_file(void *obj)
{
    NOT_NULL(obj);
    CHECK_NULL(dwarf_decl_file(obj));
}

reflect_domain_t *reflect_domain_load(const char *file)
{
    NOT_NULL(file);

    int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        REFLECT_RAISE(EBADF);
    }

    Dwarf *d = dwarf_begin(fd, DWARF_C_READ);
    close(fd);

    if (d == NULL)
    {
        REFLECT_RAISE(EMEDIUMTYPE);
    }

    return (reflect_domain_t *)d;
}

void reflect_domain_unload(reflect_domain_t *self)
{
    // TODO
    if (self == NULL)
    {
        return;
    }

    if (dwarf_end((Dwarf *)self) != 0)
    {
        // TODO: Do we swallow this?
    }
}

bool reflect_type_is_typedef(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_typedef);
}

bool reflect_type_is_array(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_array_type);
}

bool reflect_type_is_builtin(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_base_type);
}

bool reflect_type_is_union(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_union_type);
}

bool reflect_type_is_struct(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_structure_type);
}

bool reflect_type_is_enum(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_enumeration_type);
}

bool reflect_type_is_pointer(reflect_type_t *self)
{
    NOT_NULL(self);
    return obj_is(&self->_impl, DW_TAG_pointer_type);
}

bool reflect_type_is_c_string(reflect_type_t *self)
{
    NOT_NULL(self);

    Dwarf_Die type;
    REFLECT_OBJ_TO_DIE(self, &type);

    if (die_type(&type, &type) == NULL)
    {
        return false;
    }

    if (dwarf_tag(&type) == DW_TAG_const_type)
    {
        if (die_type(&type, &type) == NULL)
        {
            return false;
        }

        return dwarf_tag(&type) == DW_TAG_base_type && strcmp(dwarf_diename(&type), "char") == 0;
    }

    return false;
}

size_t reflect_type_size(reflect_type_t *self)
{
    NOT_NULL(self);

    Dwarf_Die type;
    REFLECT_OBJ_TO_DIE(self, &type);
    int size = dwarf_bytesize(&type);
    if (size <= 0)
    {
        REFLECT_RAISE(ENODATA);
    }

    return (size_t)size;
}

const char *reflect_type_name(reflect_type_t *self)
{
    NOT_NULL(self);

    if (reflect_type_is_c_string(self))
    {
        return "const char*";
    }

    // TODO: very generic
    if (reflect_type_is_pointer(self))
    {
        return "void*";
    }

    CHECK_NULL(get_name(&self->_impl));
}

reflect_repr_t reflect_type_repr(reflect_type_t *self)
{
    NOT_NULL(self);

    Dwarf_Die type;
    REFLECT_OBJ_TO_DIE(self, &type);

    Dwarf_Attribute attr;
    if (dwarf_attr(&type, DW_AT_encoding, &attr) == NULL)
    {
        REFLECT_RAISE(ENODATA);
    }

    Dwarf_Word encoding;
    if (dwarf_formudata(&attr, &encoding) != 0)
    {
        REFLECT_RAISE(ENODATA);
    }

    switch (encoding)
    {
    case DW_ATE_float:
        return REFLECT_REPR_FLOAT;
    case DW_ATE_imaginary_float:
        return REFLECT_REPR_IMAGINARY;
    case DW_ATE_complex_float:
        return REFLECT_REPR_COMPLEX;
    case DW_ATE_decimal_float:
        return REFLECT_REPR_DECIMAL;
    case DW_ATE_signed:
        return REFLECT_REPR_INT;
    case DW_ATE_unsigned:
        return REFLECT_REPR_UINT;
    case DW_ATE_address:
        return REFLECT_REPR_POINTER;
    case DW_ATE_boolean:
        return REFLECT_REPR_BOOLEAN;
    case DW_ATE_unsigned_char:
        return REFLECT_REPR_UCHAR;
    case DW_ATE_signed_char:
        return REFLECT_REPR_SCHAR;

        // TODO:
        // case DW_ATE_ASCII:
        // case DW_ATE_UCS:
        // case DW_ATE_UTF:
        // case DW_ATE_signed_fixed:
        // case DW_ATE_unsigned_fixed:
        // case DW_ATE_packed_decimal:
        // case DW_ATE_numeric_string:
        // case DW_ATE_edited:
    default:
        return REFLECT_REPR_UNKNOWN;
    }
}

reflect_type_t *reflect_type(reflect_type_t *self, reflect_domain_t *dom, const char *name)
{
    OBJ_BY_NAME(self, dom, DW_TAG_typedef || die_is_type(&die), name);
}

reflect_member_t *reflect_type_member_by_index(reflect_type_t *self, size_t index, reflect_member_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_index(&self->_impl, DW_TAG_member, index, &out->_impl));
}

reflect_member_t *reflect_type_member_by_name(reflect_type_t *self, const char *name, reflect_member_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_name(&self->_impl, DW_TAG_member, name, &out->_impl));
}

reflect_type_t *reflect_member_type(reflect_member_t *self, reflect_type_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(peel_type((reflect_type_t *)get_type(&self->_impl, &out->_impl)));
}

const char *reflect_member_name(reflect_member_t *self)
{
    NOT_NULL(self);

    CHECK_NULL(get_name(&self->_impl));
}

reflect_type_t *reflect_typedef_type(reflect_type_t *self, reflect_type_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(peel_type((reflect_type_t *)get_type(&self->_impl, &out->_impl)));
}

void *reflect_get_member(void *object, reflect_member_t *member)
{
    NOT_NULL(object);
    NOT_NULL(member);

    Dwarf_Die die;
    REFLECT_OBJ_TO_DIE(member, &die);

    Dwarf_Attribute attr;
    if (dwarf_attr(&die, DW_AT_data_member_location, &attr) == NULL)
    {
        REFLECT_RAISE(ENODATA);
    }

    Dwarf_Word offset;

    switch (dwarf_whatform(&attr))
    {
    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_udata:
        if (dwarf_formudata(&attr, &offset) != 0)
        {
            REFLECT_RAISE(ENODATA);
        }
        return ((uint8_t *)object) + offset;
    default:
        REFLECT_RAISE(ENODATA);
    }
}

bool reflect_fn_is_extern(reflect_fn_t *self)
{
    NOT_NULL(self);

    Dwarf_Die fn;
    REFLECT_OBJ_TO_DIE(self, &fn);

    Dwarf_Attribute attr;
    bool value;

    if (dwarf_attr(&fn, DW_AT_external, &attr) == NULL)
    {
        REFLECT_RAISE(ENODATA);
    }

    if (dwarf_formflag(&attr, &value) != 0)
    {
        REFLECT_RAISE(ENODATA);
    }

    return value;
}

reflect_var_t *reflect_fn_var_by_index(reflect_fn_t *self, size_t index, reflect_var_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_index(&self->_impl, DW_TAG_variable, index, &out->_impl));
}

reflect_var_t *reflect_fn_var_by_name(reflect_fn_t *self, const char *name, reflect_var_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_name(&self->_impl, DW_TAG_variable, name, &out->_impl));
}

reflect_var_t *reflect_fn_param_by_index(reflect_fn_t *self, size_t index, reflect_var_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_index(&self->_impl, DW_TAG_formal_parameter, index, &out->_impl));
}

reflect_var_t *reflect_fn_param_by_name(reflect_fn_t *self, const char *name, reflect_var_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(child_by_name(&self->_impl, DW_TAG_formal_parameter, name, &out->_impl));
}

reflect_type_t *reflect_fn_ret_type(reflect_fn_t *self, reflect_type_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(peel_type((reflect_type_t *)get_type(&self->_impl, &out->_impl)));
}

reflect_type_t *reflect_var_type(reflect_var_t *self, reflect_type_t *out)
{
    NOT_NULL(self);
    NOT_NULL(out);

    CHECK_NULL(peel_type((reflect_type_t *)get_type(&self->_impl, &out->_impl)));
}

const char *reflect_var_name(reflect_var_t *self)
{
    NOT_NULL(self);

    CHECK_NULL(get_name(&self->_impl));
}

reflect_fn_t *reflect_fn(reflect_fn_t *self, reflect_domain_t *dom, const char *name)
{
    OBJ_BY_NAME(self, dom, DW_TAG_subprogram, name);
}

reflect_var_t *reflect_var(reflect_var_t *self, reflect_domain_t *dom, const char *name)
{
    OBJ_BY_NAME(self, dom, DW_TAG_variable, name);
}

static FILE *reflect_serialize_inner(const reflect_serializer_t *self, void *object, reflect_type_t *type, FILE *output)
{
    if (reflect_type_is_typedef(type))
    {
        reflect_type_t typedef_type;
        reflect_typedef_type(type, &typedef_type);
        reflect_serialize(self, object, &typedef_type, output);
        return output;
    }

    if (reflect_type_is_builtin(type))
    {
        self->serialize(object, reflect_type_repr(type), reflect_type_size(type), output);
        return output;
    }

    if (reflect_type_is_enum(type))
    {
        self->serialize(object, REFLECT_REPR_INT, sizeof(int), output);
        return output;
    }

    if (reflect_type_is_pointer(type))
    {
        if (*(void **)object == NULL)
        {
            self->serialize(object, REFLECT_REPR_POINTER, sizeof(void *), output);
        }
        else if (reflect_type_is_c_string(type))
        {
            self->serialize(object, REFLECT_REPR_STRING, sizeof(void *), output);
        }
        else
        {
            const char *reflect_var_name(reflect_var_t * self);

            Dwarf_Die inner_type;
            dwarf_offdie((Dwarf *)type->_impl.domain, type->_impl.offset, &inner_type);
            die_type(&inner_type, &inner_type);

            dwarf_peel_type(&inner_type, &inner_type);

            reflect_type_t type2 = {
                ._impl.domain = type->_impl.domain,
                ._impl.offset = dwarf_dieoffset(&inner_type),
            };

            reflect_serialize(self, *(void **)object, &type2, output);
        }

        return output;
    }

    if (reflect_type_is_struct(type))
    {
        // TODO: error
        const char *type_name = reflect_type_name(type);

        self->begin_struct(type_name, output);

        for (size_t i = 0; i < SIZE_MAX; i++)
        {
            reflect_member_t member;
            if (child_by_index(&type->_impl, DW_TAG_member, i, &member._impl) == NULL)
            {
                break;
            }

            // TODO: error
            const char *member_name = reflect_member_name(&member);

            reflect_type_t member_type;
            reflect_member_type(&member, &member_type);

            self->begin_member(member_name, output);

            reflect_serialize(self, reflect_get_member(object, &member), &member_type, output);

            // This is really dumb. Some formats, cough cough JSON, need special handling for the last member.
            // We have to provide this info to the user.
            reflect_member_t dummy;
            self->end_member(member_name, output, child_by_index(&type->_impl, DW_TAG_member, i + 1, &dummy._impl) == NULL);
        }

        self->end_struct(type_name, output);

        return output;
    }

    // TODO
    return NULL;
}

static void serialize_int(void *object, size_t size, FILE *output)
{
    switch (size)
    {
    case 1:
        fprintf(output, "%" PRIi8, *(int8_t *)object);
        break;
    case 2:
        fprintf(output, "%" PRIi16, *(int16_t *)object);
        break;
    case 4:
        fprintf(output, "%" PRIi32, *(int32_t *)object);
        break;
    case 8:
        fprintf(output, "%" PRIi64, *(int64_t *)object);
        break;
    default:
        // TODO
        break;
    }
}

static void serialize_uint(void *object, size_t size, FILE *output)
{
    switch (size)
    {
    case 1:
        fprintf(output, "%" PRIu8, *(uint8_t *)object);
        break;
    case 2:
        fprintf(output, "%" PRIu16, *(uint16_t *)object);
        break;
    case 4:
        fprintf(output, "%" PRIu32, *(uint32_t *)object);
        break;
    case 8:
        fprintf(output, "%" PRIu64, *(uint64_t *)object);
        break;
    default:
        // TODO
        break;
    }
}

static void serialize_float(void *object, size_t size, FILE *output)
{
    switch (size)
    {
    case 4:
        fprintf(output, "%f", *(float *)object);
        break;
    case 8:
        fprintf(output, "%lf", *(double *)object);
        break;
    case 16:
        fprintf(output, "%Lf", *(long double *)object);
    default:
        // TODO
        break;
    }
}

static void json_serialize(void *object, reflect_repr_t repr, size_t size, FILE *output)
{
    switch (repr)
    {
    case REFLECT_REPR_FLOAT:
        serialize_float(object, size, output);
        break;
    case REFLECT_REPR_INT:
        serialize_int(object, size, output);
        break;
    case REFLECT_REPR_UINT:
        serialize_uint(object, size, output);
        break;
    case REFLECT_REPR_POINTER:
        serialize_uint(object, sizeof(void *), output);
        break;
    case REFLECT_REPR_BOOLEAN:
        fputs((*(bool *)object) ? "true" : "false", output);
        break;
    case REFLECT_REPR_SCHAR:
    case REFLECT_REPR_UCHAR:
        fputc(*(char *)object, output);
        break;
    case REFLECT_REPR_STRING:
    {
        fputc('"', output);
        const char *s = *(const char **)object;
        size_t len = strlen(s);
        for (size_t i = 0; i < len; i++)
        {
            if (s[i] == '"')
            {
                fputc('\\', output);
            }
            fputc(s[i], output);
        }
        fputc('"', output);
        break;
    }
    default:
        // TODO
        break;
    }
}

static void json_begin_member(const char *name, FILE *output)
{
    fprintf(output, "\"%s\":", name);
}

static void json_end_member(const char *name, FILE *output, bool is_last_member)
{
    if (!is_last_member)
    {
        fprintf(output, ",");
    }

    (void)name;
}

static void json_begin_struct(const char *name, FILE *output)
{
    fprintf(output, "{");
    (void)name;
}

static void json_end_struct(const char *name, FILE *output)
{
    fprintf(output, "}");
    (void)name;
}

const reflect_serializer_t __libreflect_serializer_json = {
    .serialize = json_serialize,
    .begin_member = json_begin_member,
    .end_member = json_end_member,
    .begin_struct = json_begin_struct,
    .end_struct = json_end_struct,
};

static void xml_serialize(void *object, reflect_repr_t repr, size_t size, FILE *output)
{
    switch (repr)
    {
    case REFLECT_REPR_FLOAT:
        serialize_float(object, size, output);
        break;
    case REFLECT_REPR_INT:
        serialize_int(object, size, output);
        break;
    case REFLECT_REPR_UINT:
        serialize_uint(object, size, output);
        break;
    case REFLECT_REPR_POINTER:
        serialize_uint(object, sizeof(void *), output);
        break;
    case REFLECT_REPR_BOOLEAN:
        fputs((*(bool *)object) ? "true" : "false", output);
        break;
    case REFLECT_REPR_SCHAR:
    case REFLECT_REPR_UCHAR:
        fputc(*(char *)object, output);
        break;
    case REFLECT_REPR_STRING:
    {
        const char *s = *(const char **)object;
        size_t len = strlen(s);
        for (size_t i = 0; i < len; i++)
        {
            switch (s[i])
            {
            case '<':
                fputs("&#60;", output);
                break;
            case '&':
                fputs("&#38;", output);
                break;
            case '>':
                fputs("&#62;", output);
                break;
            case '\'':
                fputs("&#39;", output);
                break;
            case '"':
                fputs("&#34;", output);
                break;
            default:
                fputc(s[i], output);
                break;
            }
        }
        break;
    }
    default:
        // TODO
        break;
    }
}

static void xml_begin_member(const char *name, FILE *output)
{
    fprintf(output, "<%s>", name);
}

static void xml_end_member(const char *name, FILE *output, bool is_last_member)
{
    fprintf(output, "</%s>", name);
    (void)is_last_member;
}

static void xml_begin_struct(const char *name, FILE *output)
{
    (void)name;
    (void)output;
}

static void xml_end_struct(const char *name, FILE *output)
{
    (void)name;
    (void)output;
}

const reflect_serializer_t __libreflect_serializer_xml = {
    .serialize = xml_serialize,
    .begin_member = xml_begin_member,
    .end_member = xml_end_member,
    .begin_struct = xml_begin_struct,
    .end_struct = xml_end_struct,
};

static void c_begin_member(const char *name, FILE *output)
{
    fprintf(output, ".%s = ", name);
}

static void c_end_member(const char *name, FILE *output, bool is_last_member)
{
    fprintf(output, ",\n");
    (void)name;
    (void)is_last_member;
}

static void c_begin_struct(const char *name, FILE *output)
{
    fprintf(output, "{\n");
    (void)name;
}

static void c_end_struct(const char *name, FILE *output)
{
    fprintf(output, "}");
    (void)name;
}

const reflect_serializer_t __libreflect_serializer_c = {
    .serialize = json_serialize,
    .begin_member = c_begin_member,
    .end_member = c_end_member,
    .begin_struct = c_begin_struct,
    .end_struct = c_end_struct,
};

FILE *reflect_serialize(const reflect_serializer_t *self, void *object, reflect_type_t *type, FILE *output)
{
    NOT_NULL(self);
    NOT_NULL(object);
    NOT_NULL(type);
    NOT_NULL(output);

    return reflect_serialize_inner(self, object, type, output);
}
