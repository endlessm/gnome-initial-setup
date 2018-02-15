NULL =

edit = $(AM_V_GEN) sed \
	-e 's|@SBINDIR[@]|$(sbindir)|g' \
	-e 's|@SYSCONFDIR[@]|$(sysconfdir)|g' \
	-e 's|@LIBDIR[@]|$(libdir)|g' \
	-e 's|@LIBEXECDIR[@]|$(libexecdir)|g' \
	-e 's|@LOCALSTATEDIR[@]|$(localstatedir)|g'

%.desktop.in: %.desktop.in.in Makefile
	$(edit) $< >$@

%.policy: %.policy.in Makefile
	$(edit) $< >$@

%.service: %.service.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

rulesdir = $(datadir)/polkit-1/rules.d
rules_DATA = \
	20-gnome-initial-setup.rules \
	com.endlessm.DemoMode.rules \
	$(NULL)

actionsdir = $(datadir)/polkit-1/actions
actions_DATA = \
	com.endlessm.TestMode.policy \
	com.endlessm.DemoMode.policy \
	$(NULL)

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = gnome-initial-setup.session

modedir = $(datadir)/gnome-shell/modes
mode_DATA = initial-setup.json

desktopdir = $(datadir)/gdm/greeter/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	setup-shell.desktop				\
	$(NULL)

demovideodir = $(datadir)/eos-demo-mode
demovideo_DATA = \
	eos-demo-video.webm \
	$(NULL)

EXTRA_DIST =						\
	20-gnome-initial-setup.rules			\
	eos-demo-video.webm				\
	eos-demo-video.desktop.in.in			\
	eos-transient-setup.service.in			\
	gnome-welcome-tour				\
	setup-shell.desktop				\
	gnome-welcome-tour.desktop.in.in		\
	gnome-initial-setup-first-login.desktop.in.in	\
	gnome-initial-setup.desktop.in.in		\
	gnome-initial-setup-copy-worker.desktop.in.in	\
	gnome-initial-setup.session			\
	initial-setup.json				\
	product_serial.conf                             \
	com.endlessm.TestMode.policy.in			\
	com.endlessm.DemoMode.policy.in			\
	com.endlessm.DemoMode.rules                     \
	$(NULL)

CLEANFILES =						\
	eos-demo-video.desktop.in			\
	eos-demo-video.desktop				\
	eos-transient-setup.service			\
	gnome-initial-setup.desktop.in			\
	gnome-initial-setup.desktop			\
	gnome-initial-setup-copy-worker.desktop.in	\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	gnome-welcome-tour.desktop.in			\
	gnome-welcome-tour.desktop			\
	com.endlessm.TestMode.policy			\
	com.endlessm.DemoMode.policy			\
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =					\
	gnome-welcome-tour.desktop			\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	eos-demo-video.desktop \
	$(NULL)

libexec_SCRIPTS =						\
	gnome-welcome-tour

systemdsystemunit_DATA = \
	eos-transient-setup.service \
	$(NULL)

tmpfilesdir = $(prefix)/lib/tmpfiles.d
tmpfiles_DATA =                                         \
	product_serial.conf                             \
	$(NULL)
