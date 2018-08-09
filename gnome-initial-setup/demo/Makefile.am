libexec_PROGRAMS = eos-demo-video

eos_demo_video_SOURCES =	\
	eos-demo-video.c	\
	eos-demo-video.h	\
	$(NULL)

AM_CPPFLAGS = \
	-DDEMOVIDEODATADIR=\""$(datadir)/eos-demo-mode"\" \
	$(NULL)

eos_demo_video_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(NULL)

eos_demo_video_LDADD = $(INITIAL_SETUP_LIBS)
