/*
* Copyright (c) 2017 Calvin Rose
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

#ifndef DST_COMPILE_H
#define DST_COMPILE_H

#include <dst/dst.h>
#include <setjmp.h>
#include "opcodes.h"

/* Compiler typedefs */
typedef struct DstCompiler DstCompiler;
typedef struct FormOptions FormOptions;
typedef struct SlotTracker SlotTracker;
typedef struct DstScope DstScope;
typedef struct DstSlot DstSlot;
typedef struct DstFopts DstFopts;
typedef struct DstCFunctionOptimizer DstCFunctionOptimizer;

#define DST_SLOT_CONSTANT 0x10000
#define DST_SLOT_NAMED 0x20000
#define DST_SLOT_MUTABLE 0x40000
#define DST_SLOT_REF 0x80000
#define DST_SLOT_RETURNED 0x100000
/* Needed for handling single element arrays as global vars. */

#define DST_SLOTTYPE_ANY 0xFFFF

/* A stack slot */
struct DstSlot {
    int32_t index;
    int32_t envindex; /* 0 is local, positive number is an upvalue */
    uint32_t flags;
    Dst constant; /* If the slot has a constant value */
};

/* Slot and map pairing */
typedef struct DstSM {
    DstSlot slot;
    const Dst *map;
} DstSM;

/* Special forms that need support */
/* cond
 * while (continue, break)
 * quote
 * fn
 * def
 * var
 * varset
 * do
 * apply (overloaded with normal function)
 */

#define DST_SCOPE_FUNCTION 1
#define DST_SCOPE_ENV 2
#define DST_SCOPE_TOP 4
#define DST_SCOPE_UNUSED 8

/* A symbol and slot pair */
typedef struct SymPair {
    const uint8_t *sym;
    DstSlot slot;
} SymPair;

/* A lexical scope during compilation */
struct DstScope {

    /* Constants for this funcdef */
    Dst *consts;

    /* Map of symbols to slots. Use a simple linear scan for symbols. */
    SymPair *syms;

    /* Bit vector with allocated slot indices. Used to allocate new slots */
    uint32_t *slots;
    int32_t smax;

    /* FuncDefs */
    DstFuncDef **defs;

    /* Referenced closure environents. The values at each index correspond
     * to which index to get the environment from in the parent. The environment
     * that corresponds to the direct parent's stack will always have value 0. */
    int32_t *envs;

    int32_t bytecode_start;
    int flags;
};

/* Compilation state */
struct DstCompiler {
    int recursion_guard;
    DstScope *scopes;
    
    uint32_t *buffer;
    int32_t *mapbuffer;

    /* Hold the environment */
    Dst env;

    DstCompileResult result;
};

#define DST_FOPTS_TAIL 0x10000
#define DST_FOPTS_HINT 0x20000
#define DST_FOPTS_DROP 0x40000

/* Options for compiling a single form */
struct DstFopts {
    DstCompiler *compiler;
    Dst x;
    const Dst *sourcemap;
    uint32_t flags; /* bit set of accepted primitive types */
    DstSlot hint;
};

/* A grouping of optimizations on a cfunction given certain conditions
 * on the arguments (such as all constants, or some known types). The appropriate
 * optimizations should be tried before compiling a normal function call. */
typedef struct DstCFunOptimizer {
    DstCFunction cfun;
    DstSlot (*optimize)(DstFopts opts, int32_t argn, const Dst *argv);
} DstCFunOptimizer;

/* A grouping of a named special and the corresponding compiler fragment */
typedef struct DstSpecial {
    const char *name;
    DstSlot (*compile)(DstFopts opts, int32_t argn, const Dst *argv);
} DstSpecial;

/****************************************************/

/* Get a cfunction optimizer. Return NULL if none exists.  */
const DstCFunOptimizer *dstc_cfunopt(DstCFunction cfun);

/* Get a special. Return NULL if none exists */
const DstSpecial *dstc_special(const uint8_t *name);

/* Check error */
int dstc_iserr(DstFopts *opts);

/* Allocate a slot index */
int32_t dstc_lsloti(DstCompiler *c);

/* Free a slot index */
void dstc_sfreei(DstCompiler *c, int32_t index);

/* Allocate a local near (n) slot and return its index. Slot
 * has maximum index max. Common value for max would be 0xFF,
 * the highest slot index representable with one byte. */
int32_t dstc_lslotn(DstCompiler *c, int32_t max, int32_t nth);

/* Free a slot */
void dstc_freeslot(DstCompiler *c, DstSlot s);

/* Add a slot to a scope with a symbol associated with it (def or var). */
void dstc_nameslot(DstCompiler *c, const uint8_t *sym, DstSlot s);

/* Realize any slot to a local slot. Call this to get a slot index
 * that can be used in an instruction. */
int32_t dstc_preread(
        DstCompiler *c,
        const Dst *sourcemap,
        int32_t max,
        int nth,
        DstSlot s);

/* Call this to release a read handle after emitting the instruction. */
void dstc_postread(DstCompiler *c, DstSlot s, int32_t index);

/* Move value from one slot to another. Cannot copy to constant slots. */
void dstc_copy(
        DstCompiler *c,
        const Dst *sourcemap,
        DstSlot dest,
        DstSlot src);

/* Throw away some code after checking that it is well formed. */
void dstc_throwaway(DstFopts opts);

/* Generate the return instruction for a slot. */
DstSlot dstc_return(DstCompiler *c, const Dst *sourcemap, DstSlot s);

/* Get a target slot for emitting an instruction. Will always return
 * a local slot. */
DstSlot dstc_gettarget(DstFopts opts);

/* Get a bunch of slots for function arguments */
DstSM *dstc_toslots(DstFopts opts, int32_t start);

/* Get a bunch of slots for function arguments */
DstSM *dstc_toslotskv(DstFopts opts);

/* Push slots load via dstc_toslots. */
void dstc_pushslots(DstFopts opts, DstSM *sms);

/* Free slots loaded via dstc_toslots */
void dstc_freeslots(DstFopts opts, DstSM *sms);

/* Store an error */
void dstc_error(DstCompiler *c, const Dst *sourcemap, const uint8_t *m);
void dstc_cerror(DstCompiler *c, const Dst *sourcemap, const char *m);

/* Dispatch to correct form compiler */
DstSlot dstc_value(DstFopts opts);

/* Use these to get sub options. They will traverse the source map so
 * compiler errors make sense. Then modify the returned options. */
DstFopts dstc_getindex(DstFopts opts, int32_t index);
DstFopts dstc_getkey(DstFopts opts, Dst key);
DstFopts dstc_getvalue(DstFopts opts, Dst key);

/* Push and pop from the scope stack */
void dstc_scope(DstCompiler *c, int newfn);
void dstc_popscope(DstCompiler *c);
DstFuncDef *dstc_pop_funcdef(DstCompiler *c);

/* Create a destory slots */
DstSlot dstc_cslot(Dst x);

/* Free a slot */
void dstc_freeslot(DstCompiler *c, DstSlot slot);

/* Search for a symbol */
DstSlot dstc_resolve(DstCompiler *c, const Dst *sourcemap, const uint8_t *sym);

/* Emit instructions. */
void dstc_emit(DstCompiler *c, const Dst *sourcemap, uint32_t instr);

#endif
