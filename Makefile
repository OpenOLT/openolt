# Copyright (C) 2018 Open Networking Foundation
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

########################################################################
##
##
##        Config
##
##
DEVICE = asfvolt16
#
# Three vendor proprietary source files are required to build BAL.
# SW-BCM68620_<VER>.zip - Broadcom BAL source and Maple SDK.
# sdk-all-<SDK_VER>.tar.gz - Broadcom Qumran SDK.
# ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch - Accton/Edgecore's patch.
BAL_MAJOR_VER = 02.06
BAL_VER = 2.6.0.1
SDK_VER = 6.5.7
ACCTON_VER = 201804301043
#
# Version of Open Network Linux (ONL).
ONL_KERN_VER_MAJOR = 3.7
ONL_KERN_VER_MINOR = 10
#
# Build directory
BUILD_DIR = build
#
# GRPC installation
GRPC_ADDR = https://github.com/grpc/grpc
GRPC_DST = /tmp/grpc
GRPC_VER = v1.10.x

USER := $(shell echo $(USER))
#
########################################################################
##
##
##        Install prerequisites
##
##
HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11 -fpermissive -Wno-literal-suffix
LDFLAGS += -L/usr/local/lib `pkg-config --libs grpc++ grpc` -ldl

prereq:
	sudo apt-get -q -y install git pkg-config build-essential autoconf libtool libgflags-dev libgtest-dev clang libc++-dev unzip docker.io
	sudo apt-get install -y build-essential autoconf libssl-dev gawk debhelper dh-systemd init-system-helpers

	# Install GRPC, libprotobuf and protoc
	rm -rf $(GRPC_DST)
	git clone -b $(GRPC_VER) $(GRPC_ADDR) $(GRPC_DST)
	cd $(GRPC_DST) && git submodule update --init
	cd $(GRPC_DST)/third_party/protobuf && ./autogen.sh && ./configure
	make -C $(GRPC_DST)/third_party/protobuf
	sudo make -C $(GRPC_DST)/third_party/protobuf install
	sudo ldconfig
	make -C $(GRPC_DST)
	sudo make -C $(GRPC_DST) install
	sudo ldconfig

docker:
	echo $(USER)
	sudo groupadd -f docker
ifneq "$(USER)" ""
	sudo usermod -aG docker $(USER)
endif

########################################################################
##
##
##        ONL
##
##
ONL_KERN_VER = $(ONL_KERN_VER_MAJOR).$(ONL_KERN_VER_MINOR)
ONL_REPO = $(DEVICE)-onl
ONL_DIR = $(BUILD_DIR)/$(ONL_REPO)
onl:
	if [ ! -d "$(ONL_DIR)/OpenNetworkLinux" ]; then \
		mkdir -p $(ONL_DIR); \
		git clone https://gerrit.opencord.org/$(ONL_REPO) $(ONL_DIR); \
		make -C $(ONL_DIR) $(DEVICE)-$(ONL_KERN_VER_MAJOR); \
	fi;
onl-force:
	make -C $(ONL_DIR) $(DEVICE)-$(ONL_KERN_VER_MAJOR)
distclean-onl:
	sudo rm -rf $(ONL_DIR)

