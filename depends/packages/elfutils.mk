package=elfutils
$(package)_version=0.180
$(package)_download_path=https://sourceware.org/pub/$(package)/$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=b827b6e35c59d188ba97d7cf148fa8dc6f5c68eb6c5981888dfdbb758c0b569d

define $(package)_config_cmds
  cp -f $(BASEDIR)/config.guess config/config.guess &&\
  cp -f $(BASEDIR)/config.sub config/config.sub &&\
  $($(package)_autoconf) --enable-shared
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
