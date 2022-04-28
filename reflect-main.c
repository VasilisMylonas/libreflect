#include "reflect.h"

#include <stddef.h>
#include <stdio.h>

struct point
{
    int x;
    int y;
    int z;
};

struct person
{
    const char* first_name;
    const char* last_name;
    int age;
    long date_born;
    char* obj_data;
    struct point* cords;

    float height;
    float weight;
};

#define print(x) reflect_pretty_print(&x, __func__, #x)

#define new_a(T)                                                                                   \
    (T[1])                                                                                         \
    {                                                                                              \
        {                                                                                          \
        }                                                                                          \
    }

void reflect_pretty_print(const void* object, const char* func_name, const char* var_name)
{
    reflect_fn_t* fn = reflect_fn(new_a(reflect_fn_t), func_name);
    reflect_var_t* var = reflect_fn_var_by_name(fn, var_name, new_a(reflect_var_t));
    reflect_type_t* type = reflect_var_type(var, new_a(reflect_type_t));

    // fprintf(stderr, "struct %s ", reflect_type_name(type));

    reflect_serialize(REFLECT_SERIALIZER_XML, (void*)object, type, stdout);
    // fputc('\n', stdout);
    (void)object;
}

typedef struct person person_t;

int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;

    reflect_init(argc, argv);

    struct point point = {
        .x = 1,
        .y = 2,
        .z = 3,
    };

    struct person p = {
        .first_name = "John \"The Reaper\"",
        .last_name = "Doe",
        .age = 69,
        .date_born = 33647585969,
        .obj_data = NULL,
        .height = 172.69,
        .weight = 69.69,
        .cords = &point,
    };

    print(p);

    reflect_fini();
}
