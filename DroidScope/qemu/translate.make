# this sub-Makefile is included to define a dynamic translating library
#
EMULATOR_OP_LIBRARIES := $(EMULATOR_OP_LIBRARIES) $(LOCAL_MODULE)

# we need to compile this with GCC-3.3 preferabbly
#
LOCAL_NO_DEFAULT_COMPILER_FLAGS := true
LOCAL_CC                        := $(MY_CC)

LOCAL_LDFLAGS += $(my_32bit_ldflags)
LOCAL_CFLAGS += $(my_32bit_cflags) $(OP_CFLAGS)

INTERMEDIATE := $(call intermediates-dir-for,STATIC_LIBRARIES,$(LOCAL_MODULE),true)
OP_OBJ       := $(INTERMEDIATE)/target-arm/op.o

LOCAL_CFLAGS += -I$(INTERMEDIATE)

OP_H     := $(INTERMEDIATE)/op$(OP_SUFFIX).h
OPC_H    := $(INTERMEDIATE)/opc$(OP_SUFFIX).h
GEN_OP_H := $(INTERMEDIATE)/gen-op$(OP_SUFFIX).h

$(OP_H): $(OP_OBJ) $(DYNGEN)
	$(DYNGEN) -o $@ $<

$(OPC_H): $(OP_OBJ) $(DYNGEN)
	$(DYNGEN) -c -o $@ $<

$(GEN_OP_H): $(OP_OBJ) $(DYNGEN)
	$(DYNGEN) -g -o $@ $<

TRANSLATE_SOURCES := target-arm/translate.c \
                     translate-all.c \
                     translate-op.c

LOCAL_SRC_FILES += target-arm/op.c  $(TRANSLATE_SOURCES)

$(TRANSLATE_SOURCES:%.c=$(INTERMEDIATE)/%.o): $(OP_H) $(OPC_H) $(GEN_OP_H)
