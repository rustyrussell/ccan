/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_GENERATOR_H
#define CCAN_GENERATOR_H
#include "config.h"

#if !HAVE_UCONTEXT
#error Generators require working ucontext.h functions
#endif

#if !HAVE_TYPEOF
#error Generators require typeof
#endif

#if !HAVE_STATEMENT_EXPR
#error Generators require statement expressions
#endif

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <ucontext.h>

#include <ccan/ptrint/ptrint.h>
#include <ccan/build_assert/build_assert.h>
#include <ccan/cppmagic/cppmagic.h>
#include <ccan/compiler/compiler.h>

/*
 * Internals - included just for the use of inlines and macros
 */

struct generator_ {
	ucontext_t gen;
	ucontext_t caller;
	bool complete;
	void *base;
};

static inline struct generator_ *generator_state_(const void *ret)
{
	return (struct generator_ *)ret - 1;
}

static inline void *generator_argp_(const void *ret)
{
	return generator_state_(ret)->base;
}

struct generator_incomplete_;

#define generator_rtype_(gen_)			\
	typeof((*(gen_))((struct generator_incomplete_ *)NULL))

#if HAVE_POINTER_SAFE_MAKECONTEXT
#define generator_wrapper_args_()	void *ret
#else
#define generator_wrapper_args_()	int lo, int hi
#endif
typedef void generator_wrapper_(generator_wrapper_args_());

void *generator_new_(generator_wrapper_ *fn, size_t retsize);
void generator_free_(void *ret);

/*
 * API
 */

/**
 * generator_t - type for an in-progress generator
 * @rtype: type of values the generator yield
 */
#define generator_t(rtype_)			\
	typeof(rtype_ (*)(struct generator_incomplete_ *))

/**
 * generator_declare - declare (but don't define) a generator function
 * @name: name for the generator
 * @rtype: return type for the generator
 *
 * Declares (as an extern) a generator function named @name, which
 * will yield return values of type @rtype.
 *
 * Example:
 *	generator_declare(count_to_3, int);
 */
#define generator_declare(name_, rtype_, ...)	\
	generator_t(rtype_) name_(generator_parms_outer_(__VA_ARGS__))

/**
 * generator_def - define a generator function
 * @name: name for the generator
 * @rtype: return type for the generator
 *
 * Define a generator function named @name yielding return values of
 * type @rtype.  The generator_def() line is followed immediately by a
 * block containing the generator's code.
 *
 * Example:
 *	generator_def(count_to_3, int)
 *	{
 *		generator_yield(1);
 *		generator_yield(2);
 *		generator_yield(3);
 *	}
 */
#define generator_parm_(t_, n_)			t_ n_
#define generator_parms_(...)						\
	CPPMAGIC_2MAP(generator_parm_, __VA_ARGS__)
#define generator_parms_inner_(...)					\
	CPPMAGIC_IFELSE(CPPMAGIC_NONEMPTY(__VA_ARGS__))			\
		(, generator_parms_(__VA_ARGS__))()
#define generator_parms_outer_(...)					\
	CPPMAGIC_IFELSE(CPPMAGIC_NONEMPTY(__VA_ARGS__))	\
		(generator_parms_(__VA_ARGS__))(void)
#define generator_argfield_(t_, n_)		t_ n_;
#define generator_argstruct_(...)					\
	struct {							\
		CPPMAGIC_JOIN(, CPPMAGIC_2MAP(generator_argfield_,	\
					      __VA_ARGS__))		\
	}
#define generator_arg_unpack_(t_, n_)		args->n_
#define generator_args_unpack_(...)		\
	CPPMAGIC_IFELSE(CPPMAGIC_NONEMPTY(__VA_ARGS__))			\
		(, CPPMAGIC_2MAP(generator_arg_unpack_, __VA_ARGS__))()
#define generator_arg_pack_(t_, n_)		args->n_ = n_
#define generator_args_pack_(...)					\
	CPPMAGIC_JOIN(;, CPPMAGIC_2MAP(generator_arg_pack_, __VA_ARGS__))
