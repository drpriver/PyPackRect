//
// Copyright © 2022-2024, David Priver
//
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#include <assert.h>
#include <string.h>

#define PY_SSIZE_T_CLEAN
// Python's pytime.h triggers a visibility warning (at least on windows).
// We really don't care.
#ifdef __clang___
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvisibility"
#endif

#if defined(_WIN32) && defined(_DEBUG)
// Windows release of python only ships with release lib, but the _DEBUG macro
// enables a linker comment to link against the debug lib, which will fail at link time.
// The offending header is "pyconfig.h" in the python include directory.
// So undef it.
#undef _DEBUG
#include <Python.h>
#define _DEBUG

#else
#include <Python.h>
#endif

#ifdef __clang___
#pragma clang diagnostic pop
#endif

#if PY_MAJOR_VERSION < 3
#error "Only python3 or better is supported"
#endif

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 6
#error "Only python 3.6 or better is supported"
#endif

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 10
// Shim for older pythons.
static inline
int
PyModule_AddObjectRef(PyObject* mod, const char* name, PyObject* value){
    int result = PyModule_AddObject(mod, name, value);
    if(result == 0){ // 0 is success, so above call stole our ref
        Py_INCREF(value);
    }
    return result;
}
#endif

static
PyObject*
pack(PyObject* mod, PyObject* args, PyObject* kwargs);

// 0 on success
static
int
pack_them_rects(struct stbrp_node* nodes, int n_nodes, stbrp_rect* rects, int n_rects, int* width, int* height, int max_iters);

// 1 on success, 0 on failure
static
_Bool
pack_rects(struct stbrp_node* nodes, int n_nodes, stbrp_rect*rects, int n_rects, int x, int y);

// 1 if optimal (and sets *width and *height)
static
_Bool
packing_is_optimal(stbrp_rect* rects, int n_rects, int* width, int* height);

static
PyMethodDef packrect_methods[] = {
    {
        .ml_name = "pack",
        .ml_meth = (PyCFunction)pack,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        "pack(input, width=0, height=0, max_iters=10)\n"
        "--\n"
        "\n"
        "Packs them rects.\n"
        "\n"
        "Args:\n"
        "-----\n"
        "input:     A sequence of tuples that are (width, height, item).\n"
        "           `width` and `height` must be ints, but item can be\n"
        "           anything and will be returned back to you in the rects.\n"
        "\n"
        "width:     If non-zero, the packed rect will be no wider than this.\n"
        "           Otherwise, the minimum sized rect will be searched for.\n"
        "\n"
        "height:    Like width, but for height.\n"
        "\n"
        "max_iters: If width or height are zero, how many iterations to try\n"
        "           to solve for the minimum-sized rectangle.\n"
        "\n"
        "Returns:\n"
        "--------\n"
        "(rects, width, height)\n"
        "\n"
        "rects:  A list of (x, y, width, height, item) tuples.\n"
        "        The item is the original item you passed to input.\n"
        "        There is no guarantee on the order of these rects.\n"
        "\n"
        "width:  The width of the rectangle that contains the rects.\n"
        "\n"
        "height: The height of the rectangle that contains the rects.\n"
        "\n"
        "Throws:\n"
        "-------\n"
        "If the rects can't be packed into the rectangle given by width\n"
        "and/or height, then ValueError will be thrown.\n"
        "\n"
        "ValueError can also be thrown for invalid inputs.\n"
        ,
    },
    {NULL, NULL, 0, NULL}
};

static
PyModuleDef packrect = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name="packrect",
    .m_doc= ""
        "Wrapper around stb_rect_pack.\n"
        ,
    .m_size=-1,
    .m_methods=packrect_methods,
    .m_slots=NULL,
    .m_traverse=NULL,
    .m_clear=NULL,
    .m_free=NULL,
};

PyMODINIT_FUNC
PyInit_packrect(void){
    PyObject* mod = PyModule_Create(&packrect);
    PyModule_AddStringConstant(mod, "__version__", "1.0.5");
    PyObject* version = Py_BuildValue("iii", 1, 0, 5);
    PyModule_AddObjectRef(mod, "version", version);
    Py_XDECREF(version);
    return mod;
}

