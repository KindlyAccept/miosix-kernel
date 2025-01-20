# Copyright (C) 2024 by Skyward
#
# This program is free software; you can redistribute it and/or
# it under the terms of the GNU General Public License as published
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# As a special exception, if other files instantiate templates or use
# macros or inline functions from this file, or you compile this file
# and link it with other works to produce a work based on this file,
# this file does not by itself cause the resulting work to be covered
# by the GNU General Public License. However the source code for this
# file must still be made available in accordance with the GNU
# Public License. This exception does not invalidate any other
# why a work based on this file might be covered by the GNU General
# Public License.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>

include(AddProgramTarget)

# Function to link the Miosix libraries to a target and register the build command
#
#   miosix_link_target(<target>)
#
# What it does:
# - Links the Miosix libraries to the target
# - Tells the linker to generate the map file
# - Registers custom targets to create the hex and bin files (${TARGET}_bin and ${TARGET}_hex)
# - Registers a custom target to flash the program to the board (${TARGET}_program)
function(miosix_link_target TARGET)
    if (NOT TARGET miosix)
        message(FATAL_ERROR "The board you selected is not supported")
    endif()

    # Linker script and linking options are eredited from miosix libraries

    # Link libraries
    target_link_libraries(${TARGET} PRIVATE
        -Wl,--start-group miosix stdc++ c m gcc atomic -Wl,--end-group
    )

    # Tell the linker to produce the map file
    target_link_options(${TARGET} PRIVATE -Wl,-Map,$<TARGET_FILE_DIR:${TARGET}>/$<TARGET_FILE_BASE_NAME:${TARGET}>.map)

    # Add a post build command to create the hex file to flash on the board
    add_custom_command(
        OUTPUT ${TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMENT "Creating ${TARGET}.hex"
        VERBATIM
    )
    add_custom_target(${TARGET}_hex ALL DEPENDS ${TARGET}.hex)
    add_custom_command(
        OUTPUT ${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMENT "Creating ${TARGET}.bin"
        VERBATIM
    )
    add_custom_target(${TARGET}_bin ALL DEPENDS ${TARGET}.bin)

    # Generate custom build command to flash the target
    miosix_add_program_target(${TARGET})
endfunction()
