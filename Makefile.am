SUBDIRS = src man

EXTRA_DIST = examples

dist-hook:
	find $(distdir) -type f -name "*.pyc" -delete
	find $(distdir) -type f -name "*.sqlite" -delete
	git log --shortstat -C -C > $(distdir)/ChangeLog || true
