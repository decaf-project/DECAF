#define CTL_CMD		0
#define CTL_EXEC	1
#define CTL_ALIAS	2
#define CTL_DNS		3
/* NOTE: DNS_ADDR_MAX addresses, starting from CTL_DNS, are reserved */

#define CTL_IS_DNS(x)   ((unsigned)((x)-CTL_DNS) < (unsigned)dns_addr_count)

#define CTL_SPECIAL	"10.0.2.0"
#define CTL_LOCAL	"10.0.2.15"
