#ifndef RUBY_INSNHELPER_H
#define RUBY_INSNHELPER_H
/**********************************************************************

  insnhelper.h - helper macros to implement each instructions

  $Author$
  created at: 04/01/01 15:50:34 JST

  Copyright (C) 2004-2007 Koichi Sasada

**********************************************************************/

RUBY_EXTERN VALUE ruby_vm_const_missing_count;
RUBY_EXTERN rb_serial_t ruby_vm_constant_cache_invalidations;
RUBY_EXTERN rb_serial_t ruby_vm_constant_cache_misses;
RUBY_EXTERN rb_serial_t ruby_vm_global_cvar_state;

#if USE_YJIT || USE_RJIT // We want vm_insns_count on any JIT-enabled build.
// Increment vm_insns_count for --yjit-stats. We increment this even when
// --yjit or --yjit-stats is not used because branching to skip it is slower.
// We also don't use ATOMIC_INC for performance, allowing inaccuracy on Ractors.
#define JIT_COLLECT_USAGE_INSN(insn) rb_vm_insns_count++
#else
#define JIT_COLLECT_USAGE_INSN(insn) // none
#endif

#if VM_COLLECT_USAGE_DETAILS
#define COLLECT_USAGE_INSN(insn)           vm_collect_usage_insn(insn)
#define COLLECT_USAGE_OPERAND(insn, n, op) vm_collect_usage_operand((insn), (n), ((VALUE)(op)))
#define COLLECT_USAGE_REGISTER(reg, s)     vm_collect_usage_register((reg), (s))
#else
#define COLLECT_USAGE_INSN(insn)           JIT_COLLECT_USAGE_INSN(insn)
#define COLLECT_USAGE_OPERAND(insn, n, op) // none
#define COLLECT_USAGE_REGISTER(reg, s)     // none
#endif

/**********************************************************/
/* deal with stack                                        */
/**********************************************************/

#define PUSH(x) (SET_SV(x), INC_SP(1))
#define TOPN(n) (*(GET_SP()-(n)-1))
#define POPN(n) (DEC_SP(n))
#define POP()   (DEC_SP(1))
#define STACK_ADDR_FROM_TOP(n) (GET_SP()-(n))

/**********************************************************/
/* deal with registers                                    */
/**********************************************************/

#define VM_REG_CFP (reg_cfp)
#define VM_REG_PC  (VM_REG_CFP->pc)
#define VM_REG_SP  (VM_REG_CFP->sp)
#define VM_REG_EP  (VM_REG_CFP->ep)

#define RESTORE_REGS() do { \
    VM_REG_CFP = ec->cfp; \
} while (0)

#if VM_COLLECT_USAGE_DETAILS
enum vm_regan_regtype {
    VM_REGAN_PC = 0,
    VM_REGAN_SP = 1,
    VM_REGAN_EP = 2,
    VM_REGAN_CFP = 3,
    VM_REGAN_SELF = 4,
    VM_REGAN_ISEQ = 5
};
enum vm_regan_acttype {
    VM_REGAN_ACT_GET = 0,
    VM_REGAN_ACT_SET = 1
};

