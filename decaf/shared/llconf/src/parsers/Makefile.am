EXTRA_DIST=mkallmodules

pkginclude_HEADERS = file.h ifupdown.h \
			pair.h ppp.h shell.h properties.h table.h ini.h tz.h pslave.h route.h iproute.h ipsec.h \
			funcexpr.h cyconf.h syslogng.h iptables.h mgetty.h snmpd.h conserver.h cron.h dhcp.h allmodules.h \
			options.h dhcp_leases.h xinetd.h python.h

AM_CFLAGS=-DLLCONF -I../

noinst_LTLIBRARIES = libllparsers.la

libllparsers_la_SOURCES=\
	allmodules.c allmodules.h \
	file.c file.h \
	ifupdown.c  ifupdown.h \
	shell.c  shell.h \
	properties.c  properties.h \
	table.c  table.h \
	pair.c  pair.h \
	ppp.c  ppp.h \
	ini.c ini.h \
	tz.c tz.h \
	route.c route.h \
	iproute.c iproute.h \
	pslave.c pslave.h \
	ipsec.c ipsec.h \
	funcexpr.c funcexpr.h \
	cyconf.c cyconf.h \
	syslogng.c syslogng.h \
	iptables.c iptables.h \
	snmpd.c snmpd.h \
	mgetty.c mgetty.h \
	conserver.c conserver.h \
	dhcp_leases.c dhcp_leases.h \
	cron.c cron.h \
	dhcp.c dhcp.h \
	xinetd.c xinetd.h \
	python.c python.h \
	options.c options.h

allmodules.h: allmodules
allmodules.c: allmodules
allmodules: mkallmodules
	./mkallmodules file shell properties table ifupdown pair ppp ini tz route iproute pslave ipsec funcexpr cyconf syslogng iptables mgetty snmpd conserver cron dhcp dhcp_leases python xinetd

clean:
	rm -f allmodules.h allmodules.c

.PHONY: clean allmodules