// returns list of [(x, y, w, h, o), ...], width, height
// takes sequence of [(w, h, o)]
static
PyObject*
pack(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* input;
    int max_iters = 0;
    int width = 0;
    int height = 0;
    const char* const keywords[] = {"input", "width", "height", "max_iters", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iii:pack", (char**)keywords, &input,  &width, &height, &max_iters)){
        return NULL;
    }
    if(!PySequence_Check(input)){
        PyErr_SetString(PyExc_TypeError, "input must be a sequence");
        return NULL;
    }
    const Py_ssize_t len = PySequence_Length(input);
    if(len < 0) return NULL;
    if(!len) {
        PyErr_SetString(PyExc_ValueError, "input must be non-empty");
        return NULL;
    }
    // int n_nodes = len*8;
    int n_nodes = 8000;
    int n_rects = len;
    stbrp_rect* rects = PyMem_RawCalloc(n_rects, sizeof *rects);
    stbrp_node* nodes = PyMem_RawMalloc(n_nodes * sizeof *nodes);
    if(!nodes || !rects){
        PyMem_RawFree(rects);
        PyMem_RawFree(nodes);
        PyErr_SetString(PyExc_MemoryError, "oom when allocating temp buffers");
        return NULL;
    }
    for(Py_ssize_t i = 0; i < len; i++){
        PyObject* o = PySequence_GetItem(input, i);
        if(!o) goto cleanup;
        PyObject* unused;
        if(!PyArg_ParseTuple(o, "iiO:pack", &rects[i].w, &rects[i].h, &unused)) goto cleanup;
        rects[i].id = (int)i;
        Py_XDECREF(o);
    }
    int err = pack_them_rects(nodes, n_nodes, rects, n_rects, &width, &height, max_iters);
    if(err){
        PyErr_SetString(PyExc_ValueError, "Failed to pack them rects");
        goto cleanup;
    }
    // Set width and height to actual width and height.
    width = 0; height = 0;
    for(int i = 0; i < n_rects; i++){
        int w = rects[i].x + rects[i].w;
        if(w > width) width = w;
        int h = rects[i].y + rects[i].h;
        if(h > height) height = h;
    }
    PyObject* lst = PyList_New(len);
    if(!lst) goto cleanup;
    for(Py_ssize_t i = 0; i < len; i++){
        stbrp_rect* r = &rects[i];
        PyObject* o = PySequence_GetItem(input, r->id); // new ref
        if(!o){
            Py_XDECREF(lst);
            goto cleanup;
        }
        PyObject* obj = PyTuple_GetItem(o, 2); // borrowed
        if(!obj){
            Py_XDECREF(o);
            Py_XDECREF(lst);
            goto cleanup;
        }
        assert(r->was_packed);
        PyObject* tup = Py_BuildValue("iiiiO", r->x, r->y, r->w, r->h, obj); // new ref
        Py_XDECREF(o);
        PyList_SET_ITEM(lst, i, tup); // steals ref
    }
    PyMem_RawFree(rects);
    PyMem_RawFree(nodes);
    PyObject* result = Py_BuildValue("Oii", lst, width, height);
    Py_XDECREF(lst);
    return result;

    {
        cleanup:
        PyMem_RawFree(rects);
        PyMem_RawFree(nodes);
        return NULL;
    }
}


// 1 on success, 0 on failure
static
_Bool
pack_rects(struct stbrp_node* nodes, int n_nodes, stbrp_rect*rects, int n_rects, int x, int y){
    if(x <= 0) return 0;
    if(y <= 0) return 0;
    memset(nodes, 0, sizeof(*nodes)*n_nodes);
    for(int i = 0; i < n_rects; i++){
        rects[i].was_packed = 0;
        rects[i].x = 0;
        rects[i].y = 0;
    }
    struct stbrp_context ctx = {0};
    stbrp_init_target(&ctx, x, y, nodes, n_nodes);
    int packed = stbrp_pack_rects(&ctx, rects, n_rects);
    return packed;
}

// 1 if optimal (and sets *width and *height)
static
_Bool
packing_is_optimal(stbrp_rect* rects, int n_rects, int* width, int* height){
    int total_area = 0;
    int w = 0, h = 0;
    for(int i = 0; i < n_rects; i++){
        total_area += rects[i].w * rects[i].h;
        if(rects[i].w + rects[i].x > w) w = rects[i].w + rects[i].x;
        if(rects[i].h + rects[i].y > h) h = rects[i].h + rects[i].y;
    }
    if(total_area == w * h){
        *width = w;
        *height = h;
        return 1;
    }
    return 0;
}

// 0 on success
static
int
pack_them_rects(struct stbrp_node* nodes, int n_nodes, stbrp_rect* rects, int n_rects, int* width, int* height, int max_iters){
    if(max_iters <= 0) max_iters = 20;
    int w = *width;
    int h = *height;
    if(w < 0) return 1;
    if(h < 0) return 1;
    if(!w || !h){
        enum {UPPER=20000};
        int x_min=w?w:UPPER/2;
        int y_min=h?h:UPPER/2;
        if(!pack_rects(nodes, n_nodes, rects, n_rects, x_min, y_min))
            return 1;
        if(packing_is_optimal(rects, n_rects, width, height))
            return 0;
        int best=UPPER/2*UPPER/2;
        int spread = UPPER/3;
        int step = spread/4;
        while(step && max_iters--){
            int y_ = y_min;
            int x_ = x_min;
            if(!w && !h){
                for(int y = y_-spread; y < y_+spread; y += step){
                    for(int x = x_-spread; x < x_+spread; x += step){
                        if(x * y >= best) continue;
                        if(pack_rects(nodes, n_nodes, rects, n_rects, x, y)){
                            if(packing_is_optimal(rects, n_rects, width, height))
                                return 0;
                            best = x * y;
                            x_min = x;
                            y_min = y;
                        }
                    }
                }
            }
            else if(!w){
                int y = y_min;
                for(int x = x_-spread; x < x_+spread; x += step){
                    if(x * y >= best) continue;
                    if(pack_rects(nodes, n_nodes, rects, n_rects, x, y)){
                        if(packing_is_optimal(rects, n_rects, width, height))
                            return 0;
                        best = x * y;
                        x_min = x;
                    }
                }
            }
            else {
                int x = x_min;
                for(int y = y_-spread; y < y_+spread; y += step){
                    if(x * y >= best) continue;
                    if(pack_rects(nodes, n_nodes, rects, n_rects, x, y)){
                        if(packing_is_optimal(rects, n_rects, width, height))
                            return 0;
                        best = x * y;
                        y_min = y;
                    }
                }
            }
            spread /= 2;
            step /= 2;
        }
        w = x_min;
        h = y_min;
        *width = w;
        *height = h;
    }
    int packed = pack_rects(nodes, n_nodes, rects, n_rects, w, h);
    if(!packed) return 1;
    return 0;
}
