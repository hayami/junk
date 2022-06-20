CC	= $(shell which cc > /dev/null 2>&1 && echo cc || echo gcc)
CFLAGS	= -O2 -Wall -Werror -Wno-unused-result $(DEFS)
LDFLAGS	= -s
LDLIBS	= -lssl -lcrypto

ssldir	:= $(shell \
	   (d=$$HOME/sys/local; [ -d $$d/include/openssl ] && echo $$d)	\
	|| (d=/usr/local;       [ -d $$d/include/openssl ] && echo $$d))
ifneq ($(ssldir),)
TLSINC	:= $(ssldir)/include
TLSLIB	:= $(ssldir)/lib
CFLAGS	:= $(CFLAGS) -I $(TLSINC)
LDFLAGS	:= $(LDFLAGS) -L $(TLSLIB)
endif

UNAME	:= $(shell uname)
ifeq ($(UNAME),Linux)
endif
ifeq ($(UNAME),FreeBSD)
LDFLAGS	:= $(LDFLAGS) -Wl,-rpath,$(TLSLIB)
endif

all:	a.out getroot

a.out:	example.o tls.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

getroot: getroot.o tls.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

MAKEFLAGS += --no-print-directory

.PHONY: clean
clean:
	rm -f *.o core a.out getroot

.PHONY:	test badssl-test
test badssl-test:
	@echo
	@echo '[certificate]'
	@$(MAKE) try e=bad	r=expired.badssl.com
	@$(MAKE) try e=bad	r=wrong.host.badssl.com
	@$(MAKE) try e=bad	r=self-signed.badssl.com
	@$(MAKE) try e=bad	r=untrusted-root.badssl.com
	@$(MAKE) try e=bad	r=revoked.badssl.com
	@$(MAKE) try e=bad	r=pinning-test.badssl.com
	@$(MAKE) try e=dubious	r=no-common-name.badssl.com
	@$(MAKE) try e=dubious	r=no-subject.badssl.com
	@$(MAKE) try e=dubious	r=incomplete-chain.badssl.com
	@$(MAKE) try e=dubious	r=reversed-chain.badssl.com
	@$(MAKE) try e=good	r=sha256.badssl.com
	@$(MAKE) try e=good	r=sha384.badssl.com
	@$(MAKE) try e=good	r=sha512.badssl.com
	@$(MAKE) try e=good	r=1000-sans.badssl.com
	@$(MAKE) try e=good	r=10000-sans.badssl.com
	@$(MAKE) try e=good	r=ecc256.badssl.com
	@$(MAKE) try e=good	r=ecc384.badssl.com
	@$(MAKE) try e=good	r=rsa2048.badssl.com
	@$(MAKE) try e=good	r=rsa4096.badssl.com
	@$(MAKE) try e=dubious	r=rsa8192.badssl.com
	@$(MAKE) try e=good	r=extended-validation.badssl.com
	@echo
	@echo '[client-certificate]'
	@$(MAKE) try e=good	r=client.badssl.com
	@$(MAKE) try e=bad	r=client-cert-missing.badssl.com
	@echo
	@echo '[mixed-content]'
	@$(MAKE) try e=bad	r=mixed-script.badssl.com
	@$(MAKE) try e=bad	r=very.badssl.com
	@$(MAKE) try e=dubious	r=mixed.badssl.com
	@$(MAKE) try e=dubious	r=mixed-favicon.badssl.com
	@$(MAKE) try e=dubious	r=mixed-form.badssl.com
	@#echo
	@#echo '[http]'
	@#$(MAKE) try e=bad	r=http://http.badssl.com
	@#$(MAKE) try e=bad	r=http://http-textarea.badssl.com
	@#$(MAKE) try e=bad	r=http://http-password.badssl.com
	@#$(MAKE) try e=bad	r=http://http-login.badssl.com
	@#$(MAKE) try e=bad	r=http://http-dynamic-login.badssl.com
	@#$(MAKE) try e=bad	r=http://http-credit-card.badssl.com
	@echo
	@echo '[cipher-suite]'
	@$(MAKE) try e=dubious	r=cbc.badssl.com
	@$(MAKE) try e=bad	r=rc4-md5.badssl.com
	@$(MAKE) try e=bad	r=rc4.badssl.com
	@$(MAKE) try e=bad	r=3des.badssl.com
	@$(MAKE) try e=bad	r=null.badssl.com
	@#$(MAKE) try e=bad	r=mozilla-old.badssl.com
	@#$(MAKE) try e=dubious	r=mozilla-intermediate.badssl.com
	@#$(MAKE) try e=good	r=mozilla-modern.badssl.com
	@echo
	@echo '[key-exchange]'
	@$(MAKE) try e=bad	r=dh480.badssl.com
	@$(MAKE) try e=bad	r=dh512.badssl.com
	@$(MAKE) try e=bad	r=dh1024.badssl.com
	@$(MAKE) try e=dubious	r=dh2048.badssl.com
	@$(MAKE) try e=bad	r=dh-small-subgroup.badssl.com
	@$(MAKE) try e=bad	r=dh-composite.badssl.com
	@$(MAKE) try e=dubious	r=static-rsa.badssl.com
	@echo
	@echo '[protocol]'
	@$(MAKE) try e=dubious	r=tls-v1-0.badssl.com:1010
	@$(MAKE) try e=dubious	r=tls-v1-1.badssl.com:1011
	@$(MAKE) try e=good	r=tls-v1-2.badssl.com:1012
	@echo
	@echo '[certificate-transparency]'
	@$(MAKE) try e=bad	r=no-sct.badssl.com
	@echo
	@echo '[upgrade]'
	@$(MAKE) try e=good	r=hsts.badssl.com
	@$(MAKE) try e=good	r=upgrade.badssl.com
	@$(MAKE) try e=good	r=preloaded-hsts.badssl.com
	@$(MAKE) try e=bad	r=subdomain.preloaded-hsts.badssl.com
	@$(MAKE) try e=good	r=https-everywhere.badssl.com
	@echo
	@echo '[ui]'
	@$(MAKE) try e=dubious	r=spoofed-favicon.badssl.com
	@$(MAKE) try e=dubious	r=lock-title.badssl.com
	@$(MAKE) try e=good	r=long-extended-subdomain-name-containing-many-letters-and-dashes.badssl.com
	@$(MAKE) try e=good	r=longextendedsubdomainnamewithoutdashesinordertotestwordwrapping.badssl.com
	@echo
	@echo '[known-bad]'
	@$(MAKE) try e=bad	r=superfish.badssl.com
	@$(MAKE) try e=bad	r=edellroot.badssl.com
	@$(MAKE) try e=bad	r=dsdtestprovider.badssl.com
	@$(MAKE) try e=bad	r=preact-cli.badssl.com
	@$(MAKE) try e=bad	r=webpack-dev-server.badssl.com
	@#echo
	@#echo '[chrome]'
	@#$(MAKE) try e=bad	r=captive-portal.badssl.com
	@#$(MAKE) try e=bad	r=mitm-software.badssl.com
	@echo
	@echo '[defunct]'
	@$(MAKE) try e=dubious	r=sha1-2016.badssl.com
	@$(MAKE) try e=bad	r=sha1-2017.badssl.com
	@$(MAKE) try e=bad	r=sha1-intermediate.badssl.com
	@$(MAKE) try e=bad	r=invalid-expected-sct.badssl.com

.PHONY:	try
try:	getroot
	@./getroot $(r) > /dev/null; ret=$$?; \
	echo expected=$(e) result=$$ret remote=$(r)

# vim: noet sw=8 sts=8