########################################################################
##
##
##        BAL
##
##
BAL_ZIP = SW-BCM68620_$(subst .,_,$(BAL_VER)).zip
SDK_ZIP = sdk-all-$(SDK_VER).tar.gz
ACCTON_PATCH = ACCTON_BAL_$(BAL_VER)-V$(ACCTON_VER).patch
OPENOLT_BAL_PATCH = OPENOLT_BAL_$(BAL_VER).patch
BAL_DIR = $(BUILD_DIR)/$(DEVICE)-bal
ONL_KERNDIR = $(PWD)/$(ONL_DIR)/OpenNetworkLinux/packages/base/amd64/kernels/kernel-$(ONL_KERN_VER_MAJOR)-x86-64-all/builds
MAPLE_KERNDIR = $(BAL_DIR)/bcm68620_release/$(DEVICE)/kernels
BCM_SDK = $(BAL_DIR)/bal_release/3rdparty/bcm-sdk
BALLIBDIR = $(BAL_DIR)/bal_release/build/core/src/apps/bal_api_dist_shared_lib
BALLIBNAME = bal_api_dist
BAL_INC = -I$(BAL_DIR)/bal_release/src/common/os_abstraction \
	-I$(BAL_DIR)/bal_release/src/common/os_abstraction/posix \
	-I$(BAL_DIR)/bal_release/src/common/config \
	-I$(BAL_DIR)/bal_release/src/core/platform \
	-I$(BAL_DIR)/bal_release/src/core/main \
	-I$(BAL_DIR)/bal_release/src/common/include \
	-I$(BAL_DIR)/bal_release/src/lib/libbalapi \
	-I$(BAL_DIR)/bal_release/src/balapiend \
	-I$(BAL_DIR)/bal_release/src/common/dev_log \
	-I$(BAL_DIR)/bal_release/src/common/bal_dist_utils  \
	-I$(BAL_DIR)/bal_release/src/lib/libtopology \
	-I$(BAL_DIR)/bal_release/src/lib/libcmdline \
	-I$(BAL_DIR)/bal_release/src/lib/libutils \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/utils \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/model \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/api \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_reference/cli
CXXFLAGS += $(BAL_INC) -I $(BAL_DIR)/lib/cmdline
CXXFLAGS += -DBCMOS_MSG_QUEUE_DOMAIN_SOCKET -DBCMOS_MSG_QUEUE_UDP_SOCKET -DBCMOS_MEM_CHECK -DBCMOS_SYS_UNITTEST

sdk: onl
ifeq ("$(wildcard $(BAL_DIR))","")
	mkdir $(BAL_DIR)
	unzip download/$(BAL_ZIP) -d $(BAL_DIR)
	cp download/$(SDK_ZIP) $(BCM_SDK)
	chmod -R 744 $(BAL_DIR)
	cat download/$(ACCTON_PATCH) | patch -p1 -d $(BAL_DIR)
	mkdir -p $(MAPLE_KERNDIR)
	ln -s $(ONL_KERNDIR)/linux-$(ONL_KERN_VER) $(MAPLE_KERNDIR)/linux-$(ONL_KERN_VER)
	ln -s $(ONL_DIR)/OpenNetworkLinux/packages/base/any/kernels/archives/linux-$(ONL_KERN_VER).tar.xz $(MAPLE_KERNDIR)/linux-$(ONL_KERN_VER).tar.xz
	ln -s $(ONL_DIR)/OpenNetworkLinux/packages/base/any/kernels/$(ONL_KERN_VER_MAJOR)/configs/x86_64-all/x86_64-all.config $(MAPLE_KERNDIR)/x86_64-all.config
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) maple_sdk_dir
	cat download/$(OPENOLT_BAL_PATCH) | patch -p1 -d $(BAL_DIR)
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) maple_sdk
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) switch_sdk_dir
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) switch_sdk
	KERNDIR=$(ONL_KERNDIR)/linux-$(ONL_KERN_VER) BOARD=$(DEVICE) ARCH=x86_64 SDKBUILD=build_bcm_user make -C $(BCM_SDK)/build-$(DEVICE)/sdk-all-$(SDK_VER)/systems/linux/user/x86-generic_64-2_6
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) bal
	echo 'auto_create_interface_tm=y' >> $(BAL_DIR)/bal_release/3rdparty/maple/cur/$(DEVICE)/board_files/broadcom/bal_config.ini
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) release_board
endif

bal: sdk
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) bal
	echo 'auto_create_interface_tm=y' >> $(BAL_DIR)/bal_release/3rdparty/maple/cur/asfvolt16/board_files/broadcom/bal_config.ini
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) release_board

bal-clean:
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) clean_bal

