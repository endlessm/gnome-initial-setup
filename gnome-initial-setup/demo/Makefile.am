libexec_PROGRAMS = eos-demo-video

eos_demo_video_SOURCES =	\
	eos-demo-video.c	\
	$(NULL)

AM_CPPFLAGS = \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDEMOVIDEODATADIR=\""$(datadir)/eos-demo-mode"\" \
	$(NULL)

eos_demo_video_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(NULL)

eos_demo_video_LDADD = $(INITIAL_SETUP_LIBS)
