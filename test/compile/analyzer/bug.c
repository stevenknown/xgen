typedef void (*fn_ptr_t) ();

void calls_free ()
{
}

bool test_2 (fn_ptr_t *fn_ptr)
{
  return *fn_ptr == calls_free;
}
