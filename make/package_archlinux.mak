#archlinux-related rules
pkg_archlinux = $(pkg_dir)/archlinux

pkg_xz = $(pkg_dir)/$(binary_name)-r$(pkg_revision)$(pkg_subversion)-1-$(arch).pkg.tar.xz

package_archlinux:
	@$-$(call rmfiles_cmd,$(pkg_xz) $(pkg_log))
	@$(info Creating package $(pkg_xz))
	@$(MAKE) $(pkg_xz) > $(pkg_log) 2>&1

$(pkg_xz): pkg_dependency
	@$(call showtime_cmd)
	@$(MAKE) DESTDIR=$(pkg_root) install_archlinux
	(cd $(pkg_dir) && makepkg -c -p archlinux/PKGBUILD --config archlinux/makepkg.conf)
	$(call rmdir_cmd,$(pkg_archlinux))

$(pkg_archlinux):
	$(call makedir_cmd,$@)

install_archlinux: install_linux $(pkg_archlinux)/PKGBUILD $(pkg_archlinux)/makepkg.conf

$(pkg_archlinux)/PKGBUILD: | $(pkg_archlinux)
	@echo -e "\
	# Maintainer: vitamin.caig@gmail.com\n\
	pkgname=$(binary_name)\n\
	pkgver=r$(pkg_revision)\n\
	pkgrel=1\n\
	pkgdesc=\"The $(binary_name) application is used to play chiptunes from ZX Spectrum\"\n\
	arch=('$(arch)')\n\
	url=\"http://zxtune.googlecode.com\"\n\
	license=('GPL3')\n\
	depends=('zlib')\n\
	provides=('$(binary_name)')\n\
	options=(!strip !docs !libtool !emptydirs !zipman makeflags)\n\n\
	package() {\n\
	cp -R $(CURDIR)/$(pkg_root)/* \x24{pkgdir}\n\
	}\n\
	" > $@

$(pkg_archlinux)/makepkg.conf: | $(pkg_archlinux)
	@echo -e "\
	CARCH=\"$(arch)\"\n\
	CHOST=\"$(arch)-unknown-linux-gnu\"\n\
	BUILDENV=(fakeroot)\n\
	PKGEXT='.pkg.tar.xz'\n\
	" > $@