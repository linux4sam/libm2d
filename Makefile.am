AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src test

EXTRA_DIST = README.md COPYING docs/Doxyfile.in

.PHONY: docs

docs: docs/Doxyfile
	@cd docs && doxygen
