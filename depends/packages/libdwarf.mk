package=libdwarf
$(package)_version=0.3.0
$(package)_download_path=https://www.prevanders.net/
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=dbf4ab40bfe83787e4648ce14a734ba8b2fd4be57e781e6f5a0314d3ed19c9fe

define $(package)_config_cmds
  mkdir config &&\
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
