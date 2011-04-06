#ifndef CCAN_TALLOC_H
#define CCAN_TALLOC_H
/* 
   Copyright (C) Andrew Tridgell 2004-2005
   Copyright (C) Stefan Metzmacher 2006
   
     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/compiler/compiler.h>
#include "config.h"

/*
  this uses a little trick to allow __LINE__ to be stringified
*/
#ifndef __location__
#define __TALLOC_STRING_LINE1__(s)    #s
#define __TALLOC_STRING_LINE2__(s)   __TALLOC_STRING_LINE1__(s)
#define __TALLOC_STRING_LINE3__  __TALLOC_STRING_LINE2__(__LINE__)
#define __location__ __FILE__ ":" __TALLOC_STRING_LINE3__
#endif

/* try to make talloc_set_destructor() and talloc_steal() type safe,
   if we have a recent gcc */
#if HAVE_TYPEOF
#define _TALLOC_TYPEOF(ptr) __typeof__(ptr)
#else
#define _TALLOC_TYPEOF(ptr) void *
#endif

#define talloc_move(ctx, ptr) (_TALLOC_TYPEOF(*(ptr)))_talloc_move((ctx),(void *)(ptr))

/**
 * talloc - allocate dynamic memory for a type
 * @ctx: context to be parent of this allocation, or NULL.
 * @type: the type to be allocated.
 *
 * The talloc() macro is the core of the talloc library. It takes a memory
 * context and a type, and returns a pointer to a new area of memory of the
 * given type.
 *
 * The returned pointer is itself a talloc context, so you can use it as the
 * context argument to more calls to talloc if you wish.
 *
 * The returned pointer is a "child" of @ctx. This means that if you
 * talloc_free() @ctx then the new child disappears as well.  Alternatively you
 * can free just the child.
 *
 * @ctx can be NULL, in which case a new top level context is created.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(a, unsigned int);
 *
 * See Also:
 *	talloc_zero, talloc_array, talloc_steal, talloc_free.
 */
#define talloc(ctx, type) (type *)talloc_named_const(ctx, sizeof(type), #type)

/**
 * TALLOC_CTX - indicate that a pointer is used as a talloc parent.
 *
 * As talloc is a hierarchial memory allocator, every talloc chunk is a
 * potential parent to other talloc chunks. So defining a separate type for a
 * talloc chunk is not strictly necessary. TALLOC_CTX is defined nevertheless,
 * as it provides an indicator for function arguments.
 *
 * Example:
 *	struct foo {
 *		int val;
 *	};
 *
 *	static struct foo *foo_new(TALLOC_CTX *mem_ctx)
 *	{
 *		struct foo *foo = talloc(mem_ctx, struct foo);
 *		if (foo)
 *			foo->val = 0;
 *	return foo;
 *	}
 */
typedef void TALLOC_CTX;

/**
 * talloc_set - allocate dynamic memory for a type, into a pointer
 * @ptr: pointer to the pointer to assign.
 * @ctx: context to be parent of this allocation, or NULL.
 *
 * talloc_set() does a talloc, but also adds a destructor which will make the
 * pointer invalid when it is freed.  This can find many use-after-free bugs.
 *
 * Note that the destructor is chained off a zero-length allocation, and so
 * is not affected by talloc_set_destructor().
 *
 * Example:
 *	unsigned int *b, *a;
 *	a = talloc(NULL, unsigned int);
 *	talloc_set(&b, a);
 *	talloc_free(a);
 *	*b = 1; // This will crash!
 *
 * See Also:
 *	talloc.
 */
#define talloc_set(pptr, ctx) \
	_talloc_set((pptr), (ctx), sizeof(&**(pptr)), __location__)

/**
 * talloc_free - free talloc'ed memory and its children
 * @ptr: the talloced pointer to free
 *
 * The talloc_free() function frees a piece of talloc memory, and all its
 * children. You can call talloc_free() on any pointer returned by talloc().
 *
 * The return value of talloc_free() indicates success or failure, with 0
 * returned for success and -1 for failure. The only possible failure condition
 * is if the pointer had a destructor attached to it and the destructor
 * returned -1. See talloc_set_destructor() for details on destructors.
 * errno will be preserved unless the talloc_free fails.
 *
 * If this pointer has an additional parent when talloc_free() is called then
 * the memory is not actually released, but instead the most recently
 * established parent is destroyed. See talloc_reference() for details on
 * establishing additional parents.
 *
 * For more control on which parent is removed, see talloc_unlink().
 *
 * talloc_free() operates recursively on its children.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(a, unsigned int);
 *	// Frees a and b
 *	talloc_free(a);
 *
 * See Also:
 *	talloc_set_destructor, talloc_unlink
 */
