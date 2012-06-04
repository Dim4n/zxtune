samples_path := $(path_step)/samples/chiptunes

install_samples:
	$(call makedir_cmd,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/as0/Samba.as0,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/asc/SANDRA.asc,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/ay/AYMD39.ay,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/chi/ducktale.chi,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/dmm/popcorn.dmm,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/dst/EchoDreams.dst,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/gtr/L.Boy.gtr,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/pdt/TechCent.M,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/psm/Calamba.psm,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/pt1/twins.pt1,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/pt2/Illusion.pt2,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/pt3/Speccy2.pt3,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/sqd/Maskarad.sqd,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/st1/SHOCK4.S,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/stc/stracker.stc,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/stp/ZXGuide3_07.stp,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/str/COMETDAY.str,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/ts/long_day.pt3,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/vtx/Enlight3.vtx,$(DESTDIR)/Samples)
	$(call copyfile_cmd,$(samples_path)/ym/Kurztech.ym,$(DESTDIR)/Samples)

install_samples_playlist: 
	$(call copyfile_cmd,$(path_step)/samples/playlists/samples.xspf,$(DESTDIR))
