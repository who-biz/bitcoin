package=binutils
$(package)_version=2.30.0
$(package)_download_path=https://sourceware.org/pub/$(package)/snapshots/
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=185e3f1579863d900f8534d7b0b9ec2d12f6076615bedcc67fe5700f87b65ada

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