int talloc_free(const void *ptr);

/**
 * talloc_set_destructor - set a destructor for when this pointer is freed
 * @ptr: the talloc pointer to set the destructor on
 * @destructor: the function to be called
 *
 * The function talloc_set_destructor() sets the "destructor" for the pointer
 * @ptr.  A destructor is a function that is called when the memory used by a
 * pointer is about to be released.  The destructor receives the pointer as an
 * argument, and should return 0 for success and -1 for failure.
 *
 * The destructor can do anything it wants to, including freeing other pieces
 * of memory. A common use for destructors is to clean up operating system
 * resources (such as open file descriptors) contained in the structure the
 * destructor is placed on.
 *
 * You can only place one destructor on a pointer. If you need more than one
 * destructor then you can create a zero-length child of the pointer and place
 * an additional destructor on that.
 *
 * To remove a destructor call talloc_set_destructor() with NULL for the
 * destructor.
 *
 * If your destructor attempts to talloc_free() the pointer that it is the
 * destructor for then talloc_free() will return -1 and the free will be
 * ignored. This would be a pointless operation anyway, as the destructor is
 * only called when the memory is just about to go away.
 *
 * Example:
 * static int destroy_fd(int *fd)
 * {
 *	close(*fd);
 *	return 0;
 * }
 *
 * static int *open_file(const char *filename)
 * {
 *	int *fd = talloc(NULL, int);
 *	*fd = open(filename, O_RDONLY);
 *	if (*fd < 0) {
 *		talloc_free(fd);
 *		return NULL;
 *	}
 *	// Whenever they free this, we close the file.
 *	talloc_set_destructor(fd, destroy_fd);
 *	return fd;
 * }
 *
 * See Also:
 *	talloc, talloc_free
 */
#define talloc_set_destructor(ptr, function)				      \
	_talloc_set_destructor((ptr), typesafe_cb(int, void *, (function), (ptr)))

/**
 * talloc_zero - allocate zeroed dynamic memory for a type
 * @ctx: context to be parent of this allocation, or NULL.
 * @type: the type to be allocated.
 *
 * The talloc_zero() macro is equivalent to:
 *
 *  ptr = talloc(ctx, type);
 *  if (ptr) memset(ptr, 0, sizeof(type));
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc_zero(NULL, unsigned int);
 *	b = talloc_zero(a, unsigned int);
 *
 * See Also:
 *	talloc, talloc_zero_size, talloc_zero_array
 */
#define talloc_zero(ctx, type) (type *)_talloc_zero(ctx, sizeof(type), #type)

/**
 * talloc_array - allocate dynamic memory for an array of a given type
 * @ctx: context to be parent of this allocation, or NULL.
 * @type: the type to be allocated.
 * @count: the number of elements to be allocated.
 *
 * The talloc_array() macro is a safe way of allocating an array.  It is
 * equivalent to:
 *
 *  (type *)talloc_size(ctx, sizeof(type) * count);
 *
 * except that it provides integer overflow protection for the multiply,
 * returning NULL if the multiply overflows.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc_zero(NULL, unsigned int);
 *	b = talloc_array(a, unsigned int, 100);
 *
 * See Also:
 *	talloc, talloc_zero_array, talloc_array_length
 */
#define talloc_array(ctx, type, count) (type *)_talloc_array(ctx, sizeof(type), count, #type)

/**
 * talloc_size - allocate a particular size of memory
 * @ctx: context to be parent of this allocation, or NULL.
 * @size: the number of bytes to allocate
 *
 * The function talloc_size() should be used when you don't have a convenient
 * type to pass to talloc(). Unlike talloc(), it is not type safe (as it
 * returns a void *), so you are on your own for type checking.
 *
 * Best to use talloc() or talloc_array() instead.
 *
 * Example:
 *	void *mem = talloc_size(NULL, 100);
 *	memset(mem, 0xFF, 100);
 *
 * See Also:
 *	talloc, talloc_array, talloc_zero_size
 */
#define talloc_size(ctx, size) talloc_named_const(ctx, size, __location__)