#define COLLECT_USAGE_REGISTER_HELPER(a, b, v) \
  (COLLECT_USAGE_REGISTER((VM_REGAN_##a), (VM_REGAN_ACT_##b)), (v))
#else
#define COLLECT_USAGE_REGISTER_HELPER(a, b, v) (v)
#endif

/* PC */
#define GET_PC()           (COLLECT_USAGE_REGISTER_HELPER(PC, GET, VM_REG_PC))
#define SET_PC(x)          (VM_REG_PC = (COLLECT_USAGE_REGISTER_HELPER(PC, SET, (x))))
#define GET_CURRENT_INSN() (*GET_PC())
#define GET_OPERAND(n)     (GET_PC()[(n)])
#define ADD_PC(n)          (SET_PC(VM_REG_PC + (n)))
#define JUMP(dst)          (SET_PC(VM_REG_PC + (dst)))

/* frame pointer, environment pointer */
#define GET_CFP()  (COLLECT_USAGE_REGISTER_HELPER(CFP, GET, VM_REG_CFP))
#define GET_EP()   (COLLECT_USAGE_REGISTER_HELPER(EP, GET, VM_REG_EP))
#define SET_EP(x)  (VM_REG_EP = (COLLECT_USAGE_REGISTER_HELPER(EP, SET, (x))))
#define GET_LEP()  (VM_EP_LEP(GET_EP()))

/* SP */
#define GET_SP()   (COLLECT_USAGE_REGISTER_HELPER(SP, GET, VM_REG_SP))
#define SET_SP(x)  (VM_REG_SP  = (COLLECT_USAGE_REGISTER_HELPER(SP, SET, (x))))
#define INC_SP(x)  (VM_REG_SP += (COLLECT_USAGE_REGISTER_HELPER(SP, SET, (x))))
#define DEC_SP(x)  (VM_REG_SP -= (COLLECT_USAGE_REGISTER_HELPER(SP, SET, (x))))
#define SET_SV(x)  (*GET_SP() = rb_ractor_confirm_belonging(x))
  /* set current stack value as x */

/* instruction sequence C struct */
#define GET_ISEQ() (GET_CFP()->iseq)

/**********************************************************/
/* deal with variables                                    */
/**********************************************************/

#define GET_PREV_EP(ep)                ((VALUE *)((uintptr_t)(ep)[VM_ENV_DATA_INDEX_SPECVAL] & ~0x03))

/**********************************************************/
/* deal with values                                       */
/**********************************************************/

#define GET_SELF() (COLLECT_USAGE_REGISTER_HELPER(SELF, GET, GET_CFP()->self))

/**********************************************************/
/* deal with control flow 2: method/iterator              */
/**********************************************************/

/* set fastpath when cached method is *NOT* protected
 * because inline method cache does not care about receiver.
 */

static inline void
CC_SET_FASTPATH(const struct rb_callcache *cc, vm_call_handler func, bool enabled)
{
    if (LIKELY(enabled)) {
        vm_cc_call_set(cc, func);
    }
}

#define GET_BLOCK_HANDLER() (GET_LEP()[VM_ENV_DATA_INDEX_SPECVAL])

/**********************************************************/
/* deal with control flow 3: exception                    */
/**********************************************************/


/**********************************************************/
/* deal with stack canary                                 */
/**********************************************************/

#if VM_CHECK_MODE > 0
#define SETUP_CANARY(cond) \
    VALUE *canary = 0; \
    if (cond) { \
        canary = GET_SP(); \
        SET_SV(vm_stack_canary); \
    } \
    else {\
        SET_SV(Qfalse); /* cleanup */ \
    }
#define CHECK_CANARY(cond, insn) \
    if (cond) { \
        if (*canary == vm_stack_canary) { \
            *canary = Qfalse; /* cleanup */ \
        } \
        else { \
            rb_vm_canary_is_found_dead(insn, *canary); \
        } \
    }
#else
#define SETUP_CANARY(cond)       if (cond) {} else {}
#define CHECK_CANARY(cond, insn) if (cond) {(void)(insn);}
#endif

/**********************************************************/
/* others                                                 */
/**********************************************************/

#define CALL_SIMPLE_METHOD() do { \
    rb_snum_t insn_width = attr_width_opt_send_without_block(0); \
    ADD_PC(-insn_width); \
    DISPATCH_ORIGINAL_INSN(opt_send_without_block); \
} while (0)

#define GET_GLOBAL_CVAR_STATE() (ruby_vm_global_cvar_state)
#define INC_GLOBAL_CVAR_STATE() (++ruby_vm_global_cvar_state)

static inline struct vm_throw_data *
THROW_DATA_NEW(VALUE val, const rb_control_frame_t *cf, int st)
{
    struct vm_throw_data *obj = (struct vm_throw_data *)rb_imemo_new(imemo_throw_data, val, (VALUE)cf, 0, 0);
    obj->throw_state = st;
    return obj;
}

static inline VALUE
THROW_DATA_VAL(const struct vm_throw_data *obj)
{
    VM_ASSERT(THROW_DATA_P(obj));
    return obj->throw_obj;
}

static inline const rb_control_frame_t *
THROW_DATA_CATCH_FRAME(const struct vm_throw_data *obj)
{
    VM_ASSERT(THROW_DATA_P(obj));
    return obj->catch_frame;
}

static inline int
THROW_DATA_STATE(const struct vm_throw_data *obj)
{
    VM_ASSERT(THROW_DATA_P(obj));
    return obj->throw_state;
}

static inline int
THROW_DATA_CONSUMED_P(const struct vm_throw_data *obj)
{
    VM_ASSERT(THROW_DATA_P(obj));
    return obj->flags & THROW_DATA_CONSUMED;
}

static inline void
THROW_DATA_CATCH_FRAME_SET(struct vm_throw_data *obj, const rb_control_frame_t *cfp)
{
    VM_ASSERT(THROW_DATA_P(obj));
    obj->catch_frame = cfp;
}

static inline void
THROW_DATA_STATE_SET(struct vm_throw_data *obj, int st)
#ifndef INTERNAL_IMEMO_H                                 /*-*-C-*-vi:se ft=c:*/
#define INTERNAL_IMEMO_H
/**
 * @author     Ruby developers <ruby-core@ruby-lang.org>
 * @copyright  This  file  is   a  part  of  the   programming  language  Ruby.
 *             Permission  is hereby  granted,  to  either redistribute  and/or
 *             modify this file, provided that  the conditions mentioned in the
 *             file COPYING are met.  Consult the file for details.
 * @brief      IMEMO: Internal memo object.
 */
#include "ruby/internal/config.h"
#include <stddef.h>             /* for size_t */
#include "internal/array.h"     /* for rb_ary_hidden_new_fill */
#include "ruby/internal/stdbool.h"     /* for bool */
#include "ruby/ruby.h"          /* for rb_block_call_func_t */

#ifndef IMEMO_DEBUG
# define IMEMO_DEBUG 0
#endif

#define IMEMO_MASK   0x0f

/* FL_USER0 to FL_USER3 is for type */
#define IMEMO_FL_USHIFT (FL_USHIFT + 4)
#define IMEMO_FL_USER0 FL_USER4
#define IMEMO_FL_USER1 FL_USER5
#define IMEMO_FL_USER2 FL_USER6
#define IMEMO_FL_USER3 FL_USER7
#define IMEMO_FL_USER4 FL_USER8
#define IMEMO_FL_USER5 FL_USER9

enum imemo_type {
    imemo_env            =  0,
    imemo_cref           =  1, /*!< class reference */
    imemo_svar           =  2, /*!< special variable */
    imemo_throw_data     =  3,
    imemo_ifunc          =  4, /*!< iterator function */
    imemo_memo           =  5,
    imemo_ment           =  6,
    imemo_iseq           =  7,
    imemo_tmpbuf         =  8,
    imemo_ast            =  9,
    imemo_parser_strterm = 10,
    imemo_callinfo       = 11,
    imemo_callcache      = 12,
    imemo_constcache     = 13,
};

/* CREF (Class REFerence) is defined in method.h */

/*! SVAR (Special VARiable) */
struct vm_svar {
    VALUE flags;
    const VALUE cref_or_me; /*!< class reference or rb_method_entry_t */
    const VALUE lastline;
    const VALUE backref;
    const VALUE others;
};

/*! THROW_DATA */
struct vm_throw_data {
    uintptr_t flags;
    VALUE reserved;
    const VALUE throw_obj;
    const struct rb_control_frame_struct *catch_frame;
    int throw_state;
};

#define THROW_DATA_CONSUMED IMEMO_FL_USER0

/* IFUNC (Internal FUNCtion) */

struct vm_ifunc_argc {
#if SIZEOF_INT * 2 > SIZEOF_VALUE
    signed int min: (SIZEOF_VALUE * CHAR_BIT) / 2;
    signed int max: (SIZEOF_VALUE * CHAR_BIT) / 2;
#else
    int min, max;
#endif
};

/*! IFUNC (Internal FUNCtion) */
struct vm_ifunc {
    VALUE flags;
    VALUE *svar_lep;
    rb_block_call_func_t func;
    const void *data;
    struct vm_ifunc_argc argc;
};

struct rb_imemo_tmpbuf_struct {
    VALUE flags;
    VALUE reserved;
    VALUE *ptr; /* malloc'ed buffer */
    struct rb_imemo_tmpbuf_struct *next; /* next imemo */
    size_t cnt; /* buffer size in VALUE */
};

/*! MEMO
 *
 * @see imemo_type
 * */
struct MEMO {
    uintptr_t flags;
    VALUE reserved;
    const VALUE v1;
    const VALUE v2;
    union {
        long cnt;
        long state;
        const VALUE value;
        void (*func)(void);
    } u3;
};

/* ment is in method.h */

#define THROW_DATA_P(err) imemo_throw_data_p((VALUE)err)
#define MEMO_CAST(m) ((struct MEMO *)(m))
#define MEMO_NEW(a, b, c) ((struct MEMO *)rb_imemo_new(imemo_memo, (VALUE)(a), (VALUE)(b), (VALUE)(c), 0))
#define MEMO_FOR(type, value) ((type *)RARRAY_PTR(value))
#define NEW_MEMO_FOR(type, value) \
  ((value) = rb_ary_hidden_new_fill(type_roomof(type, VALUE)), MEMO_FOR(type, value))
#define NEW_PARTIAL_MEMO_FOR(type, value, member) \
  ((value) = rb_ary_hidden_new_fill(type_roomof(type, VALUE)), \
   rb_ary_set_len((value), offsetof(type, member) / sizeof(VALUE)), \
   MEMO_FOR(type, value))

typedef struct rb_imemo_tmpbuf_struct rb_imemo_tmpbuf_t;
rb_imemo_tmpbuf_t *rb_imemo_tmpbuf_parser_heap(void *buf, rb_imemo_tmpbuf_t *old_heap, size_t cnt);
struct vm_ifunc *rb_vm_ifunc_new(rb_block_call_func_t func, const void *data, int min_argc, int max_argc);
static inline enum imemo_type imemo_type(VALUE imemo);
static inline int imemo_type_p(VALUE imemo, enum imemo_type imemo_type);
static inline bool imemo_throw_data_p(VALUE imemo);
static inline struct vm_ifunc *rb_vm_ifunc_proc_new(rb_block_call_func_t func, const void *data);
static inline VALUE rb_imemo_tmpbuf_auto_free_pointer(void);
static inline void *RB_IMEMO_TMPBUF_PTR(VALUE v);
static inline void *rb_imemo_tmpbuf_set_ptr(VALUE v, void *ptr);
static inline VALUE rb_imemo_tmpbuf_auto_free_pointer_new_from_an_RString(VALUE str);
static inline void MEMO_V1_SET(struct MEMO *m, VALUE v);
static inline void MEMO_V2_SET(struct MEMO *m, VALUE v);

RUBY_SYMBOL_EXPORT_BEGIN
#if IMEMO_DEBUG
VALUE rb_imemo_new_debug(enum imemo_type type, VALUE v1, VALUE v2, VALUE v3, VALUE v0, const char *file, int line);
#define rb_imemo_new(type, v1, v2, v3, v0) rb_imemo_new_debug(type, v1, v2, v3, v0, __FILE__, __LINE__)
#else
VALUE rb_imemo_new(enum imemo_type type, VALUE v1, VALUE v2, VALUE v3, VALUE v0);
#endif
const char *rb_imemo_name(enum imemo_type type);
RUBY_SYMBOL_EXPORT_END

static inline enum imemo_type
imemo_type(VALUE imemo)
{
    return (RBASIC(imemo)->flags >> FL_USHIFT) & IMEMO_MASK;
}

static inline int
imemo_type_p(VALUE imemo, enum imemo_type imemo_type)
{
    if (LIKELY(!RB_SPECIAL_CONST_P(imemo))) {
        /* fixed at compile time if imemo_type is given. */
        const uintptr_t mask = (IMEMO_MASK << FL_USHIFT) | RUBY_T_MASK;
        const uintptr_t expected_type = (imemo_type << FL_USHIFT) | T_IMEMO;
        /* fixed at runtime. */
        return expected_type == (RBASIC(imemo)->flags & mask);
    }
    else {
        return 0;
    }
}

#define IMEMO_TYPE_P(v, t) imemo_type_p((VALUE)v, t)

static inline bool
imemo_throw_data_p(VALUE imemo)
{
    return RB_TYPE_P(imemo, T_IMEMO);
}

static inline struct vm_ifunc *
rb_vm_ifunc_proc_new(rb_block_call_func_t func, const void *data)
{
    return rb_vm_ifunc_new(func, data, 0, UNLIMITED_ARGUMENTS);
}

static inline VALUE
rb_imemo_tmpbuf_auto_free_pointer(void)
{
    return rb_imemo_new(imemo_tmpbuf, 0, 0, 0, 0);
}

static inline void *
RB_IMEMO_TMPBUF_PTR(VALUE v)
{
    const struct rb_imemo_tmpbuf_struct *p = (const void *)v;
    return p->ptr;
}

static inline void *
rb_imemo_tmpbuf_set_ptr(VALUE v, void *ptr)
{
    return ((rb_imemo_tmpbuf_t *)v)->ptr = ptr;
}

static inline VALUE
rb_imemo_tmpbuf_auto_free_pointer_new_from_an_RString(VALUE str)
{
    const void *src;
    VALUE imemo;
    rb_imemo_tmpbuf_t *tmpbuf;
    void *dst;
    size_t len;

    SafeStringValue(str);
    /* create tmpbuf to keep the pointer before xmalloc */
    imemo = rb_imemo_tmpbuf_auto_free_pointer();
    tmpbuf = (rb_imemo_tmpbuf_t *)imemo;
    len = RSTRING_LEN(str);
    src = RSTRING_PTR(str);
    dst = ruby_xmalloc(len);
    memcpy(dst, src, len);
    tmpbuf->ptr = dst;
    return imemo;
}

static inline void
MEMO_V1_SET(struct MEMO *m, VALUE v)
{
    RB_OBJ_WRITE(m, &m->v1, v);
}

static inline void
MEMO_V2_SET(struct MEMO *m, VALUE v)
{
    RB_OBJ_WRITE(m, &m->v2, v);
}

#endif /* INTERNAL_IMEMO_H */
{
    VM_ASSERT(THROW_DATA_P(obj));
    obj->throw_state = st;
}

static inline void
THROW_DATA_CONSUMED_SET(struct vm_throw_data *obj)
{
    if (THROW_DATA_P(obj) &&
        THROW_DATA_STATE(obj) == TAG_BREAK) {
        obj->flags |= THROW_DATA_CONSUMED;
    }
}

#define IS_ARGS_SPLAT(ci)          (vm_ci_flag(ci) & VM_CALL_ARGS_SPLAT)
#define IS_ARGS_KEYWORD(ci)        (vm_ci_flag(ci) & VM_CALL_KWARG)
#define IS_ARGS_KW_SPLAT(ci)       (vm_ci_flag(ci) & VM_CALL_KW_SPLAT)
#define IS_ARGS_KW_OR_KW_SPLAT(ci) (vm_ci_flag(ci) & (VM_CALL_KWARG | VM_CALL_KW_SPLAT))
#define IS_ARGS_KW_SPLAT_MUT(ci)   (vm_ci_flag(ci) & VM_CALL_KW_SPLAT_MUT)

static inline bool
vm_call_cacheable(const struct rb_callinfo *ci, const struct rb_callcache *cc)
{
    return (vm_ci_flag(ci) & VM_CALL_FCALL) ||
        METHOD_ENTRY_VISI(vm_cc_cme(cc)) != METHOD_VISI_PROTECTED;
}
/* If this returns true, an optimized function returned by `vm_call_iseq_setup_func`
   can be used as a fastpath. */
static inline bool
vm_call_iseq_optimizable_p(const struct rb_callinfo *ci, const struct rb_callcache *cc)
{
    return !IS_ARGS_SPLAT(ci) && !IS_ARGS_KEYWORD(ci) && vm_call_cacheable(ci, cc);
}

#endif /* RUBY_INSNHELPER_H */
