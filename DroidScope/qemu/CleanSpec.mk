# This empty file is here solely for the purpose of optimizing the Android build
# Please keep it there, and empty, thanks :-)
#

$(call add-clean-step, rm -rf $(OUT_DIR)/host/linux-x86/obj/{SHARED,STATIC}_LIBRARIES/emulator*)
$(call add-clean-step, rm -rf $(OUT_DIR)/host/linux-x86/obj/EXECUTABLES/emulator*)