#if HAVE_TYPEOF
/**
 * talloc_steal - change/set the parent context of a talloc pointer
 * @ctx: the new parent
 * @ptr: the talloc pointer to reparent
 *
 * The talloc_steal() function changes the parent context of a talloc
 * pointer. It is typically used when the context that the pointer is currently
 * a child of is going to be freed and you wish to keep the memory for a longer
 * time.
 *
 * The talloc_steal() function returns the pointer that you pass it. It does
 * not have any failure modes.
 *
 * NOTE: It is possible to produce loops in the parent/child relationship if
 * you are not careful with talloc_steal(). No guarantees are provided as to
 * your sanity or the safety of your data if you do this.
 *
 * talloc_steal (new_ctx, NULL) will return NULL with no sideeffects.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(NULL, unsigned int);
 *	// Reparent b to a as if we'd done 'b = talloc(a, unsigned int)'.
 *	talloc_steal(a, b);
 *
 * See Also:
 *	talloc_reference
 */
#define talloc_steal(ctx, ptr) ({ _TALLOC_TYPEOF(ptr) _talloc_steal_ret = (_TALLOC_TYPEOF(ptr))_talloc_steal((ctx),(ptr)); _talloc_steal_ret; }) /* this extremely strange macro is to avoid some braindamaged warning stupidity in gcc 4.1.x */
#else
#define talloc_steal(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_steal((ctx),(ptr))
#endif /* HAVE_TYPEOF */

/**
 * talloc_report_full - report all the memory used by a pointer and children.
 * @ptr: the context to report on
 * @f: the file to report to
 *
 * Recursively print the entire tree of memory referenced by the
 * pointer. References in the tree are shown by giving the name of the pointer
 * that is referenced.
 *
 * You can pass NULL for the pointer, in which case a report is printed for the
 * top level memory context, but only if talloc_enable_null_tracking() has been
 * called.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(a, unsigned int);
 *	fprintf(stderr, "Dumping memory tree for a:\n");
 *	talloc_report_full(a, stderr);
 *
 * See Also:
 *	talloc_report
 */
void talloc_report_full(const void *ptr, FILE *f);

/**
 * talloc_reference - add an additional parent to a context
 * @ctx: the additional parent
 * @ptr: the talloc pointer
 *
 * The talloc_reference() function makes @ctx an additional parent of @ptr.
 *
 * The return value of talloc_reference() is always the original pointer @ptr,
 * unless talloc ran out of memory in creating the reference in which case it
 * will return NULL (each additional reference consumes around 48 bytes of
 * memory on intel x86 platforms).
 *
 * If @ptr is NULL, then the function is a no-op, and simply returns NULL.
 *
 * After creating a reference you can free it in one of the following ways:
 *
 *  - you can talloc_free() any parent of the original pointer. That will
 *    reduce the number of parents of this pointer by 1, and will cause this
 *    pointer to be freed if it runs out of parents.
 *
 *  - you can talloc_free() the pointer itself. That will destroy the most
 *    recently established parent to the pointer and leave the pointer as a
 *    child of its current parent.
 *
 * For more control on which parent to remove, see talloc_unlink().
 * Example:
 *	unsigned int *a, *b, *c;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(NULL, unsigned int);
 *	c = talloc(a, unsigned int);
 *	// b also serves as a parent of c (don't care about errors)
 *	(void)talloc_reference(b, c);
 */
#define talloc_reference(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_reference((ctx),(ptr))

/**
 * talloc_unlink - remove a specific parent from a talloc pointer.
 * @context: the parent to remove
 * @ptr: the talloc pointer
 *
 * The talloc_unlink() function removes a specific parent from @ptr. The
 * context passed must either be a context used in talloc_reference() with this
 * pointer, or must be a direct parent of @ptr.
 *
 * Note that if the parent has already been removed using talloc_free() then
 * this function will fail and will return -1.  Likewise, if @ptr is NULL,
 * then the function will make no modifications and return -1.
 *
 * Usually you can just use talloc_free() instead of talloc_unlink(), but
 * sometimes it is useful to have the additional control on which parent is
 * removed.
 *
 * Example:
 *	unsigned int *a, *b, *c;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(NULL, unsigned int);
 *	c = talloc(a, unsigned int);
 *	// b also serves as a parent of c.
 *	(void)talloc_reference(b, c);
 *	talloc_unlink(b, c);
 */
int talloc_unlink(const void *context, void *ptr);

/**
 * talloc_report - print a summary of memory used by a pointer
 *
 * The talloc_report() function prints a summary report of all memory
 * used by @ptr.  One line of report is printed for each immediate child of
 * @ptr, showing the total memory and number of blocks used by that child.
 *
 * You can pass NULL for the pointer, in which case a report is printed for the
 * top level memory context, but only if talloc_enable_null_tracking() has been
 * called.
 *
 * Example:
 *	unsigned int *a, *b;
 *	a = talloc(NULL, unsigned int);
 *	b = talloc(a, unsigned int);
 *	fprintf(stderr, "Summary of memory tree for a:\n");
 *	talloc_report(a, stderr);
 *
 * See Also:
 *	talloc_report_full
 */
