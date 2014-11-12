APP_NAME = ext2fs
CLEAN_FILES += ext2fs-itf-*.h

CXXFLAGS += -g

INCLUDES += -I$(OMNIOS_ROOT_DIR)/drivers/include 
INCLUDES += -I$(OMNIOS_ROOT_DIR)/libs/libmd5/include

EXTRA_LIBS=-lsupc++ -lmd5

PREREQUISITES = idl-headers

include ../servers.mk

idl-headers:
	$(IDL_COMPILER) -i ext2fs.idl

client:
	make -C test-client

manifest.bin:
	../../tools/bin/bfc.exe c huff ./manifest.xml ./manifest.bin

copy:	
	cp ./ext2fs	/tftpboot/omnios-dan/

.PHONY: idl-headers manifest.bin clean copy
