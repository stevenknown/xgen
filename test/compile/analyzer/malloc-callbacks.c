#include <stdlib.h>
#include <bltin.h>

typedef void *(*allocator_t) (size_t);
typedef void (*deallocator_t) (void *);

static allocator_t
get_malloc (void)
{
  return malloc;
}

static allocator_t 
get_alloca (void)
{
  /* On e.g. Solaris, alloca is a macro so we can't take its address;
     use __builtin_alloca instead.  */
  return __builtin_alloca;
}

static deallocator_t 
get_free (void)
{
  return free;
}

void test_1 (void *ptr)
{
  deallocator_t dealloc_fn = free;
  dealloc_fn (ptr); /* { dg-message "first 'free' here" } */
  dealloc_fn (ptr); /* { dg-warning "double-'free'" } */
}

void test_2 (void *ptr)
{
  deallocator_t dealloc_fn = get_free ();
  dealloc_fn (ptr); /* { dg-message "first 'free' here" } */
  dealloc_fn (ptr); /* { dg-warning "double-'free'" } */
}

static void 
called_by_test_3 (void *ptr, deallocator_t dealloc_fn)
{
  dealloc_fn (ptr); /* { dg-warning "double-'free'" } */
}

void test_3 (void *ptr)
{
  called_by_test_3 (ptr, free);
  called_by_test_3 (ptr, free);
}

int *test_4 (void)
{
  allocator_t alloc_fn = get_malloc ();
  int *ptr = alloc_fn (sizeof (int)); /* { dg-message "this call could return NULL" } */
  *ptr = 42; /* { dg-warning "dereference of possibly-NULL 'ptr'" } */
  return ptr;
}

int *test_5 (void)
{
  allocator_t alloc_fn = get_alloca ();
  deallocator_t dealloc_fn = get_free ();
  int *ptr = alloc_fn (sizeof (int)); /* { dg-message "pointer is from here" } */
  /* TODO: message should read "memory is allocated on the stack here".  */
  dealloc_fn (ptr); /* { dg-warning "'free' of 'ptr' which points to memory not on the heap" } */
}

static void 
called_by_test_6a (void *ptr)
{
  free (ptr); /* { dg-warning "double-'free'" "" { xfail *-*-* } } */
}

static deallocator_t 
called_by_test_6b (void)
{
  return called_by_test_6a;
}

void test_6 (void *ptr)
{
  deallocator_t dealloc_fn = called_by_test_6b ();
  dealloc_fn (ptr);
  dealloc_fn (ptr);
}
