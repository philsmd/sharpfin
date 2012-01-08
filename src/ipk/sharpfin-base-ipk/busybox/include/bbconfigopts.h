#ifndef _BBCONFIGOPTS_H
#define _BBCONFIGOPTS_H
/*
 * busybox configuration settings.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * This file is generated automatically by scripts/mkconfigs.
 * Do not edit.
 *
 */
static const char *const bbconfig_config =
"CONFIG_HAVE_DOT_CONFIG=y\n"
"CONFIG_NITPICK=y\n"
"# CONFIG_DESKTOP is not set\n"
"CONFIG_FEATURE_BUFFERS_USE_MALLOC=y\n"
"# CONFIG_FEATURE_BUFFERS_GO_ON_STACK is not set\n"
"# CONFIG_FEATURE_BUFFERS_GO_IN_BSS is not set\n"
"CONFIG_SHOW_USAGE=y\n"
"CONFIG_FEATURE_VERBOSE_USAGE=y\n"
"CONFIG_FEATURE_COMPRESS_USAGE=y\n"
"CONFIG_FEATURE_INSTALLER=y\n"
"CONFIG_LOCALE_SUPPORT=y\n"
"CONFIG_GETOPT_LONG=y\n"
"CONFIG_FEATURE_DEVPTS=y\n"
"# CONFIG_FEATURE_CLEAN_UP is not set\n"
"CONFIG_FEATURE_PIDFILE=y\n"
"CONFIG_FEATURE_SUID=y\n"
"CONFIG_FEATURE_SUID_CONFIG=y\n"
"CONFIG_FEATURE_SUID_CONFIG_QUIET=y\n"
"# CONFIG_SELINUX is not set\n"
"# CONFIG_FEATURE_PREFER_APPLETS is not set\n"
"CONFIG_BUSYBOX_EXEC_PATH=\"/proc/self/exe\"\n"
"CONFIG_FEATURE_SYSLOG=y\n"
"CONFIG_FEATURE_HAVE_RPC=y\n"
"# CONFIG_STATIC is not set\n"
"# CONFIG_NOMMU is not set\n"
"# CONFIG_BUILD_LIBBUSYBOX is not set\n"
"# CONFIG_FEATURE_INDIVIDUAL is not set\n"
"# CONFIG_FEATURE_SHARED_BUSYBOX is not set\n"
"CONFIG_LFS=y\n"
"# CONFIG_DEBUG is not set\n"
"# CONFIG_WERROR is not set\n"
"CONFIG_NO_DEBUG_LIB=y\n"
"# CONFIG_DMALLOC is not set\n"
"# CONFIG_EFENCE is not set\n"
"CONFIG_INCLUDE_SUSv2=y\n"
"# CONFIG_INSTALL_NO_USR is not set\n"
"CONFIG_INSTALL_APPLET_SYMLINKS=y\n"
"# CONFIG_INSTALL_APPLET_HARDLINKS is not set\n"
"# CONFIG_INSTALL_APPLET_SCRIPT_WRAPPERS is not set\n"
"# CONFIG_INSTALL_APPLET_DONT is not set\n"
"# CONFIG_INSTALL_SH_APPLET_SYMLINK is not set\n"
"# CONFIG_INSTALL_SH_APPLET_HARDLINK is not set\n"
"# CONFIG_INSTALL_SH_APPLET_SCRIPT_WRAPPER is not set\n"
"CONFIG_PREFIX=\"./_install\"\n"
"CONFIG_PASSWORD_MINLEN=6\n"
"CONFIG_MD5_SIZE_VS_SPEED=2\n"
"CONFIG_FEATURE_FAST_TOP=y\n"
"# CONFIG_FEATURE_ETC_NETWORKS is not set\n"
"CONFIG_FEATURE_EDITING=y\n"
"CONFIG_FEATURE_EDITING_MAX_LEN=1024\n"
"# CONFIG_FEATURE_EDITING_VI is not set\n"
"CONFIG_FEATURE_EDITING_HISTORY=15\n"
"# CONFIG_FEATURE_EDITING_SAVEHISTORY is not set\n"
"CONFIG_FEATURE_TAB_COMPLETION=y\n"
"# CONFIG_FEATURE_USERNAME_COMPLETION is not set\n"
"# CONFIG_FEATURE_EDITING_FANCY_PROMPT is not set\n"
"# CONFIG_FEATURE_VERBOSE_CP_MESSAGE is not set\n"
"CONFIG_FEATURE_COPYBUF_KB=4\n"
"# CONFIG_MONOTONIC_SYSCALL is not set\n"
"CONFIG_IOCTL_HEX2STR_ERROR=y\n"
"CONFIG_AR=y\n"
"CONFIG_FEATURE_AR_LONG_FILENAMES=y\n"
"CONFIG_BUNZIP2=y\n"
"CONFIG_BZIP2=y\n"
"CONFIG_CPIO=y\n"
"# CONFIG_DPKG is not set\n"
"# CONFIG_DPKG_DEB is not set\n"
"# CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY is not set\n"
"CONFIG_GUNZIP=y\n"
"CONFIG_FEATURE_GUNZIP_UNCOMPRESS=y\n"
"CONFIG_GZIP=y\n"
"CONFIG_RPM2CPIO=y\n"
"CONFIG_RPM=y\n"
"CONFIG_FEATURE_RPM_BZ2=y\n"
"CONFIG_TAR=y\n"
"CONFIG_FEATURE_TAR_CREATE=y\n"
"CONFIG_FEATURE_TAR_GZIP=y\n"
"CONFIG_FEATURE_TAR_BZIP2=y\n"
"CONFIG_FEATURE_TAR_LZMA=y\n"
"CONFIG_FEATURE_TAR_COMPRESS=y\n"
"CONFIG_FEATURE_TAR_AUTODETECT=y\n"
"CONFIG_FEATURE_TAR_FROM=y\n"
"CONFIG_FEATURE_TAR_OLDGNU_COMPATIBILITY=y\n"
"CONFIG_FEATURE_TAR_OLDSUN_COMPATIBILITY=y\n"
"CONFIG_FEATURE_TAR_GNU_EXTENSIONS=y\n"
"CONFIG_FEATURE_TAR_LONG_OPTIONS=y\n"
"CONFIG_FEATURE_TAR_UNAME_GNAME=y\n"
"CONFIG_UNCOMPRESS=y\n"
"CONFIG_UNLZMA=y\n"
"CONFIG_FEATURE_LZMA_FAST=y\n"
"CONFIG_UNZIP=y\n"
"CONFIG_FEATURE_UNARCHIVE_TAPE=y\n"
"# CONFIG_FEATURE_DEB_TAR_GZ is not set\n"
"# CONFIG_FEATURE_DEB_TAR_BZ2 is not set\n"
"# CONFIG_FEATURE_DEB_TAR_LZMA is not set\n"
"CONFIG_BASENAME=y\n"
"CONFIG_CAL=y\n"
"CONFIG_CAT=y\n"
"CONFIG_CATV=y\n"
"CONFIG_CHGRP=y\n"
"CONFIG_CHMOD=y\n"
"CONFIG_CHOWN=y\n"
"CONFIG_CHROOT=y\n"
"CONFIG_CKSUM=y\n"
"CONFIG_COMM=y\n"
"CONFIG_CP=y\n"
"CONFIG_CUT=y\n"
"CONFIG_DATE=y\n"
"CONFIG_FEATURE_DATE_ISOFMT=y\n"
"CONFIG_DD=y\n"
"CONFIG_FEATURE_DD_SIGNAL_HANDLING=y\n"
"CONFIG_FEATURE_DD_IBS_OBS=y\n"
"CONFIG_DF=y\n"
"CONFIG_FEATURE_DF_INODE=y\n"
"CONFIG_DIRNAME=y\n"
"CONFIG_DOS2UNIX=y\n"
"CONFIG_UNIX2DOS=y\n"
"CONFIG_DU=y\n"
"CONFIG_FEATURE_DU_DEFAULT_BLOCKSIZE_1K=y\n"
"CONFIG_ECHO=y\n"
"CONFIG_FEATURE_FANCY_ECHO=y\n"
"CONFIG_ENV=y\n"
"CONFIG_FEATURE_ENV_LONG_OPTIONS=y\n"
"CONFIG_EXPAND=y\n"
"CONFIG_FEATURE_EXPAND_LONG_OPTIONS=y\n"
"CONFIG_EXPR=y\n"
"CONFIG_EXPR_MATH_SUPPORT_64=y\n"
"CONFIG_FALSE=y\n"
"CONFIG_FOLD=y\n"
"CONFIG_HEAD=y\n"
"CONFIG_FEATURE_FANCY_HEAD=y\n"
"CONFIG_HOSTID=y\n"
"CONFIG_ID=y\n"
"CONFIG_INSTALL=y\n"
"CONFIG_FEATURE_INSTALL_LONG_OPTIONS=y\n"
"CONFIG_LENGTH=y\n"
"CONFIG_LN=y\n"
"CONFIG_LOGNAME=y\n"
"CONFIG_LS=y\n"
"CONFIG_FEATURE_LS_FILETYPES=y\n"
"CONFIG_FEATURE_LS_FOLLOWLINKS=y\n"
"CONFIG_FEATURE_LS_RECURSIVE=y\n"
"CONFIG_FEATURE_LS_SORTFILES=y\n"
"CONFIG_FEATURE_LS_TIMESTAMPS=y\n"
"CONFIG_FEATURE_LS_USERNAME=y\n"
"CONFIG_FEATURE_LS_COLOR=y\n"
"CONFIG_FEATURE_LS_COLOR_IS_DEFAULT=y\n"
"CONFIG_MD5SUM=y\n"
"CONFIG_MKDIR=y\n"
"CONFIG_FEATURE_MKDIR_LONG_OPTIONS=y\n"
"CONFIG_MKFIFO=y\n"
"CONFIG_MKNOD=y\n"
"CONFIG_MV=y\n"
"CONFIG_FEATURE_MV_LONG_OPTIONS=y\n"
"CONFIG_NICE=y\n"
"CONFIG_NOHUP=y\n"
"CONFIG_OD=y\n"
"CONFIG_PRINTENV=y\n"
"CONFIG_PRINTF=y\n"
"CONFIG_PWD=y\n"
"CONFIG_READLINK=y\n"
"CONFIG_FEATURE_READLINK_FOLLOW=y\n"
"CONFIG_REALPATH=y\n"
"CONFIG_RM=y\n"
"CONFIG_RMDIR=y\n"
"# CONFIG_FEATURE_RMDIR_LONG_OPTIONS is not set\n"
"CONFIG_SEQ=y\n"
"CONFIG_SHA1SUM=y\n"
"CONFIG_SLEEP=y\n"
"CONFIG_FEATURE_FANCY_SLEEP=y\n"
"CONFIG_SORT=y\n"
"CONFIG_FEATURE_SORT_BIG=y\n"
"CONFIG_SPLIT=y\n"
"CONFIG_FEATURE_SPLIT_FANCY=y\n"
"CONFIG_STAT=y\n"
"CONFIG_FEATURE_STAT_FORMAT=y\n"
"CONFIG_STTY=y\n"
"CONFIG_SUM=y\n"
"CONFIG_SYNC=y\n"
"CONFIG_TAC=y\n"
"CONFIG_TAIL=y\n"
"CONFIG_FEATURE_FANCY_TAIL=y\n"
"CONFIG_TEE=y\n"
"CONFIG_FEATURE_TEE_USE_BLOCK_IO=y\n"
"CONFIG_TEST=y\n"
"CONFIG_FEATURE_TEST_64=y\n"
"CONFIG_TOUCH=y\n"
"CONFIG_TR=y\n"
"CONFIG_FEATURE_TR_CLASSES=y\n"
"CONFIG_FEATURE_TR_EQUIV=y\n"
"CONFIG_TRUE=y\n"
"CONFIG_TTY=y\n"
"CONFIG_UNAME=y\n"
"CONFIG_UNEXPAND=y\n"
"CONFIG_FEATURE_UNEXPAND_LONG_OPTIONS=y\n"
"CONFIG_UNIQ=y\n"
"CONFIG_USLEEP=y\n"
"CONFIG_UUDECODE=y\n"
"CONFIG_UUENCODE=y\n"
"CONFIG_WC=y\n"
"CONFIG_FEATURE_WC_LARGE=y\n"
"CONFIG_WHO=y\n"
"CONFIG_WHOAMI=y\n"
"CONFIG_YES=y\n"
"CONFIG_FEATURE_PRESERVE_HARDLINKS=y\n"
"CONFIG_FEATURE_AUTOWIDTH=y\n"
"CONFIG_FEATURE_HUMAN_READABLE=y\n"
"CONFIG_FEATURE_MD5_SHA1_SUM_CHECK=y\n"
"CONFIG_CHVT=y\n"
"CONFIG_CLEAR=y\n"
"CONFIG_DEALLOCVT=y\n"
"CONFIG_DUMPKMAP=y\n"
"CONFIG_KBD_MODE=y\n"
"CONFIG_LOADFONT=y\n"
"CONFIG_LOADKMAP=y\n"
"CONFIG_OPENVT=y\n"
"CONFIG_RESET=y\n"
"CONFIG_RESIZE=y\n"
"CONFIG_FEATURE_RESIZE_PRINT=y\n"
"CONFIG_SETCONSOLE=y\n"
"CONFIG_FEATURE_SETCONSOLE_LONG_OPTIONS=y\n"
"CONFIG_SETKEYCODES=y\n"
"CONFIG_SETLOGCONS=y\n"
"CONFIG_MKTEMP=y\n"
"CONFIG_PIPE_PROGRESS=y\n"
"CONFIG_RUN_PARTS=y\n"
"CONFIG_FEATURE_RUN_PARTS_LONG_OPTIONS=y\n"
"CONFIG_FEATURE_RUN_PARTS_FANCY=y\n"
"CONFIG_START_STOP_DAEMON=y\n"
"CONFIG_FEATURE_START_STOP_DAEMON_FANCY=y\n"
"CONFIG_FEATURE_START_STOP_DAEMON_LONG_OPTIONS=y\n"
"CONFIG_WHICH=y\n"
"CONFIG_AWK=y\n"
"CONFIG_FEATURE_AWK_MATH=y\n"
"CONFIG_CMP=y\n"
"CONFIG_DIFF=y\n"
"CONFIG_FEATURE_DIFF_BINARY=y\n"
"CONFIG_FEATURE_DIFF_DIR=y\n"
"CONFIG_FEATURE_DIFF_MINIMAL=y\n"
"CONFIG_ED=y\n"
"CONFIG_PATCH=y\n"
"CONFIG_SED=y\n"
"CONFIG_VI=y\n"
"CONFIG_FEATURE_VI_MAX_LEN=4096\n"
"# CONFIG_FEATURE_VI_8BIT is not set\n"
"CONFIG_FEATURE_VI_COLON=y\n"
"CONFIG_FEATURE_VI_YANKMARK=y\n"
"CONFIG_FEATURE_VI_SEARCH=y\n"
"CONFIG_FEATURE_VI_USE_SIGNALS=y\n"
"CONFIG_FEATURE_VI_DOT_CMD=y\n"
"CONFIG_FEATURE_VI_READONLY=y\n"
"CONFIG_FEATURE_VI_SETOPTS=y\n"
"CONFIG_FEATURE_VI_SET=y\n"
"CONFIG_FEATURE_VI_WIN_RESIZE=y\n"
"CONFIG_FEATURE_VI_OPTIMIZE_CURSOR=y\n"
"CONFIG_FEATURE_ALLOW_EXEC=y\n"
"CONFIG_FIND=y\n"
"CONFIG_FEATURE_FIND_PRINT0=y\n"
"CONFIG_FEATURE_FIND_MTIME=y\n"
"CONFIG_FEATURE_FIND_MMIN=y\n"
"CONFIG_FEATURE_FIND_PERM=y\n"
"CONFIG_FEATURE_FIND_TYPE=y\n"
"CONFIG_FEATURE_FIND_XDEV=y\n"
"CONFIG_FEATURE_FIND_MAXDEPTH=y\n"
"CONFIG_FEATURE_FIND_NEWER=y\n"
"CONFIG_FEATURE_FIND_INUM=y\n"
"CONFIG_FEATURE_FIND_EXEC=y\n"
"CONFIG_FEATURE_FIND_USER=y\n"
"CONFIG_FEATURE_FIND_GROUP=y\n"
"CONFIG_FEATURE_FIND_NOT=y\n"
"CONFIG_FEATURE_FIND_DEPTH=y\n"
"CONFIG_FEATURE_FIND_PAREN=y\n"
"CONFIG_FEATURE_FIND_SIZE=y\n"
"CONFIG_FEATURE_FIND_PRUNE=y\n"
"CONFIG_FEATURE_FIND_DELETE=y\n"
"CONFIG_FEATURE_FIND_PATH=y\n"
"CONFIG_FEATURE_FIND_REGEX=y\n"
"# CONFIG_FEATURE_FIND_CONTEXT is not set\n"
"CONFIG_GREP=y\n"
"CONFIG_FEATURE_GREP_EGREP_ALIAS=y\n"
"CONFIG_FEATURE_GREP_FGREP_ALIAS=y\n"
"CONFIG_FEATURE_GREP_CONTEXT=y\n"
"CONFIG_XARGS=y\n"
"CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION=y\n"
"CONFIG_FEATURE_XARGS_SUPPORT_QUOTES=y\n"
"CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT=y\n"
"CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM=y\n"
"CONFIG_INIT=y\n"
"# CONFIG_DEBUG_INIT is not set\n"
"CONFIG_FEATURE_USE_INITTAB=y\n"
"# CONFIG_FEATURE_KILL_REMOVED is not set\n"
"CONFIG_FEATURE_KILL_DELAY=0\n"
"CONFIG_FEATURE_INIT_SCTTY=y\n"
"# CONFIG_FEATURE_INIT_SYSLOG is not set\n"
"CONFIG_FEATURE_EXTRA_QUIET=y\n"
"CONFIG_FEATURE_INIT_COREDUMPS=y\n"
"CONFIG_FEATURE_INITRD=y\n"
"CONFIG_HALT=y\n"
"CONFIG_MESG=y\n"
"CONFIG_FEATURE_SHADOWPASSWDS=y\n"
"CONFIG_USE_BB_SHADOW=y\n"
"CONFIG_USE_BB_PWD_GRP=y\n"
"CONFIG_ADDGROUP=y\n"
"CONFIG_FEATURE_ADDUSER_TO_GROUP=y\n"
"CONFIG_DELGROUP=y\n"
"CONFIG_FEATURE_DEL_USER_FROM_GROUP=y\n"
"# CONFIG_FEATURE_CHECK_NAMES is not set\n"
"CONFIG_ADDUSER=y\n"
"# CONFIG_FEATURE_ADDUSER_LONG_OPTIONS is not set\n"
"CONFIG_DELUSER=y\n"
"CONFIG_GETTY=y\n"
"CONFIG_FEATURE_UTMP=y\n"
"CONFIG_FEATURE_WTMP=y\n"
"CONFIG_LOGIN=y\n"
"# CONFIG_PAM is not set\n"
"CONFIG_LOGIN_SCRIPTS=y\n"
"CONFIG_FEATURE_NOLOGIN=y\n"
"CONFIG_FEATURE_SECURETTY=y\n"
"CONFIG_PASSWD=y\n"
"CONFIG_FEATURE_PASSWD_WEAK_CHECK=y\n"
"CONFIG_CRYPTPW=y\n"
"CONFIG_CHPASSWD=y\n"
"CONFIG_SU=y\n"
"CONFIG_FEATURE_SU_SYSLOG=y\n"
"CONFIG_FEATURE_SU_CHECKS_SHELLS=y\n"
"CONFIG_SULOGIN=y\n"
"CONFIG_VLOCK=y\n"
"CONFIG_CHATTR=y\n"
"CONFIG_FSCK=y\n"
"CONFIG_LSATTR=y\n"
"CONFIG_INSMOD=y\n"
"CONFIG_FEATURE_INSMOD_VERSION_CHECKING=y\n"
"CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS=y\n"
"CONFIG_FEATURE_INSMOD_LOADINKMEM=y\n"
"CONFIG_FEATURE_INSMOD_LOAD_MAP=y\n"
"CONFIG_FEATURE_INSMOD_LOAD_MAP_FULL=y\n"
"CONFIG_RMMOD=y\n"
"CONFIG_LSMOD=y\n"
"CONFIG_FEATURE_LSMOD_PRETTY_2_6_OUTPUT=y\n"
"CONFIG_MODPROBE=y\n"
"CONFIG_FEATURE_MODPROBE_MULTIPLE_OPTIONS=y\n"
"CONFIG_FEATURE_MODPROBE_FANCY_ALIAS=y\n"
"CONFIG_FEATURE_CHECK_TAINTED_MODULE=y\n"
"CONFIG_FEATURE_2_4_MODULES=y\n"
"CONFIG_FEATURE_2_6_MODULES=y\n"
"# CONFIG_FEATURE_QUERY_MODULE_INTERFACE is not set\n"
"CONFIG_DMESG=y\n"
"CONFIG_FEATURE_DMESG_PRETTY=y\n"
"CONFIG_FBSET=y\n"
"CONFIG_FEATURE_FBSET_FANCY=y\n"
"CONFIG_FEATURE_FBSET_READMODE=y\n"
"CONFIG_FDFLUSH=y\n"
"CONFIG_FDFORMAT=y\n"
"CONFIG_FDISK=y\n"
"CONFIG_FDISK_SUPPORT_LARGE_DISKS=y\n"
"CONFIG_FEATURE_FDISK_WRITABLE=y\n"
"# CONFIG_FEATURE_AIX_LABEL is not set\n"
"# CONFIG_FEATURE_SGI_LABEL is not set\n"
"# CONFIG_FEATURE_SUN_LABEL is not set\n"
"# CONFIG_FEATURE_OSF_LABEL is not set\n"
"CONFIG_FEATURE_FDISK_ADVANCED=y\n"
"# CONFIG_FINDFS is not set\n"
"CONFIG_FREERAMDISK=y\n"
"CONFIG_FSCK_MINIX=y\n"
"CONFIG_MKFS_MINIX=y\n"
"CONFIG_FEATURE_MINIX2=y\n"
"CONFIG_GETOPT=y\n"
"CONFIG_HEXDUMP=y\n"
"# CONFIG_FEATURE_HEXDUMP_REVERSE is not set\n"
"# CONFIG_HD is not set\n"
"CONFIG_HWCLOCK=y\n"
"CONFIG_FEATURE_HWCLOCK_LONG_OPTIONS=y\n"
"CONFIG_FEATURE_HWCLOCK_ADJTIME_FHS=y\n"
"CONFIG_IPCRM=y\n"
"CONFIG_IPCS=y\n"
"CONFIG_LOSETUP=y\n"
"CONFIG_MDEV=y\n"
"CONFIG_FEATURE_MDEV_CONF=y\n"
"CONFIG_FEATURE_MDEV_RENAME=y\n"
"CONFIG_FEATURE_MDEV_EXEC=y\n"
"CONFIG_FEATURE_MDEV_LOAD_FIRMWARE=y\n"
"CONFIG_MKSWAP=y\n"
"CONFIG_FEATURE_MKSWAP_V0=y\n"
"CONFIG_MORE=y\n"
"CONFIG_FEATURE_USE_TERMIOS=y\n"
"# CONFIG_VOLUMEID is not set\n"
"# CONFIG_FEATURE_VOLUMEID_EXT is not set\n"
"# CONFIG_FEATURE_VOLUMEID_REISERFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_FAT is not set\n"
"# CONFIG_FEATURE_VOLUMEID_HFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_JFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_XFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_NTFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_ISO9660 is not set\n"
"# CONFIG_FEATURE_VOLUMEID_UDF is not set\n"
"# CONFIG_FEATURE_VOLUMEID_LUKS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_LINUXSWAP is not set\n"
"# CONFIG_FEATURE_VOLUMEID_CRAMFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_ROMFS is not set\n"
"# CONFIG_FEATURE_VOLUMEID_SYSV is not set\n"
"# CONFIG_FEATURE_VOLUMEID_OCFS2 is not set\n"
"# CONFIG_FEATURE_VOLUMEID_LINUXRAID is not set\n"
"CONFIG_MOUNT=y\n"
"CONFIG_FEATURE_MOUNT_FAKE=y\n"
"CONFIG_FEATURE_MOUNT_VERBOSE=y\n"
"# CONFIG_FEATURE_MOUNT_HELPERS is not set\n"
"# CONFIG_FEATURE_MOUNT_LABEL is not set\n"
"CONFIG_FEATURE_MOUNT_NFS=y\n"
"CONFIG_FEATURE_MOUNT_CIFS=y\n"
"CONFIG_FEATURE_MOUNT_FLAGS=y\n"
"CONFIG_FEATURE_MOUNT_FSTAB=y\n"
"CONFIG_PIVOT_ROOT=y\n"
"CONFIG_RDATE=y\n"
"CONFIG_READPROFILE=y\n"
"# CONFIG_RTCWAKE is not set\n"
"CONFIG_SETARCH=y\n"
"CONFIG_SWAPONOFF=y\n"
"CONFIG_SWITCH_ROOT=y\n"
"CONFIG_UMOUNT=y\n"
"CONFIG_FEATURE_UMOUNT_ALL=y\n"
"CONFIG_FEATURE_MOUNT_LOOP=y\n"
"# CONFIG_FEATURE_MTAB_SUPPORT is not set\n"
"CONFIG_ADJTIMEX=y\n"
"# CONFIG_BBCONFIG is not set\n"
"# CONFIG_CHAT is not set\n"
"# CONFIG_FEATURE_CHAT_NOFAIL is not set\n"
"# CONFIG_FEATURE_CHAT_TTY_HIFI is not set\n"
"# CONFIG_FEATURE_CHAT_IMPLICIT_CR is not set\n"
"# CONFIG_FEATURE_CHAT_SWALLOW_OPTS is not set\n"
"# CONFIG_FEATURE_CHAT_SEND_ESCAPES is not set\n"
"# CONFIG_FEATURE_CHAT_VAR_ABORT_LEN is not set\n"
"# CONFIG_FEATURE_CHAT_CLR_ABORT is not set\n"
"CONFIG_CHRT=y\n"
"CONFIG_CROND=y\n"
"# CONFIG_DEBUG_CROND_OPTION is not set\n"
"CONFIG_FEATURE_CROND_CALL_SENDMAIL=y\n"
"CONFIG_CRONTAB=y\n"
"CONFIG_DC=y\n"
"# CONFIG_DEVFSD is not set\n"
"# CONFIG_DEVFSD_MODLOAD is not set\n"
"# CONFIG_DEVFSD_FG_NP is not set\n"
"# CONFIG_DEVFSD_VERBOSE is not set\n"
"# CONFIG_FEATURE_DEVFS is not set\n"
"CONFIG_EJECT=y\n"
"CONFIG_FEATURE_EJECT_SCSI=y\n"
"CONFIG_LAST=y\n"
"CONFIG_LESS=y\n"
"CONFIG_FEATURE_LESS_MAXLINES=9999999\n"
"CONFIG_FEATURE_LESS_BRACKETS=y\n"
"CONFIG_FEATURE_LESS_FLAGS=y\n"
"CONFIG_FEATURE_LESS_FLAGCS=y\n"
"CONFIG_FEATURE_LESS_MARKS=y\n"
"CONFIG_FEATURE_LESS_REGEXP=y\n"
"CONFIG_HDPARM=y\n"
"CONFIG_FEATURE_HDPARM_GET_IDENTITY=y\n"
"CONFIG_FEATURE_HDPARM_HDIO_SCAN_HWIF=y\n"
"CONFIG_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF=y\n"
"CONFIG_FEATURE_HDPARM_HDIO_DRIVE_RESET=y\n"
"CONFIG_FEATURE_HDPARM_HDIO_TRISTATE_HWIF=y\n"
"CONFIG_FEATURE_HDPARM_HDIO_GETSET_DMA=y\n"
"CONFIG_MAKEDEVS=y\n"
"# CONFIG_FEATURE_MAKEDEVS_LEAF is not set\n"
"CONFIG_FEATURE_MAKEDEVS_TABLE=y\n"
"CONFIG_MICROCOM=y\n"
"CONFIG_MOUNTPOINT=y\n"
"CONFIG_MT=y\n"
"CONFIG_RAIDAUTORUN=y\n"
"# CONFIG_READAHEAD is not set\n"
"CONFIG_RUNLEVEL=y\n"
"CONFIG_RX=y\n"
"CONFIG_SCRIPT=y\n"
"CONFIG_STRINGS=y\n"
"CONFIG_SETSID=y\n"
"CONFIG_TASKSET=y\n"
"CONFIG_FEATURE_TASKSET_FANCY=y\n"
"CONFIG_TIME=y\n"
"CONFIG_TTYSIZE=y\n"
"CONFIG_WATCHDOG=y\n"
"CONFIG_FEATURE_IPV6=y\n"
"CONFIG_FEATURE_PREFER_IPV4_ADDRESS=y\n"
"# CONFIG_VERBOSE_RESOLUTION_ERRORS is not set\n"
"CONFIG_ARP=y\n"
"CONFIG_ARPING=y\n"
"CONFIG_BRCTL=y\n"
"CONFIG_FEATURE_BRCTL_FANCY=y\n"
"CONFIG_DNSD=y\n"
"CONFIG_ETHER_WAKE=y\n"
"CONFIG_FAKEIDENTD=y\n"
"CONFIG_FTPGET=y\n"
"CONFIG_FTPPUT=y\n"
"CONFIG_FEATURE_FTPGETPUT_LONG_OPTIONS=y\n"
"CONFIG_HOSTNAME=y\n"
"CONFIG_HTTPD=y\n"
"CONFIG_FEATURE_HTTPD_RANGES=y\n"
"CONFIG_FEATURE_HTTPD_USE_SENDFILE=y\n"
"CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP=y\n"
"CONFIG_FEATURE_HTTPD_SETUID=y\n"
"CONFIG_FEATURE_HTTPD_BASIC_AUTH=y\n"
"CONFIG_FEATURE_HTTPD_AUTH_MD5=y\n"
"CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES=y\n"
"CONFIG_FEATURE_HTTPD_CGI=y\n"
"CONFIG_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR=y\n"
"CONFIG_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV=y\n"
"CONFIG_FEATURE_HTTPD_ENCODE_URL_STR=y\n"
"CONFIG_FEATURE_HTTPD_ERROR_PAGES=y\n"
"CONFIG_FEATURE_HTTPD_PROXY=y\n"
"CONFIG_IFCONFIG=y\n"
"CONFIG_FEATURE_IFCONFIG_STATUS=y\n"
"CONFIG_FEATURE_IFCONFIG_SLIP=y\n"
"CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ=y\n"
"CONFIG_FEATURE_IFCONFIG_HW=y\n"
"CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS=y\n"
"CONFIG_IFENSLAVE=y\n"
"CONFIG_IFUPDOWN=y\n"
"CONFIG_IFUPDOWN_IFSTATE_PATH=\"/var/run/ifstate\"\n"
"CONFIG_FEATURE_IFUPDOWN_IP=y\n"
"CONFIG_FEATURE_IFUPDOWN_IP_BUILTIN=y\n"
"# CONFIG_FEATURE_IFUPDOWN_IFCONFIG_BUILTIN is not set\n"
"CONFIG_FEATURE_IFUPDOWN_IPV4=y\n"
"CONFIG_FEATURE_IFUPDOWN_IPV6=y\n"
"CONFIG_FEATURE_IFUPDOWN_MAPPING=y\n"
"# CONFIG_FEATURE_IFUPDOWN_EXTERNAL_DHCP is not set\n"
"CONFIG_INETD=y\n"
"CONFIG_FEATURE_INETD_SUPPORT_BUILTIN_ECHO=y\n"
"CONFIG_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD=y\n"
"CONFIG_FEATURE_INETD_SUPPORT_BUILTIN_TIME=y\n"
"CONFIG_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME=y\n"
"CONFIG_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN=y\n"
"# CONFIG_FEATURE_INETD_RPC is not set\n"
"CONFIG_IP=y\n"
"CONFIG_FEATURE_IP_ADDRESS=y\n"
"CONFIG_FEATURE_IP_LINK=y\n"
"CONFIG_FEATURE_IP_ROUTE=y\n"
"CONFIG_FEATURE_IP_TUNNEL=y\n"
"CONFIG_FEATURE_IP_RULE=y\n"
"CONFIG_FEATURE_IP_SHORT_FORMS=y\n"
"# CONFIG_FEATURE_IP_RARE_PROTOCOLS is not set\n"
"CONFIG_IPADDR=y\n"
"CONFIG_IPLINK=y\n"
"CONFIG_IPROUTE=y\n"
"CONFIG_IPTUNNEL=y\n"
"CONFIG_IPRULE=y\n"
"CONFIG_IPCALC=y\n"
"CONFIG_FEATURE_IPCALC_FANCY=y\n"
"CONFIG_FEATURE_IPCALC_LONG_OPTIONS=y\n"
"CONFIG_NAMEIF=y\n"
"# CONFIG_FEATURE_NAMEIF_EXTENDED is not set\n"
"CONFIG_NC=y\n"
"CONFIG_NC_SERVER=y\n"
"CONFIG_NC_EXTRA=y\n"
"CONFIG_NETSTAT=y\n"
"CONFIG_FEATURE_NETSTAT_WIDE=y\n"
"CONFIG_NSLOOKUP=y\n"
"CONFIG_PING=y\n"
"CONFIG_PING6=y\n"
"CONFIG_FEATURE_FANCY_PING=y\n"
"CONFIG_PSCAN=y\n"
"CONFIG_ROUTE=y\n"
"CONFIG_SENDMAIL=y\n"
"CONFIG_FETCHMAIL=y\n"
"CONFIG_SLATTACH=y\n"
"CONFIG_TELNET=y\n"
"CONFIG_FEATURE_TELNET_TTYPE=y\n"
"CONFIG_FEATURE_TELNET_AUTOLOGIN=y\n"
"CONFIG_TELNETD=y\n"
"CONFIG_FEATURE_TELNETD_STANDALONE=y\n"
"CONFIG_TFTP=y\n"
"CONFIG_TFTPD=y\n"
"CONFIG_FEATURE_TFTP_GET=y\n"
"CONFIG_FEATURE_TFTP_PUT=y\n"
"CONFIG_FEATURE_TFTP_BLOCKSIZE=y\n"
"# CONFIG_DEBUG_TFTP is not set\n"
"CONFIG_TRACEROUTE=y\n"
"# CONFIG_FEATURE_TRACEROUTE_VERBOSE is not set\n"
"# CONFIG_FEATURE_TRACEROUTE_SOURCE_ROUTE is not set\n"
"# CONFIG_FEATURE_TRACEROUTE_USE_ICMP is not set\n"
"CONFIG_APP_UDHCPD=y\n"
"CONFIG_APP_DHCPRELAY=y\n"
"CONFIG_APP_DUMPLEASES=y\n"
"CONFIG_FEATURE_UDHCPD_WRITE_LEASES_EARLY=y\n"
"CONFIG_DHCPD_LEASES_FILE=\"/var/lib/misc/udhcpd.leases\"\n"
"CONFIG_APP_UDHCPC=y\n"
"CONFIG_FEATURE_UDHCPC_ARPING=y\n"
"# CONFIG_FEATURE_UDHCP_PORT is not set\n"
"# CONFIG_FEATURE_UDHCP_DEBUG is not set\n"
"CONFIG_FEATURE_RFC3397=y\n"
"CONFIG_DHCPC_DEFAULT_SCRIPT=\"/usr/share/udhcpc/default.script\"\n"
"CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS=80\n"
"CONFIG_VCONFIG=y\n"
"CONFIG_WGET=y\n"
"CONFIG_FEATURE_WGET_STATUSBAR=y\n"
"CONFIG_FEATURE_WGET_AUTHENTICATION=y\n"
"CONFIG_FEATURE_WGET_LONG_OPTIONS=y\n"
"CONFIG_ZCIP=y\n"
"CONFIG_TCPSVD=y\n"
"CONFIG_UDPSVD=y\n"
"CONFIG_FREE=y\n"
"CONFIG_FUSER=y\n"
"CONFIG_KILL=y\n"
"CONFIG_KILLALL=y\n"
"CONFIG_KILLALL5=y\n"
"CONFIG_NMETER=y\n"
"CONFIG_PGREP=y\n"
"CONFIG_PIDOF=y\n"
"CONFIG_FEATURE_PIDOF_SINGLE=y\n"
"CONFIG_FEATURE_PIDOF_OMIT=y\n"
"CONFIG_PKILL=y\n"
"CONFIG_PS=y\n"
"CONFIG_FEATURE_PS_WIDE=y\n"
"# CONFIG_FEATURE_PS_TIME is not set\n"
"# CONFIG_FEATURE_PS_UNUSUAL_SYSTEMS is not set\n"
"CONFIG_RENICE=y\n"
"CONFIG_BB_SYSCTL=y\n"
"CONFIG_TOP=y\n"
"CONFIG_FEATURE_TOP_CPU_USAGE_PERCENTAGE=y\n"
"CONFIG_FEATURE_TOP_CPU_GLOBAL_PERCENTS=y\n"
"# CONFIG_FEATURE_TOP_DECIMALS is not set\n"
"CONFIG_FEATURE_TOPMEM=y\n"
"CONFIG_UPTIME=y\n"
"CONFIG_WATCH=y\n"
"CONFIG_FEATURE_SH_IS_ASH=y\n"
"# CONFIG_FEATURE_SH_IS_HUSH is not set\n"
"# CONFIG_FEATURE_SH_IS_MSH is not set\n"
"# CONFIG_FEATURE_SH_IS_NONE is not set\n"
"CONFIG_ASH=y\n"
"CONFIG_ASH_JOB_CONTROL=y\n"
"CONFIG_ASH_READ_NCHARS=y\n"
"CONFIG_ASH_READ_TIMEOUT=y\n"
"# CONFIG_ASH_ALIAS is not set\n"
"CONFIG_ASH_MATH_SUPPORT=y\n"
"CONFIG_ASH_MATH_SUPPORT_64=y\n"
"# CONFIG_ASH_GETOPTS is not set\n"
"CONFIG_ASH_BUILTIN_ECHO=y\n"
"CONFIG_ASH_BUILTIN_TEST=y\n"
"CONFIG_ASH_CMDCMD=y\n"
"# CONFIG_ASH_MAIL is not set\n"
"CONFIG_ASH_OPTIMIZE_FOR_SIZE=y\n"
"CONFIG_ASH_RANDOM_SUPPORT=y\n"
"# CONFIG_ASH_EXPAND_PRMT is not set\n"
"# CONFIG_HUSH is not set\n"
"# CONFIG_HUSH_HELP is not set\n"
"# CONFIG_HUSH_INTERACTIVE is not set\n"
"# CONFIG_HUSH_JOB is not set\n"
"# CONFIG_HUSH_TICK is not set\n"
"# CONFIG_HUSH_IF is not set\n"
"# CONFIG_HUSH_LOOPS is not set\n"
"# CONFIG_LASH is not set\n"
"# CONFIG_MSH is not set\n"
"CONFIG_FEATURE_SH_EXTRA_QUIET=y\n"
"# CONFIG_FEATURE_SH_STANDALONE is not set\n"
"# CONFIG_CTTYHACK is not set\n"
"CONFIG_SYSLOGD=y\n"
"CONFIG_FEATURE_ROTATE_LOGFILE=y\n"
"CONFIG_FEATURE_REMOTE_LOG=y\n"
"# CONFIG_FEATURE_SYSLOGD_DUP is not set\n"
"CONFIG_FEATURE_IPC_SYSLOG=y\n"
"CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE=16\n"
"CONFIG_LOGREAD=y\n"
"CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING=y\n"
"CONFIG_KLOGD=y\n"
"CONFIG_LOGGER=y\n"
"CONFIG_RUNSV=y\n"
"CONFIG_RUNSVDIR=y\n"
"CONFIG_SV=y\n"
"CONFIG_SVLOGD=y\n"
"CONFIG_CHPST=y\n"
"CONFIG_SETUIDGID=y\n"
"CONFIG_ENVUIDGID=y\n"
"CONFIG_ENVDIR=y\n"
"CONFIG_SOFTLIMIT=y\n"
"# CONFIG_CHCON is not set\n"
"# CONFIG_FEATURE_CHCON_LONG_OPTIONS is not set\n"
"# CONFIG_GETENFORCE is not set\n"
"# CONFIG_GETSEBOOL is not set\n"
"# CONFIG_LOAD_POLICY is not set\n"
"# CONFIG_MATCHPATHCON is not set\n"
"# CONFIG_RESTORECON is not set\n"
"# CONFIG_RUNCON is not set\n"
"# CONFIG_FEATURE_RUNCON_LONG_OPTIONS is not set\n"
"# CONFIG_SELINUXENABLED is not set\n"
"# CONFIG_SETENFORCE is not set\n"
"# CONFIG_SETFILES is not set\n"
"# CONFIG_FEATURE_SETFILES_CHECK_OPTION is not set\n"
"# CONFIG_SETSEBOOL is not set\n"
"# CONFIG_SESTATUS is not set\n"
"CONFIG_LPD=y\n"
"CONFIG_LPR=y\n"
"CONFIG_LPQ=y\n"
;
#endif /* _BBCONFIGOPTS_H */