void talloc_report(const void *ptr, FILE *f);

/**
 * talloc_ptrtype - allocate a size of memory suitable for this pointer
 * @ctx: context to be parent of this allocation, or NULL.
 * @ptr: the pointer whose type we are to allocate
 *
 * The talloc_ptrtype() macro should be used when you have a pointer and
 * want to allocate memory to point at with this pointer. When compiling
 * with gcc >= 3 it is typesafe. Note this is a wrapper of talloc_size()
 * and talloc_get_name() will return the current location in the source file.
 * and not the type.
 *
 * Example:
 *	unsigned int *a = talloc_ptrtype(NULL, a);
 */
#define talloc_ptrtype(ctx, ptr) (_TALLOC_TYPEOF(ptr))talloc_size(ctx, sizeof(*(ptr)))

/**
 * talloc_new - create a new context
 * @ctx: the context to use as a parent.
 *
 * This is a utility macro that creates a new memory context hanging off an
 * exiting context, automatically naming it "talloc_new: __location__" where
 * __location__ is the source line it is called from. It is particularly useful
 * for creating a new temporary working context.
 */
#define talloc_new(ctx) talloc_named_const(ctx, 0, "talloc_new: " __location__)

/**
 * talloc_zero_size -  allocate a particular size of zeroed memory
 *
 * The talloc_zero_size() function is useful when you don't have a known type.
 */
#define talloc_zero_size(ctx, size) _talloc_zero(ctx, size, __location__)

/**
 * talloc_zero_array -  allocate an array of zeroed types
 * @ctx: context to be parent of this allocation, or NULL.
 * @type: the type to be allocated.
 * @count: the number of elements to be allocated.
 *
 * Just like talloc_array, but zeroes the memory.
 */
#define talloc_zero_array(ctx, type, count) (type *)_talloc_zero_array(ctx, sizeof(type), count, #type)

/**
 * talloc_array_size - allocate an array of elements of the given size
 * @ctx: context to be parent of this allocation, or NULL.
 * @size: the size of each element
 * @count: the number of elements to be allocated.
 *
 * Typeless form of talloc_array.
 */
#define talloc_array_size(ctx, size, count) _talloc_array(ctx, size, count, __location__)

/**
 * talloc_array_ptrtype - allocate an array of memory suitable for this pointer
 * @ctx: context to be parent of this allocation, or NULL.
 * @ptr: the pointer whose type we are to allocate
 * @count: the number of elements for the array
 *
 * Like talloc_ptrtype(), except it allocates an array.
 */
#define talloc_array_ptrtype(ctx, ptr, count) (_TALLOC_TYPEOF(ptr))talloc_array_size(ctx, sizeof(*(ptr)), count)

/**
 * talloc_realloc - resize a talloc array
 * @ctx: the parent to assign (if p is NULL)
 * @p: the memory to reallocate
 * @type: the type of the object to allocate
 * @count: the number of objects to reallocate
 *
 * The talloc_realloc() macro changes the size of a talloc pointer. The "count"
 * argument is the number of elements of type "type" that you want the
 * resulting pointer to hold.
 *
 * talloc_realloc() has the following equivalences:
 *
 *  talloc_realloc(context, NULL, type, 1) ==> talloc(context, type);
 *  talloc_realloc(context, NULL, type, N) ==> talloc_array(context, type, N);
 *  talloc_realloc(context, ptr, type, 0)  ==> talloc_free(ptr);
 *
 * The "context" argument is only used if "ptr" is NULL, otherwise it is
 * ignored.
 *
 * talloc_realloc() returns the new pointer, or NULL on failure. The call will
 * fail either due to a lack of memory, or because the pointer has more than
 * one parent (see talloc_reference()).
 */
#define talloc_realloc(ctx, p, type, count) (type *)_talloc_realloc_array(ctx, p, sizeof(type), count, #type)

/**
 * talloc_realloc_size - resize talloc memory
 * @ctx: the parent to assign (if p is NULL)
 * @ptr: the memory to reallocate
 * @size: the new size of memory.
 *
 * The talloc_realloc_size() function is useful when the type is not known so
 * the typesafe talloc_realloc() cannot be used.
 */
