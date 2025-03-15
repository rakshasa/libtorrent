#include "config.h"

#undef HAVE_CONFIG_H

// TOOD: Add namespace torrent::net::udns.

#include "net/udns/udns.h"

#include "net/udns/dnsget.c"
#include "net/udns/ex-rdns.c"
#include "net/udns/getopt.c"
#include "net/udns/inet_XtoX.c"
#include "net/udns/rblcheck.c"
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
