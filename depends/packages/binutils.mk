package=binutils
$(package)_version=2.22
$(package)_download_path=https://mirrors.nav.ro/gnu/binutils/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=12c26349fc7bb738f84b9826c61e103203187ca2d46f08b82e61e21fcbc6e3e6

define $(package)_config_cmds
  cp -f $(BASEDIR)/config.guess config/config.guess &&\
  cp -f $(BASEDIR)/config.sub config/config.sub &&\
  $($(package)_autoconf) --disable-shared --enable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
