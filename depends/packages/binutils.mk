package=binutils
$(package)_version=2.30
$(package)_download_path=https://mirrors.nav.ro/gnu/binutils/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=8c3850195d1c093d290a716e20ebcaa72eda32abf5e3d8611154b39cff79e9ea

define $(package)_config_cmds
  cp -f $(BASEDIR)/config.guess config/config.guess &&\
  cp -f $(BASEDIR)/config.sub config/config.sub &&\
  $($(package)_autoconf) --enable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
