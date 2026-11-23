/* host-only shim: glibc declares gethostname() with noexcept, libctru's soc.h without. newlib has no such clash. */
#define gethostname _host_gethostname
#include_next <unistd.h>
#undef gethostname