#define talloc_realloc_size(ctx, ptr, size) _talloc_realloc(ctx, ptr, size, __location__)

/**
 * talloc_strdup - duplicate a string
 * @ctx: the talloc context for the new string
 * @p: the string to copy
 *
 * The talloc_strdup() function is equivalent to:
 *
 *  ptr = talloc_size(ctx, strlen(p)+1);
 *  if (ptr) memcpy(ptr, p, strlen(p)+1);
 *
 * This functions sets the name of the new pointer to the passed string. This
 * is equivalent to:
 *
 *  talloc_set_name_const(ptr, ptr)
 */
char *talloc_strdup(const void *t, const char *p);

/**
 * talloc_strndup - duplicate a limited length of a string
 * @ctx: the talloc context for the new string
 * @p: the string to copy
 * @n: the maximum length of the returned string.
 *
 * The talloc_strndup() function is the talloc equivalent of the C library
 * function strndup(): the result will be truncated to @n characters before
 * the nul terminator.
 *
 * This functions sets the name of the new pointer to the passed string. This
 * is equivalent to:
 *
 *   talloc_set_name_const(ptr, ptr)
 */
char *talloc_strndup(const void *t, const char *p, size_t n);

/**
 * talloc_memdup - duplicate some talloc memory
 *
 * The talloc_memdup() function is equivalent to:
 *
 *  ptr = talloc_size(ctx, size);
 *  if (ptr) memcpy(ptr, p, size);
 */
#define talloc_memdup(t, p, size) _talloc_memdup(t, p, size, __location__)

/**
 * talloc_asprintf - sprintf into a talloc buffer.
 * @t: The context to allocate the buffer from
 * @fmt: printf-style format for the buffer.
 *
 * The talloc_asprintf() function is the talloc equivalent of the C library
 * function asprintf().
 *
 * This functions sets the name of the new pointer to the new string. This is
 * equivalent to:
 *
 *   talloc_set_name_const(ptr, ptr)
 */
char *talloc_asprintf(const void *t, const char *fmt, ...) PRINTF_FMT(2,3);

/**
 * talloc_append_string - concatenate onto a tallocated string 
 * @orig: the tallocated string to append to
 * @append: the string to add, or NULL to add nothing.
 *
 * The talloc_append_string() function appends the given formatted string to
 * the given string.
 *
 * This function sets the name of the new pointer to the new string. This is
 * equivalent to:
 *
 *    talloc_set_name_const(ptr, ptr)
 */
char *WARN_UNUSED_RESULT talloc_append_string(char *orig, const char *append);

/**
 * talloc_asprintf_append - sprintf onto the end of a talloc buffer.
 * @s: The tallocated string buffer
 * @fmt: printf-style format to append to the buffer.
 *
 * The talloc_asprintf_append() function appends the given formatted string to
 * the given string.
 *
 * This functions sets the name of the new pointer to the new string. This is
 * equivalent to:
 *   talloc_set_name_const(ptr, ptr)
 */
char *WARN_UNUSED_RESULT talloc_asprintf_append(char *s, const char *fmt, ...)
	PRINTF_FMT(2,3);

/**
 * talloc_vasprintf - vsprintf into a talloc buffer.
 * @t: The context to allocate the buffer from
 * @fmt: printf-style format for the buffer
 * @ap: va_list arguments
 *
 * The talloc_vasprintf() function is the talloc equivalent of the C library
 * function vasprintf()
 *
 * This functions sets the name of the new pointer to the new string. This is
 * equivalent to:
 *
 *   talloc_set_name_const(ptr, ptr)
 */
char *talloc_vasprintf(const void *t, const char *fmt, va_list ap)
	PRINTF_FMT(2,0);

/**
 * talloc_vasprintf_append - sprintf onto the end of a talloc buffer.
 * @t: The context to allocate the buffer from
 * @fmt: printf-style format for the buffer
 * @ap: va_list arguments
 *
 * The talloc_vasprintf_append() function is equivalent to
 * talloc_asprintf_append(), except it takes a va_list.
 */
char *WARN_UNUSED_RESULT talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
	PRINTF_FMT(2,0);

/**
 * talloc_set_type - force the name of a pointer to a particular type
 * @ptr: the talloc pointer
 * @type: the type whose name to set the ptr name to.
 *
 * This macro allows you to force the name of a pointer to be a particular
 * type. This can be used in conjunction with talloc_get_type() to do type
 * checking on void* pointers.
 *
 * It is equivalent to this:
 *   talloc_set_name_const(ptr, #type)
 */