########################################################################
##
##
##        OpenOLT API
##
##
OPENOLT_PROTOS_DIR = ./protos
OPENOLT_API_LIB = $(OPENOLT_PROTOS_DIR)/libopenoltapi.a
CXXFLAGS += -I./$(OPENOLT_PROTOS_DIR) -I $(OPENOLT_PROTOS_DIR)/googleapis/gens
protos:
	make -C $(OPENOLT_PROTOS_DIR) all
protos-clean:
	-make -C $(OPENOLT_PROTOS_DIR) clean

########################################################################
##
##
##        Main
##
##
SRCS = $(wildcard src/*.cc)
OBJS = $(SRCS:.cc=.o)
DEPS = $(SRCS:.cc=.d)
.DEFAULT_GOAL := all
all: $(BUILD_DIR)/openolt
$(BUILD_DIR)/openolt: sdk protos $(OBJS)
	$(CXX) -pthread -L/usr/local/lib -L$(BALLIBDIR) $(OBJS) $(OPENOLT_API_LIB) /usr/local/lib/libprotobuf.a -o $@ -l$(BALLIBNAME) -lgrpc++ -lgrpc -lpthread -ldl
	ln -sf $(PWD)/$(BAL_DIR)/bcm68620_release/$(DEVICE)/release/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz $(BUILD_DIR)/.
	ln -sf $(PWD)/$(BAL_DIR)/bcm68620_release/$(DEVICE)/release/broadcom/libbal_api_dist.so $(BUILD_DIR)/.
	ln -sf $(PWD)/$(BAL_DIR)/bal_release/build/core/src/apps/bal_core_dist/bal_core_dist $(BUILD_DIR)/.
	ln -sf $(shell ldconfig -p | grep libgrpc.so.6 | tr ' ' '\n' | grep /) $(BUILD_DIR)/libgrpc.so.6
	ln -sf $(shell ldconfig -p | grep libgrpc++.so.1 | tr ' ' '\n' | grep /) $(BUILD_DIR)/libgrpc++.so.1

deb:
	cp $(BUILD_DIR)/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz mkdebian/debian
	cp $(BUILD_DIR)/openolt mkdebian/debian
	cp $(BUILD_DIR)/libgrpc.so.6 mkdebian/debian
	cp $(BUILD_DIR)/libgrpc++.so.1 mkdebian/debian
	cd mkdebian && ./build_$(DEVICE)_deb.sh
	mv *.deb $(BUILD_DIR)/openolt.deb
	make deb-cleanup

src/%.o: %.cpp
	$(CXX) -MMD -c $< -o $@

deb-cleanup:
	rm -f mkdebian/debian/$(DEVICE).debhelper.log
	rm -f mkdebian/debian/$(DEVICE).postinst.debhelper
	rm -f mkdebian/debian/$(DEVICE).postrm.debhelper
	rm -f mkdebian/debian/$(DEVICE).substvars
	rm -rf mkdebian/debian/$(DEVICE)/
	rm -f mkdebian/debian/libgrpc++.so.1
	rm -f mkdebian/debian/libgrpc.so.6
	rm -f mkdebian/debian/openolt
	rm -f mkdebian/debian/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz
	rm -rf mkdebian/debian/tmp/
	rm -f $(DEVICE)_$(BAL_VER)+edgecore-V$(ACCTON_VER)_amd64.changes

clean: protos-clean deb-cleanup
	rm -f $(OBJS) $(DEPS)
	rm -rf protos/googleapis
	rm -f $(BUILD_DIR)/libgrpc.so.6 $(BUILD_DIR)/libgrpc++.so.1
	rm -f $(BUILD_DIR)/libbal_api_dist.so
	rm -f $(BUILD_DIR)/openolt
	rm -f $(BUILD_DIR)/bal_core_dist
	rm -f $(BUILD_DIR)/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz
	rm -f $(BUILD_DIR)/openolt.deb

distclean:
	rm -rf $(BUILD_DIR)

.PHONY: onl sdk bal protos prereq
