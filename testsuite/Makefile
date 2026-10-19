#
# Makefile
#
# Makefile for Poco OSPCore testsuite
#

.PHONY: projects
all: projects

clean:
	$(MAKE) -f Makefile-Driver clean
	$(MAKE) -f Makefile-TestBundle clean

distclean:
	$(MAKE) -f Makefile-Driver distclean
	$(MAKE) -f Makefile-TestBundle distclean

projects:
	$(MAKE) -f Makefile-Driver $(MAKECMDGOALS)
	$(MAKE) -f Makefile-TestBundle $(MAKECMDGOALS)
	$(MAKE) -f Makefile-TestBundle bundle