#define talloc_set_type(ptr, type) talloc_set_name_const(ptr, #type)

/**
 * talloc_get_type - convert a talloced pointer with typechecking
 * @ptr: the talloc pointer
 * @type: the type which we expect the talloced pointer to be.
 *
 * This macro allows you to do type checking on talloc pointers. It is
 * particularly useful for void* private pointers. It is equivalent to this:
 *
 *   (type *)talloc_check_name(ptr, #type)
 */
#define talloc_get_type(ptr, type) (type *)talloc_check_name(ptr, #type)

/**
 * talloc_find_parent_byname - find a talloc parent by type
 * @ptr: the talloc pointer
 * @type: the type we're looking for
 *
 * Find a parent memory context of the current context that has the given
 * name. This can be very useful in complex programs where it may be difficult
 * to pass all information down to the level you need, but you know the
 * structure you want is a parent of another context.
 */
#define talloc_find_parent_bytype(ptr, type) (type *)talloc_find_parent_byname(ptr, #type)

/**
 * talloc_increase_ref_count - hold a reference to a talloc pointer
 * @ptr: the talloc pointer
 *
 * The talloc_increase_ref_count(ptr) function is exactly equivalent to:
 *
 *  talloc_reference(NULL, ptr);
 *
 * You can use either syntax, depending on which you think is clearer in your
 * code.
 *
 * It returns 0 on success and -1 on failure.
 */
int talloc_increase_ref_count(const void *ptr);

/**
 * talloc_set_name - set the name for a talloc pointer
 * @ptr: the talloc pointer
 * @fmt: the printf-style format string for the name
 *
 * Each talloc pointer has a "name". The name is used principally for debugging
 * purposes, although it is also possible to set and get the name on a pointer
 * in as a way of "marking" pointers in your code.
 *
 * The main use for names on pointer is for "talloc reports". See
 * talloc_report() and talloc_report_full() for details. Also see
 * talloc_enable_leak_report() and talloc_enable_leak_report_full().
 *
 * The talloc_set_name() function allocates memory as a child of the
 * pointer. It is logically equivalent to:
 *   talloc_set_name_const(ptr, talloc_asprintf(ptr, fmt, ...));
 *
 * Note that multiple calls to talloc_set_name() will allocate more memory
 * without releasing the name. All of the memory is released when the ptr is
 * freed using talloc_free().
 */
const char *talloc_set_name(const void *ptr, const char *fmt, ...)
	PRINTF_FMT(2,3);

/**
 * talloc_set_name_const - set a talloc pointer name to a string constant
 * @ptr: the talloc pointer to name
 * @name: the strucng constant.
 *
 * The function talloc_set_name_const() is just like talloc_set_name(), but it
 * takes a string constant, and is much faster. It is extensively used by the
 * "auto naming" macros, such as talloc().
 *
 * This function does not allocate any memory. It just copies the supplied
 * pointer into the internal representation of the talloc ptr. This means you
 * must not pass a name pointer to memory that will disappear before the ptr is
 * freed with talloc_free().
 */
void talloc_set_name_const(const void *ptr, const char *name);

/**
 * talloc_named - create a specifically-named talloc pointer
 * @context: the parent context for the allocation
 * @size: the size to allocate
 * @fmt: the printf-style format for the name
 *
 * The talloc_named() function creates a named talloc pointer. It is equivalent
 * to:
 *
 *   ptr = talloc_size(context, size);
 *   talloc_set_name(ptr, fmt, ....);
 */
void *talloc_named(const void *context, size_t size, 
		   const char *fmt, ...) PRINTF_FMT(3,4);

/**
 * talloc_named_const - create a specifically-named talloc pointer
 * @context: the parent context for the allocation
 * @size: the size to allocate
 * @name: the string constant to use as the name
 *
 * This is equivalent to:
 *
 *   ptr = talloc_size(context, size);
 *   talloc_set_name_const(ptr, name);
 */
void *talloc_named_const(const void *context, size_t size, const char *name);

/**
 * talloc_get_name - get the name of a talloc pointer
 * @ptr: the talloc pointer
 *
 * This returns the current name for the given talloc pointer. See
 * talloc_set_name() for details.
 */
const char *talloc_get_name(const void *ptr);

/**
 * talloc_check_name - check if a pointer has the specified name
 * @ptr: the talloc pointer
 * @name: the name to compare with the pointer's name
 *
 * This function checks if a pointer has the specified name. If it does then
 * the pointer is returned. It it doesn't then NULL is returned.
 */
void *talloc_check_name(const void *ptr, const char *name);