#define generator_def_(name_, rtype_, storage_, ...)			\
	static void name_##_generator_(rtype_ *ret_			\
				       generator_parms_inner_(__VA_ARGS__)); \
	static void name_##_generator__(generator_wrapper_args_())	\
	{								\
		struct generator_ *gen;					\
		UNNEEDED generator_argstruct_(__VA_ARGS__) *args;	\
		CPPMAGIC_IFELSE(HAVE_POINTER_SAFE_MAKECONTEXT)		\
			()						\
			(ptrdiff_t hilo = ((ptrdiff_t)hi << (8*sizeof(int))) \
			 	+ (ptrdiff_t)lo;			\
			rtype_ *ret = (rtype_ *)int2ptr(hilo);		\
			BUILD_ASSERT(sizeof(struct generator_ *)	\
				     <= 2*sizeof(int));)		\
		gen = generator_state_(ret);				\
		args = generator_argp_(ret);				\
		name_##_generator_(ret generator_args_unpack_(__VA_ARGS__)); \
		gen->complete = true;					\
		setcontext(&gen->caller);				\
		assert(0);						\
	}								\
	storage_ generator_t(rtype_)					\
	name_(generator_parms_outer_(__VA_ARGS__))			\
	{								\
		generator_t(rtype_) gen = generator_new_(name_##_generator__, \
							 sizeof(rtype_)); \
		UNNEEDED generator_argstruct_(__VA_ARGS__) *args =	\
			generator_argp_(gen);				\
		generator_args_pack_(__VA_ARGS__);			\
		return gen;						\
	}								\
	static void name_##_generator_(rtype_ *ret_			\
				       generator_parms_inner_(__VA_ARGS__))
#define generator_def(name_, rtype_, ...)	\
	generator_def_(name_, rtype_, , __VA_ARGS__)

/**
 * generator_def_static - define a private / local generator function
 * @name: name for the generator
 * @rtype: return type for the generator
 *
 * As generator_def, but the resulting generator function will be
 * local to this module.
 */
#define generator_def_static(name_, rtype_, ...)	\
	generator_def_(name_, rtype_, static, __VA_ARGS__)

/**
 * generator_yield - yield (return) a value from a generator
 * @val: value to yield
 *
 * Invoke only from within a generator.  Yield the given value to the
 * caller.  This will stop execution of the generator code until the
 * caller next invokes generator_next(), at which point it will
 * continue from the generator_yield statement.
 */
#define generator_yield(val_)						\
	do {								\
		struct generator_ *gen_ = generator_state_(ret_);	\
		int rc;							\
		*(ret_) = (val_);					\
		rc = swapcontext(&gen_->gen, &gen_->caller);		\
		assert(rc == 0);					\
	} while (0)

/**
 * generator_next - get next value from a generator
 * @gen: a generator state variable
 *
 * Returns a pointer to a (correctly typed) buffer containing the next
 * value yielded by @gen, or NULL if @gen is finished.  The buffer
 * contents is only valid until the next time @gen is called or
 * manipulated.
 */
static inline void *generator_next_(void *ret_)
{
	struct generator_ *gen = generator_state_(ret_);
	int rc;

	if (gen->complete)
		return NULL;

	rc = swapcontext(&gen->caller, &gen->gen);
	assert(rc == 0);

	return gen->complete ? NULL : ret_;
}
#define generator_next(gen_)				\
	((generator_rtype_(gen_) *)generator_next_(gen_))

/**
 * generator_next_val - store next value from a generator
 * @val: a variable of type suitable to store the generator's return
 *       type (lvalue)
 * @gen: a generator state variable
 *
 * Returns 'true' if @gen yielded a new value, false if @gen is
 * complete.  If a new value was yielded, it is stored in @val.
 */
#define generator_next_val(val_, gen_)			\
	({						\
		generator_rtype_(gen_) *ret;		\
		ret = generator_next(gen_);		\
		if (ret)				\
			(val_) = *ret;			\
		!!ret;					\
	})

#define generator_free(gen_)					\
	generator_free_((generator_rtype_(gen_) *)(gen_))

#endif /* CCAN_GENERATOR_H */
