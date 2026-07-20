#include "config.h"

#define HAVE_INET_PTON_NTOP 1
#define HAVE_IPv6 1
#define HAVE_POLL 1

#undef HAVE_CONFIG_H

#define register

// TODO: Check if we can remove extern "C" and put this in a namespace.

extern "C" {

#include "net/udns/udns.h"

#include "net/udns/udns_XtoX.c"
#include "net/udns/udns_bl.c"
#include "net/udns/udns_codes.c"
#include "net/udns/udns_dn.c"
#include "net/udns/udns_dntosp.c"
#include "net/udns/udns_init.c"
#include "net/udns/udns_jran.c"
#include "net/udns/udns_misc.c"
#include "net/udns/udns_parse.c"
#include "net/udns/udns_resolver.c"
#include "net/udns/udns_rr_a.c"
#include "net/udns/udns_rr_mx.c"
#include "net/udns/udns_rr_naptr.c"
#include "net/udns/udns_rr_ptr.c"
#include "net/udns/udns_rr_srv.c"
#include "net/udns/udns_rr_txt.c"

}