/**
 * talloc_init - create a top-level context of particular name
 * @fmt: the printf-style format of the name
 *
 * This function creates a zero length named talloc context as a top level
 * context. It is equivalent to:
 *
 *   talloc_named(NULL, 0, fmt, ...);
 */
void *talloc_init(const char *fmt, ...) PRINTF_FMT(1,2);

/**
 * talloc_total_size - get the bytes used by the pointer and its children
 * @ptr: the talloc pointer
 *
 * The talloc_total_size() function returns the total size in bytes used by
 * this pointer and all child pointers. Mostly useful for debugging.
 *
 * Passing NULL is allowed, but it will only give a meaningful result if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full() has been
 * called.
 */
size_t talloc_total_size(const void *ptr);

/**
 * talloc_total_blocks - get the number of allocations for the pointer
 * @ptr: the talloc pointer
 *
 * The talloc_total_blocks() function returns the total allocations used by
 * this pointer and all child pointers. Mostly useful for debugging. For
 * example, a pointer with no children will return "1".
 *
 * Passing NULL is allowed, but it will only give a meaningful result if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full() has been
 * called.
 */
size_t talloc_total_blocks(const void *ptr);

/**
 * talloc_report_depth_cb - walk the entire talloc tree under a talloc pointer
 * @ptr: the talloc pointer to recurse under
 * @depth: the current depth of traversal
 * @max_depth: maximum depth to traverse, or -1 for no maximum
 * @callback: the function to call on each pointer
 * @private_data: pointer to hand to @callback.
 *
 * This provides a more flexible reports than talloc_report(). It will
 * recursively call the callback for the entire tree of memory referenced by
 * the pointer. References in the tree are passed with is_ref = 1 and the
 * pointer that is referenced.
 *
 * You can pass NULL for the pointer, in which case a report is printed for the
 * top level memory context, but only if talloc_enable_leak_report() or
 * talloc_enable_leak_report_full() has been called.
 *
 * The recursion is stopped when depth >= max_depth.  max_depth = -1 means only
 * stop at leaf nodes.
 */
void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
			  		     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data);

/**
 * talloc_report_depth_file - report talloc usage to a maximum depth
 * @ptr: the talloc pointer to recurse under
 * @depth: the current depth of traversal
 * @max_depth: maximum depth to traverse, or -1 for no maximum
 * @f: the file to report to
 *
 * This provides a more flexible reports than talloc_report(). It will let you
 * specify the depth and max_depth.
 */
void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f);

/**
 * talloc_enable_null_tracking - enable tracking of top-level tallocs
 *
 * This enables tracking of the NULL memory context without enabling leak
 * reporting on exit. Useful for when you want to do your own leak reporting
 * call via talloc_report_null_full();
 */
void talloc_enable_null_tracking(void);

/**
 * talloc_disable_null_tracking - enable tracking of top-level tallocs
 *
 * This disables tracking of the NULL memory context.
 */
void talloc_disable_null_tracking(void);

/**
 * talloc_enable_leak_report - call talloc_report on program exit
 *
 * This enables calling of talloc_report(NULL, stderr) when the program
 * exits. In Samba4 this is enabled by using the --leak-report command line
 * option.
 *
 * For it to be useful, this function must be called before any other talloc
 * function as it establishes a "null context" that acts as the top of the
 * tree. If you don't call this function first then passing NULL to
 * talloc_report() or talloc_report_full() won't give you the full tree
 * printout.
 *
 * Here is a typical talloc report:
 *
 * talloc report on 'null_context' (total 267 bytes in 15 blocks)
 *         libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *         libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *         iconv(UTF8,CP850)              contains     42 bytes in   2 blocks
 *         libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *         iconv(CP850,UTF8)              contains     42 bytes in   2 blocks
 *         iconv(UTF8,UTF-16LE)           contains     45 bytes in   2 blocks
 *         iconv(UTF-16LE,UTF8)           contains     45 bytes in   2 blocks
 */
void talloc_enable_leak_report(void);

/**
 * talloc_enable_leak_report - call talloc_report_full on program exit
 *
 * This enables calling of talloc_report_full(NULL, stderr) when the program
 * exits. In Samba4 this is enabled by using the --leak-report-full command
 * line option.
 *
 * For it to be useful, this function must be called before any other talloc
 * function as it establishes a "null context" that acts as the top of the
 * tree. If you don't call this function first then passing NULL to
 * talloc_report() or talloc_report_full() won't give you the full tree
 * printout.
 *
 * Here is a typical full report:
 *
 * full talloc report on 'root' (total 18 bytes in 8 blocks)
 *    p1                        contains     18 bytes in   7 blocks (ref 0)
 *         r1                        contains     13 bytes in   2 blocks (ref 0)
 *             reference to: p2
 *         p2                        contains      1 bytes in   1 blocks (ref 1)
 *         x3                        contains      1 bytes in   1 blocks (ref 0)
 *         x2                        contains      1 bytes in   1 blocks (ref 0)
 *         x1                        contains      1 bytes in   1 blocks (ref 0)
 */
void talloc_enable_leak_report_full(void);

/**
 * talloc_autofree_context - a context which will be freed at exit
 *
 * This is a handy utility function that returns a talloc context which will be
 * automatically freed on program exit. This can be used to reduce the noise in
 * memory leak reports.
 */
void *talloc_autofree_context(void);

/**
 * talloc_array_length - get the number of elements in a talloc array
 * @p: the talloc pointer whose allocation to measure.
 *
 * This assumes that @p has been allocated as the same type.  NULL returns 0.
 *
 * See Also:
 *	talloc_get_size
 */
#define talloc_array_length(p) (talloc_get_size(p) / sizeof((*p)))

/**
 * talloc_get_size - get the requested size of an allocation
 * @ctx: the talloc pointer whose allocation to measure.
 *
 * This function lets you know the amount of memory alloced so far by this
 * context. It does NOT account for subcontext memory.
 *
 * See Also:
 *	talloc_array_length
 */
size_t talloc_get_size(const void *ctx);

/**
 * talloc_find_parent_byname - find a parent of this context with this name
 * @ctx: the context whose ancestors to search
 * @name: the name to look for
 *
 * Find a parent memory context of @ctx that has the given name. This can be
 * very useful in complex programs where it may be difficult to pass all
 * information down to the level you need, but you know the structure you want
 * is a parent of another context.
 */
void *talloc_find_parent_byname(const void *ctx, const char *name);

/**
 * talloc_set_allocator - set the allocations function(s) for talloc.
 * @malloc: the malloc function
 * @free: the free function
 * @realloc: the realloc function
 *
 * Instead of using the standard malloc, free and realloc, talloc will use
 * these replacements.  @realloc will never be called with size 0 or ptr NULL.
 */
void talloc_set_allocator(void *(*malloc)(size_t size),
			  void (*free)(void *ptr),
			  void *(*realloc)(void *ptr, size_t size));

/**
 * talloc_add_external - create an externally allocated node
 * @ctx: the parent
 * @realloc: the realloc() equivalent
 * @lock: the call to lock before manipulation of external nodes
 * @unlock: the call to unlock after manipulation of external nodes
 *
 * talloc_add_external() creates a node which uses a separate allocator.  All
 * children allocated from that node will also use that allocator.
 *
 * Note: Currently there is only one external allocator, not per-node,
 * and it is set with this function.
 *
 * @lock is handed a pointer which was previous returned from your realloc
 * function; you should use that to figure out which lock to get if you have
 * multiple external pools.
 *
 * The parent pointers in realloc is the talloc pointer of the parent, if any.
 */
void *talloc_add_external(const void *ctx,
			  void *(*realloc)(const void *parent,
					   void *ptr, size_t),
			  void (*lock)(const void *p),
			  void (*unlock)(void));

/* The following definitions come from talloc.c  */
void *_talloc(const void *context, size_t size);
void _talloc_set(void *ptr, const void *ctx, size_t size, const char *name);
void _talloc_set_destructor(const void *ptr, int (*destructor)(void *));
size_t talloc_reference_count(const void *ptr);
void *_talloc_reference(const void *context, const void *ptr);

void *WARN_UNUSED_RESULT _talloc_realloc(const void *context, void *ptr, size_t size, const char *name);
void *talloc_parent(const void *ptr);
const char *talloc_parent_name(const void *ptr);
void *_talloc_steal(const void *new_ctx, const void *ptr);
void *_talloc_move(const void *new_ctx, const void *pptr);
void *_talloc_zero(const void *ctx, size_t size, const char *name);
void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name);
void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name);
void *_talloc_zero_array(const void *ctx, size_t el_size, unsigned count, const char *name);
void *WARN_UNUSED_RESULT _talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name);
void *talloc_realloc_fn(const void *context, void *ptr, size_t size);
void talloc_show_parents(const void *context, FILE *file);
int talloc_is_parent(const void *context, const void *ptr);

#endif /* CCAN_TALLOC_H */
